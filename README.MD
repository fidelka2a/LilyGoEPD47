# LilyGO T-Display-S3 Calendar (MQTT → ePaper)

Projekt zobrazuje nadcházející události a měsíční kalendář na ePaper displeji (LilyGO T-Display-S3 + EPD driver). Data přichází jako JSON přes MQTT.

## Co to umí

- Připojí se na Wi‑Fi, synchronizuje čas přes NTP (SNTP).
- Připojí se na MQTT broker a odebírá zprávy z jednoho topicu.
- Po přijetí nové zprávy:
  - naparsuje události z JSON payloadu,
  - seřadí je podle `start`,
  - vlevo vykreslí seznam „upcoming“ událostí (aktuálně max. 4),
- vpravo vykreslí kalendář aktuálního měsíce a v polích dnů zobrazí počet událostí.
- Překreslení ePaperu probíhá pouze při změně (při příjmu zprávy), ne v každé iteraci `loop()`.

## Hardware

- Board: `lilygo-t-display-s3`
- Displej: ePaper (EPD driver, rozměry `EPD_WIDTH` × `EPD_HEIGHT`)
- PSRAM: vyžadována (`BOARD_HAS_PSRAM`)

## Build / Upload (PlatformIO)

Konfigurace je v `platformio.ini` (env `lilygo-t-display-s3`).

Typické příkazy:

```bash
pio run
pio run -t upload
pio device monitor
```

Poznámky k „záchrannému“ flashnutí jsou v `README.TXT`.

## Konfigurace

### Wi‑Fi

Přihlašovací údaje se berou z `arduino_secrets.h` (není součástí repa):

```cpp
#define SECRET_SSID "..."
#define SECRET_PASS "..."
```

### MQTT

Nastavení je přímo v `src/displayKitchen.cpp`:

- `broker`: IP/host brokera
- `port`: typicky `1883`
- `topic`: topic, ze kterého se čte JSON

Implementovaná je i automatická obnova spojení (`ensureMqttConnected()`), aby zařízení po výpadku dál přijímalo zprávy.

### Čas (NTP)

Čas se synchronizuje v `setup()` přes:

- `configTzTime(kTimeZone, kNtpServer1, kNtpServer2)`

Timezone string je nastaven na CET/CEST.

## Formát vstupního JSON

MQTT payload musí být **JSON pole** (`[...]`) objektů událostí. Každá událost je objekt se standardními klíči:

- `summary` *(string, povinné)* – název události
- `start` *(string, povinné)* – začátek
- `end` *(string, povinné)* – konec
- `location` *(string, volitelné)* – místo

Událost je považována za validní pouze pokud má `summary`, `start` a `end`.

### Podporované formáty `start`/`end`

- Časová událost: ISO‑8601 `YYYY-MM-DDTHH:MM:SS` (čas se zobrazuje jako `HH:mm`)
- Celodenní událost: `YYYY-MM-DD` (bez času)

> UI zobrazuje data/čas vždy jako `DD.MM.YYYY` případně `DD.MM.YYYY HH:mm`.

### Jak se události zobrazují (UpcomingEvents)

Pro každou událost:

1. vždy `summary`
2. pak datum/čas podle pravidel:
   - celodenní a stejný den: `DD.MM.YYYY`
   - časová a stejný den: `DD.MM.YYYY HH:mm - HH:mm`
   - jinak: řádky `od:` a `do:` ve formátu `DD.MM.YYYY` nebo `DD.MM.YYYY HH:mm`
3. nakonec `location` (pokud existuje)

## Značení událostí v měsíčním kalendáři (MonthFrame)

Kalendář vpravo zobrazuje každý den jako políčko v tabulce a používá šrafování pro rychlé rozlišení typů událostí:

