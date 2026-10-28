/*
 * displayKitchen.cpp – LilyGO T5 4.7" e-paper (ESP32-S3)
 *
 * Rezim: deep sleep 1 hodinu, probuzeni, HTTP GET udalosti, render, spat.
 * Fallback: pokud HTTP selze, ceka 8s na MQTT zpravu.
 */

#include <Arduino.h>
#include <ArduinoMqttClient.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <array>
#include <string>
#include <time.h>
#include <esp_sntp.h>
#include "epd_driver.h"
#include "firasans.h"
#include "pins.h"

#include "apps/CalendarApp.h"
#include "core/AppManager.h"
#include "core/DisplayDriver.h"
#include "core/DisplayInfo.h"
#include "core/Framebuffer.h"
#include "core/RenderContext.h"
#include "core/Services.h"
#include "apps/widgets/ScaledText.h"

#include "arduino_secrets.h"

#ifndef BOARD_HAS_PSRAM
#error "Please enable PSRAM, Arduino IDE -> tools -> PSRAM -> OPI !!!"
#endif

// ── Konfigurace casovani ──────────────────────────────────────────────────────
static constexpr uint64_t kSleepIntervalUs   = 3600ULL * 1000000ULL; // 1 hodina
static constexpr uint32_t kNtpSyncEveryBoots = 24;   // NTP sync 1x za 24 probuzeni (~1x denne)
static constexpr uint32_t kHttpTimeoutMs     = 10000;
static constexpr uint32_t kMqttFallbackMs    = 8000; // jak dlouho cekat na MQTT po HTTP chybe

// ── Sitova konfigurace ────────────────────────────────────────────────────────
static const char  kBroker[]    = "192.168.1.22";
static const int   kMqttPort    = 1883;
static const char  kMqttTopic[] = "test/topic";
static const char* kTimeZone    = "CET-1CEST,M3.5.0,M10.5.0/3";
static const char* kNtpServer1  = "pool.ntp.org";
static const char* kNtpServer2  = "time.nist.gov";

// ── RTC pamet (prezije deep sleep, ztrati se pri vybiti baterie) ─────────────
RTC_DATA_ATTR static uint32_t bootCount     = 0;
RTC_DATA_ATTR static char     lastHash[65]  = {0}; // SHA-256 hex ze serveru
RTC_DATA_ATTR static time_t   rtcStoredTime = 0;
RTC_DATA_ATTR static uint32_t lastRenderDay = 0;  // YYYYMMDD posledniho renderu (klouzavy kalendar)

// ── Platform services ─────────────────────────────────────────────────────────
class EspClockService : public core::ClockService {
  public:
    unsigned long nowMs() const override { return millis(); }
};

class StaticFontService : public core::FontService {
  public:
    const void* getDefaultFont() const override { return &FiraSans; }
};

class NullStorageService : public core::StorageService {
  public:
    bool readConfig(const char* key, char* buf, unsigned int sz) override {
        (void)key; (void)buf; (void)sz;
        return false;
    }
};

class EpdDisplayDriver : public core::DisplayDriver {
  public:
    void begin(const core::DisplayInfo&) override {
        epd_init();
    }

    void present(core::Framebuffer& fb) override {
        auto span = fb.acquireSpan();
        epd_poweron();
        epd_clear(); // odstranovani ghostingu
        epd_draw_grayscale_image(epd_full_screen(), span.data());
        epd_poweroff();
        fb.releaseSpan();
    }
};

// ── Globalni objekty ──────────────────────────────────────────────────────────
EspClockService    clockService;
StaticFontService  fontService;
NullStorageService storageService;

core::DisplayInfo   displayInfo(EPD_WIDTH, EPD_HEIGHT, 4, core::DisplayRotation::Landscape0, 0);
core::Framebuffer   sharedFramebuffer(EPD_WIDTH * EPD_HEIGHT / 2);
core::RenderContext renderContext(displayInfo, &sharedFramebuffer);
core::Services      services{&clockService, &fontService, &storageService};
core::AppManager    appManager(services);
EpdDisplayDriver    displayDriver;
apps::CalendarApp   calendarApp;

