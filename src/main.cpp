#include <Arduino.h>
#include "WebSocketServer.h"
#include "StateWriter.h"
#include "credentials.h"

#define LED_BUILTIN 2

StateWriter stateWriter;
WebSocketServer wss(80, stateWriter);

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("======= ESP32 Starting Up =======");
    
    Serial.println("Establishing WiFi connection ->");
    wss.beginWiFi(WIFI_SSID, WIFI_PASSWORD);

    Serial.println("Beginning state writer");
    stateWriter.beginSubscriptions();
    
    Serial.println("Starting socket server ->");
    wss.startServer(); 
    
    pinMode(LED_BUILTIN, OUTPUT);
    Serial.println("Setup complete :)");
    Serial.println("=========================");
}

void loop() {
    wss.checkWiFiStatus();
    digitalWrite(LED_BUILTIN, HIGH);
    delay(2000);
    digitalWrite(LED_BUILTIN, LOW);
    delay(2000);
}