- **Diagonální šrafování** se kreslí pouze v případě, že v daném dni existuje alespoň jedna **časová událost**, která **začíná i končí ve stejný den** (tj. `start`/`end` obsahují čas a datum je stejné).
- **Vodorovné šrafování** se kreslí v případě, že v daném dni existuje alespoň jedna **celodenní událost** (`YYYY-MM-DD`) **nebo** jakákoliv **vícedenní událost** (pro kalendář se bere jako „celodenní“), a to přes **celé políčko**.
- Pokud je v jednom dni kombinace obou typů, nejdřív se vykreslí diagonální šrafování a **přes něj** vodorovné.

### Vícedenní rozsahy (`end` není včetně)

U vícedenních událostí je v kalendáři rozsah dní interpretován jako **[start, end)**:

- `start: 2025-12-24`, `end: 2025-12-26` vyplní políčka **24. a 25.**, ale **ne 26.**

### Číslice s počtem událostí

- Číslice v políčku zobrazuje počet událostí přiřazených k danému dni.
- Pro **vícedenní události** se číslice započítá **pouze v 1. dni akce** (v dalších dnech je pouze šrafování).

### Tloušťka vodorovných čar při souběhu

- Pokud je v jednom dni více „celodenních“ událostí, vodorovné čáry se zesílí.
- Tloušťka je úměrná počtu celodenních událostí v daném dni (aktuálně `3 × počet`).

### Příklad: časová událost (stejný den)

```json
[
  {
    "summary": "Oběd s klientem",
    "start": "2025-12-23T12:00:00",
    "end": "2025-12-23T13:00:00",
    "location": "Restaurace U Mostu"
  }
]
```

### Příklad: celodenní událost (stejný den)

```json
[
  {
    "summary": "Dovolená",
    "start": "2025-12-24",
    "end": "2025-12-24",
    "location": "domov"
  }
]
```

### Příklad: vícedenní událost (bez času)

```json
[
  {
    "summary": "Vánoční období",
    "start": "2025-12-24",
    "end": "2025-12-26",
    "location": "lesák"
  }
]
```

### Kódování textu (diakritika)

EPD font renderer vyžaduje validní UTF‑8. Aby zařízení nepadalo na nevalidních bajtech z MQTT, `summary` a `location` se sanitizují funkcí `apps::ensureSafeUtf8ForFont()` (`apps/Utf8.h`, `apps/Utf8.cpp`):

- pokud text není validní UTF‑8, bere se jako CP1250 a převede se do UTF‑8,
- znaky mimo rozsah aktuálního fontu se zjednoduší (např. `ř` → `r`) nebo nahradí `?`.

## Architektura (stručně)

- `src/displayKitchen.cpp`: main (Wi‑Fi, NTP, MQTT, loop, display driver)
- `core/AppManager.h`: fronta zpráv + řízení překreslení
- `apps/CalendarApp.*`: parse JSON → `events_` → render + calendar aggregation
- `apps/widgets/UpcomingEvents.*`: levý seznam událostí
- `apps/widgets/MonthFrame.*`: pravý měsíční kalendář + počty událostí

Diagramy:

- `uml_class_1_0.md` – class diagram (PlantUML)
- `uml_seq_1_0.md` – sekvenční diagram (PlantUML)

## Troubleshooting

### „Zařízení po první zprávě přestane reagovat na MQTT“

Typická příčina je nedočitění celého payloadu (zůstanou bajty v bufferu klienta a rozbije se parser). V callbacku se proto payload čte po blocích přes `mqttClient.read(buf, len)` až do délky `messageSize`, s krátkým timeoutem.

### Ghosting / blikání ePaperu

Řidič displeje používá `epd_clear()` před `epd_draw_grayscale_image()`, což redukuje „duchy“, ale způsobí bliknutí (typické pro ePaper).

### Národní znaky (Čeština)

Display se zasekne pokud obdrží nějaké znaky. Není obajsněno jaké znaky to jsou, nejspíše ř, š, ě, í, é, atd.. Pomůže restart displaye.

### Upload nejde zopakovat

Viz `README.TXT` (esptool příkaz a postup pro bootloader režim).