WiFiMulti  wifiMulti;
WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

static bool mqttMessageReceived = false;

// ── Forward deklarace ─────────────────────────────────────────────────────────
void onMqttMessage(int messageSize);

// ── Pomocne funkce ────────────────────────────────────────────────────────────


static bool waitForTimeSync(unsigned long timeoutMs = 15000) {
    const unsigned long start = millis();
    time_t now = 0;
    struct tm tm_info;
    while ((millis() - start) < timeoutMs) {
        time(&now);
        localtime_r(&now, &tm_info);
        if (now > 1609459200L) { // > 2021-01-01 = realy cas
            Serial.print("NTP synced: ");
            Serial.println(asctime(&tm_info));
            return true;
        }
        Serial.print(".");
        delay(500);
    }
    Serial.println("\nNTP timeout.");
    return false;
}

static void syncNtp() {
    configTzTime(kTimeZone, kNtpServer1, kNtpServer2);
    Serial.println("Syncing NTP...");
    if (waitForTimeSync()) {
        time(&rtcStoredTime);
    }
}

// Po probuzeni z deep sleep nastavi systemovy cas odhadem z RTC pameti.
// Presnost: +-par sekund (nepresnost uspani), pro kalendar dostacujici.
static void restoreTimeFromRtc() {
    if (rtcStoredTime == 0) {
        return;
    }
    const time_t estimated = rtcStoredTime + (time_t)(kSleepIntervalUs / 1000000ULL);
    struct timeval tv = {estimated, 0};
    settimeofday(&tv, nullptr);
    rtcStoredTime = estimated;
    Serial.print("Time restored (approx): ");
    Serial.println((long)estimated);
}

static uint8_t readBatteryPercent() {
    constexpr float kDividerRatio = 2.0f;   // 100K/100K delic na BATT_PIN
    constexpr float kBattMinV     = 3.0f;
    constexpr float kBattMaxV     = 4.2f;
    constexpr int   kSamples      = 16;

    analogSetAttenuation(ADC_11db); // rozsah 0..3.9V

    uint32_t raw = 0;
    for (int i = 0; i < kSamples; i++) {
        raw += analogRead(BATT_PIN);
        delay(1);
    }
    raw /= kSamples;

    const float measured = (raw / 4095.0f) * 3.3f;
    const float voltage  = measured * kDividerRatio;
    const float clamped  = voltage < kBattMinV ? kBattMinV
                         : voltage > kBattMaxV ? kBattMaxV : voltage;
    const uint8_t pct = static_cast<uint8_t>(
        (clamped - kBattMinV) / (kBattMaxV - kBattMinV) * 100.0f);

    Serial.printf("Baterie: raw=%lu, %.2fV → %d%%\n", raw, voltage, pct);
    return pct;
}

static bool connectWiFi(unsigned long timeoutMs = 20000) {
    const unsigned long start = millis();
    while (wifiMulti.run() != WL_CONNECTED) {
        if ((millis() - start) > timeoutMs) {
            Serial.println("WiFi timeout.");
            return false;
        }
        Serial.print(".");
        delay(500);
    }
    Serial.print("\nWiFi OK: ");
    Serial.println(WiFi.localIP());
    return true;
}

