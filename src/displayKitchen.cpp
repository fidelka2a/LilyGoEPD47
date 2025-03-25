/* 
 *  Vzorovy priklad pouziti LilyGO T5 4.7"
 *  od https://laskakit.cz
 *  
 *  ePaper vykresli logo a 3 vety.
 *  
 *  (@)2021 laskakit.cz
 *  
 *  Vzorovy kod ja zalozen na kodu od LilyGo
 *  https://github.com/Xinyuan-LilyGO/LilyGo-EPD47
 *
 *  Pouzity Online image convertor
 *  https://lvgl.io/tools/imageconverter
 *  Color format: Alpha only 16 shades
 */

#include <Arduino.h>
#include <ArduinoMqttClient.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <Arduino.h>
#include "epd_driver.h"
// #include "pic1.h"
#include "firasans.h"
// #include "karlik.h"
#include <stdio.h>
#include "Calendar.h"
#include "pins.h"

#include "arduino_secrets.h"

int incomingByte = 0;

WiFiMulti WiFiMulti;
WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

const char broker[] = "192.168.1.22";
int        port     = 1883;
const char topic[]  = "test/topic";

Calendar calendar;


void onMqttMessage(int messageSize) {
  // we received a message, print out the topic and contents
  Serial.println("Received a message with topic '");
  Serial.print(mqttClient.messageTopic());
  Serial.print("', length ");
  Serial.print(messageSize);
  Serial.println(" bytes:");

  
  
  String messageContent;

  uint8_t buffer[messageSize];  // Declare as uint8_t array

  // Read bytes into the buffer
  size_t bytesRead = mqttClient.readBytes(buffer, messageSize);  // No cast needed
  Serial.println((char*)buffer);

// If you want to treat it as a string, cast it back to char*
// Serial.println((char*)buffer);
//   // use the Stream interface to print the contents
//   while (mqttClient.available()) {
//     Serial.print((char)mqttClient.read());
//     messageContent += (char)mqttClient.read();
//   }


   if (calendar.addEventsFromJsonArray((char*)buffer)) {
     Serial.println("Events added successfully from MQTT message.");
   } else {
     Serial.println("Failed to parse events from MQTT message.");
   }

  Serial.println();
  delay(300);
}


void setup()
{
    Serial.begin(115200);
    while(!Serial);
      Serial.println("Test");
    // inicializace T5
    epd_init();


  WiFiMulti.addAP(SECRET_SSID, SECRET_PASS);

  while (WiFiMulti.run() != WL_CONNECTED) {
    // failed, retry
    Serial.print(".");
    delay(5000);
  }

  if (!mqttClient.connect(broker, port)) {
    Serial.print("MQTT connection failed! Error code = ");
    Serial.println(mqttClient.connectError());

    while (1);
  }

  mqttClient.onMessage(onMqttMessage);
  mqttClient.subscribe(topic);


    // Rect_t area1 = {
    //     .x = 30,
    //     .y = 10,
    //     .width = pic1_width,
    //     .height =  pic1_height
    // };

    // Zapnuti T5
     epd_poweron();

    // Smazani T5
     epd_clear();

    // Vykresleni loga
    // epd_draw_grayscale_image(area1, (uint8_t *) pic1_data);
    //  delay(200);
    // // //epd_poweroff();

    
    // int32_t cursor_x = 30;
    // int32_t cursor_y = 460;
    // char *string1 = "➸ LilyGo T5 4,7\"\n";
    // // Vykresleni prvni vety
    // writeln((GFXfont *)&demicko, string1, &cursor_x, &cursor_y, NULL);
    // delay(200);
    
    // //epd_poweron();
    int32_t cursor_x2 = 30;
    int32_t cursor_y2 = 260;
    // char *string2 = "➸ snad to jeste funguje...";
    // // Vykresleni prvni vety
    // writeln((GFXfont *)&FiraSans, string2, &cursor_x2, &cursor_y2, NULL);


    // string2 = "maslo";
     int barva = 0;

     char text[30];

     int  y_r = 30;
     cursor_x2 = 0;

    for (int i = 30; i < 431;i = i + 40)    
    {

        FontProperties fontProp = {
        .fg_color = barva,
        .bg_color = 15,
        .fallback_glyph = 0,
        .flags = 0
        };

    
    y_r += i;
    cursor_x2 = 0;

    sprintf(text, "Maslo %d", barva);

    write_mode((GFXfont *)&FiraSans, text, &cursor_x2, &y_r, NULL, BLACK_ON_WHITE, &fontProp);
    delay(200);

    barva = barva +4;
    
    }

    
    // cursor_x = 30;
    // cursor_y += 50;
    // char *string2 = "➸ 16 odstinu sedi\n";
    // // Vykresleni druhe vety
    // writeln((GFXfont *)&FiraSans, string2, &cursor_x, &cursor_y, NULL);
    // delay(200);
    // cursor_x = 30;
    // cursor_y += 50;
    // char *string3 = "➸ Renda rules!!";
    // // Vykresleni treti vety
    // writeln((GFXfont *)&FiraSans, string3, &cursor_x, &cursor_y, NULL);
     delay(200);

    // Vypnuti T5
     epd_poweroff();    
}

void loop()
{
  mqttClient.poll();

  if (Serial.available() > 0) {
    // read the incoming byte:
    String incomingMessage = Serial.readString();

    // say what you got:
    Serial.print("I received: ");
    Serial.println(incomingMessage);

    if (calendar.addEventsFromJsonArray(incomingMessage.c_str())) {
     Serial.println("Events added successfully from MQTT message.");
     calendar.displayEvents();
   } else {
     Serial.println("Failed to parse events from MQTT message.");
   }
   
    // epd_poweron();

    // // Smazani T5
    // epd_clear();
    
    //     int32_t cursor_x1 = 50;
    //     int32_t cursor_y1 = 100;
    // writeln((GFXfont *)&FiraSans, incomingMessage.c_str(), &cursor_x1, &cursor_y1, NULL);
    // delay(200);
    // epd_poweroff();   
  }

  //  Serial.println("zapno, bude to good.2");
   delay(1000);
}

