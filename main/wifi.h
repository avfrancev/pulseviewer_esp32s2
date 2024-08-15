#pragma once

#include <WiFi.h>
#include <WiFiMulti.h>
#include "esp_log.h"

// #include "typedefs.h"

const char *ssid = "zxlwrt"; // Change this to your WiFi SSID
// const char *ssid = "BEE_465CM"; // Change this to your WiFi SSID
const char *password = "123qwe123"; // Change this to your WiFi password
WiFiMulti wifiMulti;

// WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
//   if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
//     Serial.print(">>>> WiFi connected! IP address: ");
//     Serial.println(WiFi.localIP());
//   }
// });
const char *TAG_WIFI = "WIFI";

void setup_wifi();

void WiFiEvents(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      ESP_LOGW(TAG_WIFI, "Connected!");
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      ESP_LOGW(TAG_WIFI, "Disconnected!");
      WiFi.removeEvent(WiFiEvents);
      setup_wifi();
      break;
    default:
      break;
  }
};

static void wifi_task( void *pvParameters ) {
  ESP_LOGI(TAG_WIFI, "Starting...");
  
  WiFi.setAutoReconnect(false);
  WiFi.mode(WIFI_MODE_STA);
  WiFi.useStaticBuffers(true);
  WiFi.disconnect();
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  wifiMulti.addAP("M3", "123qwe123");
  wifiMulti.addAP("zxlwrt", "123qwe123");
  wifiMulti.addAP("BEE_465CM", "123qwe123");
  wifiMulti.addAP("BEE_465C", "123qwe123");

  wifiMulti.run();
  WiFi.onEvent(WiFiEvents);

  vTaskDelete(NULL);
}

void setup_wifi() {
  xTaskCreate(wifi_task, "wifi_task", 1024 * 10, NULL, 3, NULL);
}