// Vraci JSON pole udalosti pokud se hash zmenil, prazdny string pokud ne.
// outError = true pokud HTTP nebo parse selzal (pro MQTT fallback).
static std::string httpFetchEvents(bool& outError) {
    outError = false;

    HTTPClient http;
    http.begin(SECRET_EVENTS_URL);
    http.setTimeout(kHttpTimeoutMs);
    http.addHeader("Accept", "application/json");

    Serial.print("HTTP GET " SECRET_EVENTS_URL " ... ");
    const int code = http.GET();

    if (code != 200) {
        Serial.print("failed, code=");
        Serial.println(code);
        http.end();
        outError = true;
        return "";
    }

    String body = http.getString();
    http.end();
    Serial.print("OK, ");
    Serial.print(body.length());
    Serial.println(" bytes");

    // Parsovani envelope { "hash": "...", "events": [...] }
    DynamicJsonDocument doc(8448);
    auto err = deserializeJson(doc, body);
    if (err) {
        Serial.print("JSON parse failed: ");
        Serial.println(err.c_str());
        outError = true;
        return "";
    }

    const char* serverHash = doc["hash"] | "";
    if (serverHash[0] == '\0') {
        Serial.println("Chybi hash v odpovedi.");
        outError = true;
        return "";
    }

    if (strncmp(lastHash, serverHash, sizeof(lastHash)) == 0) {
        Serial.println("Hash stejny – udalosti nezmeneny.");
        return ""; // outError zustava false
    }

    // Hash se zmenil – uloz a vrat events pole jako JSON string
    strncpy(lastHash, serverHash, sizeof(lastHash) - 1);
    lastHash[sizeof(lastHash) - 1] = '\0';

    std::string eventsJson;
    serializeJson(doc["events"], eventsJson);
    Serial.print("Novy hash: ");
    Serial.println(serverHash);
    return eventsJson;
}

static void postAndRender(const std::string& json) {
    core::Message msg;
    msg.type        = core::MessageType::NetworkUpdate;
    msg.timestampMs = clockService.nowMs();
    msg.payload     = json;
    appManager.postMessage(msg);
    appManager.process(0, displayDriver, renderContext);
}

static void setPackedPixel4bpp(uint8_t* buffer,
                               int32_t width,
                               int32_t height,
                               int32_t x,
                               int32_t y,
                               uint8_t gray) {
    if (!buffer || x < 0 || y < 0 || x >= width || y >= height) {
        return;
    }

    const int32_t pixelIndex = y * width + x;
    const uint8_t nibble = static_cast<uint8_t>((gray >> 4) & 0x0F);
    uint8_t& packed = buffer[pixelIndex / 2];
    if ((pixelIndex & 1) == 0) {
        packed = static_cast<uint8_t>((packed & 0xF0) | nibble);
    } else {
        packed = static_cast<uint8_t>((packed & 0x0F) | (nibble << 4));
    }
}

static void drawSyncErrorOverlayOnly() {
    // Lokální partial refresh jen v rohu se symbolem chyby, bez smazání kalendáře.
    // Geometrie kopíruje drawSyncErrorSymbol v CalendarApp, aby symbol seděl pixelově přesně.
    constexpr int32_t kBodyW = 32;
    constexpr int32_t kNubW = 4;
    constexpr int32_t kBodyH = 14;
    constexpr int32_t kMargin = 8;
    constexpr int32_t kTop = 10;
    constexpr int32_t kSymW = 3;
    constexpr int32_t kGap = 8;
    constexpr int32_t kPad = 2;
    constexpr uint8_t kBlack = 0x00;

    const int32_t bx = EPD_WIDTH - kMargin - kNubW - kBodyW;
    const int32_t by = kTop;
    const int32_t battTextW = apps::widgets::measureTextWidthHalf(&FiraSans, "100%");
    const int32_t sx = bx - battTextW - kGap - kSymW - 4;

    const int32_t bodyTop = by + 1;
    const int32_t bodyBottom = by + kBodyH - 4;
    const int32_t dotTop = bodyBottom + 2;
    const int32_t dotBottom = dotTop + 1;

    int32_t areaX = sx - kPad;
    int32_t areaY = bodyTop - kPad;
    int32_t areaRight = (sx + kSymW - 1) + kPad;
    int32_t areaBottom = dotBottom + kPad;

    if (areaX < 0) areaX = 0;
    if (areaY < 0) areaY = 0;
    if (areaRight >= EPD_WIDTH) areaRight = EPD_WIDTH - 1;
    if (areaBottom >= EPD_HEIGHT) areaBottom = EPD_HEIGHT - 1;

    const int32_t kAreaX = areaX;
    const int32_t kAreaY = areaY;
    const int32_t kAreaW = areaRight - areaX + 1;
    const int32_t kAreaH = areaBottom - areaY + 1;
    const int32_t localSymX = sx - areaX;
    const int32_t localBodyTop = bodyTop - areaY;
    const int32_t localBodyBottom = bodyBottom - areaY;
    const int32_t localDotTop = dotTop - areaY;

    std::array<uint8_t, (18 * 18 + 1) / 2> overlay{};
    const int32_t bytesNeeded = (kAreaW * kAreaH + 1) / 2;
    if (kAreaW > 18 || kAreaH > 18 || bytesNeeded > static_cast<int32_t>(overlay.size())) {
        Serial.println("Sync error symbol: invalid partial area.");
        return;
    }

    overlay.fill(0xFF);

    for (int32_t y = localBodyTop; y <= localBodyBottom; ++y) {
        for (int32_t x = localSymX; x < localSymX + kSymW; ++x) {
            setPackedPixel4bpp(overlay.data(), kAreaW, kAreaH, x, y, kBlack);
        }
    }

    for (int32_t y = localDotTop; y <= localDotTop + 1; ++y) {
        for (int32_t x = localSymX; x < localSymX + kSymW; ++x) {
            setPackedPixel4bpp(overlay.data(), kAreaW, kAreaH, x, y, kBlack);
        }
    }

    const Rect_t area{kAreaX, kAreaY, kAreaW, kAreaH};
    epd_poweron();
    epd_clear_area(area);
    epd_draw_grayscale_image(area, overlay.data());
    epd_poweroff();
    Serial.println("Sync error symbol: partial refresh only.");
}

