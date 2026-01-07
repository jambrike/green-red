#include <WiFi.h> // Changed from ESP8266WiFi.h to WiFi.h

void setup(){
  Serial.begin(115200);

  Serial.print("Hello world!");
  
  // Set the device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);
  
  // Give the hardware a moment to initialize
  delay(5000); 

  Serial.println("");
  Serial.print("ESP32 MAC Address: ");
  Serial.println(WiFi.macAddress());
}

void loop(){
  // Nothing to do here
}