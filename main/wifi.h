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
void WiFiEvents(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      // ESP_LOGI(TAG_WIFI, "connected to %s! IP address: " IPSTR, WiFi.SSID().c_str(), IP2STR(WiFi.localIP().u_addr.ip4.addr));
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      ESP_LOGI(TAG_WIFI, "disconnected!");
      break;
    default:
      break;
  }
};

static void wifi_task( void *pvParameters ) {
  ESP_LOGI(TAG_WIFI, "Starting...");

  WiFi.onEvent(WiFiEvents);
  WiFi.mode(WIFI_MODE_STA);
  WiFi.enableLongRange(true);
  WiFi.printDiag(Serial);
  // WiFi.begin(ssid, password);
  wifiMulti.addAP("M3", "123qwe123");
  wifiMulti.addAP("zxlwrt", "123qwe123");
  wifiMulti.run();

  vTaskDelete(NULL);
}

void setup_wifi() {
  xTaskCreate(wifi_task, "wifi_task", 1024 * 10, NULL, 3, NULL);
}