static bool tryMqttFallback() {
    Serial.println("HTTP failed – zkousim MQTT fallback...");
    mqttClient.stop();
    mqttClient.setKeepAliveInterval(30 * 1000L);
    mqttClient.setConnectionTimeout(3000);

    if (!mqttClient.connect(kBroker, kMqttPort)) {
        Serial.print("MQTT connect failed: ");
        Serial.println(mqttClient.connectError());
        return false;
    }

    mqttClient.onMessage(onMqttMessage);
    mqttClient.subscribe(kMqttTopic);
    Serial.print("MQTT connected, cekam ");
    Serial.print(kMqttFallbackMs);
    Serial.println("ms na zpravu...");

    mqttMessageReceived = false;
    const unsigned long deadline = millis() + kMqttFallbackMs;
    while (millis() < deadline && !mqttMessageReceived) {
        mqttClient.poll();
        delay(50);
    }
    mqttClient.stop();

    if (mqttMessageReceived) {
        Serial.println("MQTT zprava prijata, renderuji.");
        appManager.process(0, displayDriver, renderContext);
    } else {
        Serial.println("MQTT: zadna zprava.");
    }
    return mqttMessageReceived;
}

// ── MQTT callback ─────────────────────────────────────────────────────────────

void onMqttMessage(int messageSize) {
    Serial.print("MQTT zprava na '");
    Serial.print(mqttClient.messageTopic());
    Serial.print("', ");
    Serial.print(messageSize);
    Serial.println(" bytes");

    std::string payload;
    payload.resize(messageSize > 0 ? static_cast<size_t>(messageSize) : 0);

    int bytesRead = 0;
    const unsigned long startMs = millis();
    while (bytesRead < messageSize) {
        int n = mqttClient.read(
            reinterpret_cast<uint8_t*>(&payload[bytesRead]),
            messageSize - bytesRead);
        if (n > 0) {
            bytesRead += n;
            continue;
        }
        if (millis() - startMs > 2000) {
            break;
        }
        delay(1);
    }

    if (bytesRead != messageSize) {
        Serial.println("Warning: payload zkracen.");
        payload.resize(bytesRead > 0 ? static_cast<size_t>(bytesRead) : 0);
    }

    core::Message msg;
    msg.type        = core::MessageType::NetworkUpdate;
    msg.timestampMs = clockService.nowMs();
    msg.payload     = payload;

    if (!appManager.postMessage(msg)) {
        Serial.println("Fronta plna, zprava zahozena.");
        return;
    }

    mqttMessageReceived = true;
}

