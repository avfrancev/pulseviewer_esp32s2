#include "Arduino.h"
#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "WiFi.h"
#include "esp_log.h"
#include <dirent.h> 

#define LED_BUILTIN 15
#include "esp_spiffs.h"
#include "main.h"
#include "pump.h"
#include "radio.h"
#include "HCS301.h"

const char *TAG = "MAIN";

HCS301* hcs301 = new HCS301(0x001C4A01);

void setup_SPIFFS() {
  esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = NULL,
      .max_files = 5,
      .format_if_mount_failed = false
  };
  esp_err_t ret = esp_vfs_spiffs_register(&conf);
  if (ret != ESP_OK) {
      if (ret == ESP_FAIL) {
          ESP_LOGE(TAG, "Failed to mount or format filesystem");
      } else if (ret == ESP_ERR_NOT_FOUND) {
          ESP_LOGE(TAG, "Failed to find SPIFFS partition");
      } else {
          ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
      }
    return;
  }

  size_t total = 0, used = 0;
  ret = esp_spiffs_info(NULL, &total, &used);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
  } else {
    ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    
    DIR* dir = opendir("/spiffs");
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        ESP_LOGI(TAG, "File: %s", entry->d_name);
    }
    closedir(dir);
  }
}


extern "C" void app_main()
{
  vTaskDelay(500 / portTICK_PERIOD_MS);
  initArduino();

  esp_log_level_set("*", ESP_LOG_INFO);
  esp_log_level_set("gpio", ESP_LOG_WARN);
  esp_log_level_set(TAG, ESP_LOG_DEBUG);
  // esp_log_level_set("PUMP", ESP_LOG_DEBUG);

  ESP_LOGW(TAG, "Starting...");
  
  pinMode(LED_BUILTIN, OUTPUT);
  for (size_t i = 0; i < 21; i++)
  {
    digitalWrite(LED_BUILTIN, i%2);
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }

  setup_SPIFFS();

  // if (!setup_CC1101()) {
  //   ESP_LOGE("TAG#1", "Failed to initialize CC1101");
  // } else {
  //   ESP_LOGI("CC1101", "OK");

  // }
  setup_wifi();
  start_webserver();

  initRadio();

  pump->init();
  // pump->loadConfig();
  
  // pump->printPumpSettings();

  hcs301->set_on_buttons_press([](EventBits_t button) {
    ESP_LOGD(TAG, "HCS301: Pressed button: %d", (int)button);
    switch (button) {
      case 15:
        ESP_LOGE(TAG, "Restarting...");
        vTaskDelay(1000);
        ESP.restart();
        break;
      case 4:
        // ESP_LOGD(TAG, "GET STATE: %d", (int)pump->state);
        pump->stop();
        break;
      case 2:
        if (pump->isOn()) {
          pump->addCapacityLiters(5);
          break;
        }
        pump->startByLiters(5*4);
        // pump->addTime(pump->getPumpSettings().max_off_time_ms);
        break;
      case 9:
        // if (pump->getState() == Pump::State::OFF) {
        //   pump->addCapacityLiters(5*1);
        // } else {
        //   pump->addCapacityLiters(5);
        // }
        break;
    }
  });
  
  // vTaskDelay(5000 / portTICK_PERIOD_MS);

}