// ── Arduino entry points ──────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
#ifdef WAIT_FOR_SERIAL
    // Cekej max 3s na serial monitor – po timeoutu pokracuj bez nej (jen pri ladeni, na baterii zbytecne pali energii)
    { const unsigned long t = millis(); while (!Serial && millis() - t < 3000) { delay(10); } }
#endif

    ++bootCount;
    Serial.print("\n\n=== Boot #");
    Serial.print(bootCount);
    Serial.println(" ===");

    // ── WiFi ─────────────────────────────────────────────────────────────────
    // Po deep sleep/restartu vynutime cisty STA stav, jinak muze zustat stale
    // rozpracovany predchozi pokus o pripojeni a WiFiMulti se nepripoji spolehlive.
    WiFi.persistent(false);
    WiFi.disconnect(true, true);
    delay(100);
    WiFi.mode(WIFI_STA);

    wifiMulti.addAP(SECRET_SSID_HOME,   SECRET_PASS_HOME);
    wifiMulti.addAP(SECRET_SSID_SUMAVA, SECRET_PASS_SUMAVA);
    if (!connectWiFi()) {
        Serial.println("Bez WiFi – spim dal.");
        esp_sleep_enable_timer_wakeup(kSleepIntervalUs);
        esp_deep_sleep_start();
    }

    // ── Cas ──────────────────────────────────────────────────────────────────
    // NTP sync jen pri prvnim bootu, po vybiti baterie (rtcStoredTime == 0),
    // nebo jednou denne (kazdy kNtpSyncEveryBoots-ty boot).
    const bool needNtp = (bootCount == 1)
                      || (rtcStoredTime == 0)
                      || (bootCount % kNtpSyncEveryBoots == 0);
    if (needNtp) {
        syncNtp();
    } else {
        restoreTimeFromRtc();
    }

    // Klouzavy kalendar zavisi na dnesnim dni – pri zmene dne vynutime render i se stejnym hashem.
    uint32_t today = 0;
    {
        const time_t now = time(nullptr);
        const struct tm* info = localtime(&now);
        if (info) {
            today = (info->tm_year + 1900) * 10000 + (info->tm_mon + 1) * 100 + info->tm_mday;
        }
    }
    if (today != lastRenderDay) {
        Serial.println("Novy den – vynucuji prekresleni.");
        lastHash[0] = '\0';
    }

    // ── Baterie – merit pred WiFi zatizenim, po inicializaci ADC ─────────────
    const uint8_t battPct = readBatteryPercent();

    // ── Display + app inicializace ────────────────────────────────────────────
    // sharedFramebuffer je globalni – konstruktor probehne znovu pri kazdem bootu,
    // coz je spravne (PSRAM obsah se po deep sleepu ztrati).
    displayDriver.begin(displayInfo);
    renderContext.font = fontService.getDefaultFont();
    calendarApp.setEvents({});
    calendarApp.setBatteryPercent(battPct);
    appManager.setActiveApp(&calendarApp);

    // ── Primarni zdroj: HTTP ──────────────────────────────────────────────────
    bool httpError = false;
    const std::string json = httpFetchEvents(httpError);

    calendarApp.setSyncError(httpError);

    if (!json.empty()) {
        // Hash se zmenil – render (hash uz byl ulozen uvnitr httpFetchEvents)
        postAndRender(json);
        lastRenderDay = today;
    } else if (httpError) {
        // Server nedostupny – nech predchozi kalendar a dokresli jen chybovy symbol.
        drawSyncErrorOverlayOnly();
    }
    // Prazdny string bez chyby = hash stejny, render preskocen

    // ── Deep sleep ────────────────────────────────────────────────────────────
    Serial.println("Jdu spat na 1 hodinu.");
    Serial.flush();
    esp_sleep_enable_timer_wakeup(kSleepIntervalUs);
    esp_deep_sleep_start();
}

void loop() {
    // Nikdy se nedostaneme sem – deep sleep restartuje od setup().
    delay(10);
}
