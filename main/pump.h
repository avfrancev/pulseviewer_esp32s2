#pragma once
#include <Arduino.h>
#include <cJSON.h>
#include "main.h"

// implement class Pump. It takes pin and isInverted. It must on and off pin state. It has feature: off after time in ms. Class uses events to conroll pump state. Timer uses hw_timer_t from Arduino esp32 library.


#define TAG_PUMP "PUMP"

// class Pump
// {
// public:
//   enum class State
//   {
//     OFF,
//     ON
//   };
//   struct pump_settings_t {
//     int idle_time;
//     float liters_per_minute;
//     int max_off_time_ms;
//   };

//   Pump(int pin, bool isInverted, const char* fileName = nullptr) : pin_(pin), isInverted_(isInverted), fileName_(fileName) {
//   }

//   void on() { setState(State::ON); }
//   void off() { setState(State::OFF); }
//   void setState(State state) {
//     // if (state_ == state)
//     //   return;
//     state_ = state;
//     if (state_ == State::ON)
//       turnOn();
//     else
//       turnOff();
//   }
  
//   Pump::State getState() { return state_; }
//   pump_settings_t getPumpSettings() { return pump_settings_; }

//   void addTime(int time_ms) {
//     // if timer_ started recalculate time
//     if (timer_ != nullptr) {
//       uint64_t t = timerReadMilis(timer_);
//       offTime_ += time_ms - t;
//       // ESP_LOGD(TAG_PUMP, "t %d; offTime %d", t, offTime_);
//       startTimer();
//     } else {
//       offTime_ = time_ms + pump_settings_.idle_time;
//       on();
//     }
//     ESP_LOGD(TAG_PUMP, "NEW Time: %d", offTime_);
//     // offTime_ = time_ms;
//   }
//   void addCapacityLiters(float capacity) {
//     int time_ms = capacity / pump_settings_.liters_per_minute * 60000;
//     ESP_LOGD(TAG_PUMP, "Adding capacity: %f time_ms: %d", capacity, time_ms);
//     addTime(time_ms);
//   }

//   void loadConfig() {
//     cJSON* json = nullptr;
//     JsonConfig::load(fileName_, &json);
//     if (json == nullptr) {
//       ESP_LOGE(TAG_PUMP, "Can't load pump config file");
//       saveConfig();
//       return;
//     }
//     deserializeSettings(json);
//     cJSON_Delete(json);
//   }

//   void saveConfig() {
//     cJSON* json = cJSON_CreateObject();
//     serializeSettings(json);
//     JsonConfig::save(fileName_, json);
//     cJSON_Delete(json);
//   }

//   void printPumpSettings() {
//     ESP_LOGD(TAG_PUMP, "LED_BUILTIN: %d", LED_BUILTIN);
//     ESP_LOGD(TAG_PUMP, "Pin: %d", pin_);
//     ESP_LOGD(TAG_PUMP, "settings: idle_time: %d ms; liters_per_minute: %.3f l/min", pump_settings_.idle_time, pump_settings_.liters_per_minute);
//     ESP_LOGD(TAG_PUMP, "settings: max_off_time_ms: %d ms", pump_settings_.max_off_time_ms);
//   }

//   void setPumpConfig(cJSON* json) {
//     deserializeSettings(json);
//     saveConfig();
//   }

// private:
//   void turnOn() {
//     ESP_LOGD(TAG_PUMP, "Start");
//     digitalWrite(pin_, isInverted_ ? LOW : HIGH);
//     startTimer();
//   }

//   void turnOff() {
//     ESP_EARLY_LOGI(TAG_PUMP, "Stop");
//     digitalWrite(pin_, isInverted_ ? HIGH : LOW);
//     stopTimer();
//     offTime_ = 0;
//   }

//   static void staticTimerCallback(void* arg) {
//     Pump* pump = static_cast<Pump*>(arg);
//     pump->off();
//   }

//   void startTimer() {
//     if (offTime_ == 0)
//       return;
//     stopTimer();
//     // timerSemaphore_ = xSemaphoreCreateBinary();
//     timer_ = timerBegin(1000000);
//     timerAttachInterruptArg(timer_, staticTimerCallback, this);
//     timerAlarm(timer_, offTime_ * 1000, false, 0);
//     ESP_LOGD(TAG_PUMP, "startTimer < %d >", offTime_);
//   }

//   void stopTimer() {
//     if (timer_ != nullptr) {
//       timerEnd(timer_);
//       timer_ = nullptr;
//     }
//   }

//   void serializeSettings(cJSON* json) const {
//     cJSON_AddNumberToObject(json, "idle_time", pump_settings_.idle_time);
//     cJSON_AddNumberToObject(json, "liters_per_minute", pump_settings_.liters_per_minute);
//     cJSON_AddNumberToObject(json, "max_off_time_ms", pump_settings_.max_off_time_ms);
//   }

//   void deserializeSettings(cJSON* json) {
//     pump_settings_.idle_time = JSON_OBJECT_NOT_NULL(json, "idle_time", pump_settings_.idle_time);
//     pump_settings_.liters_per_minute = JSON_OBJECT_NOT_NULL(json, "liters_per_minute", pump_settings_.liters_per_minute);
//     pump_settings_.max_off_time_ms = JSON_OBJECT_NOT_NULL(json, "max_off_time_ms", pump_settings_.max_off_time_ms);
//   }

//   State state_ = State::OFF;
//   int pin_;
//   bool isInverted_;
//   const char* fileName_;
//   int offTime_ = 0;
//   hw_timer_t* timer_ = nullptr;

//   pump_settings_t pump_settings_ = { 5000, 10.0f, 10*60000 };
// };

// Pump* pump = new Pump(LED_BUILTIN, false, "/spiffs/pump_config.json");



// static void pump2_task(void* arg);

#define PUMP_BIT_OFF BIT0
#define PUMP_BIT_ON BIT1
#define PUMP_BITS (PUMP_BIT_OFF | PUMP_BIT_ON)


class Pump {

  struct pump_settings_t {
    int idle_time;
    float liters_per_minute;
    int max_off_time_ms;
  };
  
  static void task(void* arg) {
    Pump* this_ = static_cast<Pump*>(arg);
    EventBits_t uxBits;
    for(;;) {
      uxBits = xEventGroupWaitBits(this_->eventGroup, PUMP_BITS, pdTRUE, pdFALSE, portMAX_DELAY);
      // ESP_LOGD(TAG_PUMP, "Pump: uxBits: %d", (int)uxBits);
      // if (state == State::ON) {
      //   // startTimer(this);
      //   // digitalWrite(pin_, isInverted_ ? LOW : HIGH);
      //   xEventGroupSetBits(eventGroup, PUMP_BIT_ON);
      // } else if (state == State::OFF) {
      //   xEventGroupSetBits(eventGroup, PUMP_BIT_OFF);
      //   // digitalWrite(pin_, isInverted_ ? HIGH : LOW);
      //   // stopTimer(this);
      // }
      if (uxBits & PUMP_BIT_ON) {
        ESP_LOGW(TAG_PUMP, "ON");
        startTimer(this_);
        digitalWrite(this_->pin_, this_->isInverted_ ? LOW : HIGH);
        this_->state = State::ON;
        // this_->setState(State::ON);
      } else if (uxBits & PUMP_BIT_OFF) {
        ESP_LOGW(TAG_PUMP, "OFF");
        digitalWrite(this_->pin_, this_->isInverted_ ? HIGH : LOW);
        stopTimer(this_);
        this_->state = State::OFF;
        // this_->setState(State::OFF);
      }
    }
    vTaskDelete(NULL);
  }

  static void startTimer(void* arg) {
    Pump *this_ = static_cast<Pump*>(arg);
    if (this_->offTime == 0)
      return;
    stopTimer(this_);
    // timerSemaphore_ = xSemaphoreCreateBinary();
    this_->timer = timerBegin(1000000);
    // timerAttachInterruptArg(this_->timer, staticTimerCallback, this_->eventGroup);
    timerAttachInterruptArg(this_->timer, [](void* arg) {
      EventGroupHandle_t eventGroup = static_cast<EventGroupHandle_t>(arg);
      xEventGroupSetBits(eventGroup, PUMP_BIT_OFF);
    }, this_->eventGroup);
    timerAlarm(this_->timer, this_->offTime * 1000, false, 0);
    // timerAlarm(this_->timer, this_->offTime, false, 0);
    ESP_LOGD(TAG_PUMP, "startTimer < %d >", this_->offTime);
  }  

  static void stopTimer(void* arg) {
    // ESP_LOGD(TAG_PUMP, "static void stopTimer");
    Pump *this_ = static_cast<Pump*>(arg);
    if (this_->timer != nullptr) {
      timerEnd(this_->timer);
      this_->timer = nullptr;
    }
  }
public:
  Pump(int pin, bool isInverted, const char* fileName = nullptr) : pin_(pin), isInverted_(isInverted), fileName_(fileName) {}
  enum class State {
    OFF, ON
  };
  State state = State::OFF;
  EventGroupHandle_t eventGroup;
  int offTime = 0;
  hw_timer_t* timer = nullptr;

  void init() {
    loadConfig();
    printPumpSettings();
    pinMode(pin_, OUTPUT);
    eventGroup = xEventGroupCreateStatic(&eventGroupBuffer_);
    xTaskCreate(task, "pump_task", 1024 * 2, this, 3, NULL);
    setState(State::OFF);
  }
  
  void start() {
    offTime = pump_settings_.max_off_time_ms;
    setState(State::ON);
  }
  void startByTime(int time_ms) {
    offTime = time_ms;
    setState(State::ON);
  }
  void startByLiters(float liters) {
    int time_ms = liters / pump_settings_.liters_per_minute * 60000;
    offTime = time_ms;
    ESP_LOGD(TAG_PUMP, "startByLiters: %f time_ms: %d", liters, time_ms);
    setState(State::ON);
  }
  void addTime(int time_ms) {
    if (timer != nullptr) {
      uint64_t t = timerReadMilis(timer);
      offTime += time_ms - t;
      // timerWrite(timer, offTime * 1000000);
      if (offTime < 0) {
        stop();
        return;
      }
      startTimer(this);
      // ESP_LOGD(TAG_PUMP, "NEW Time: %d", offTime);
    } else {
      ESP_LOGE(TAG_PUMP, "Timer not started!");
    } 
  }  
  void addCapacityLiters(float liters) {
    int time_ms = liters / pump_settings_.liters_per_minute * 60000;
    ESP_LOGD(TAG_PUMP, "addCapacityLiters: %f time_ms: %d", liters, time_ms);
    addTime(time_ms);
  }
  void stop() {
    setState(State::OFF);
  }

  int getPin() {
    return pin_;
  }
  bool isOn() {
    return state == State::ON;
  }
  void setState(State _state) {
    ESP_LOGD(TAG_PUMP, "setState: %d", (int)_state);
    xEventGroupSetBits(eventGroup, _state == State::ON ? PUMP_BIT_ON : PUMP_BIT_OFF);
  }
  /////////////////////////////////////////////////
  void loadConfig() {
    cJSON* json = nullptr;
    JsonConfig::load(fileName_, &json);
    if (json == nullptr) {
      ESP_LOGE(TAG_PUMP, "Can't load pump config file");
      saveConfig();
      return;
    }
    deserializeSettings(json);
    cJSON_Delete(json);
  }

  void saveConfig() {
    cJSON* json = cJSON_CreateObject();
    serializeSettings(json);
    JsonConfig::save(fileName_, json);
    cJSON_Delete(json);
  }

  void printPumpSettings() {
    ESP_LOGD(TAG_PUMP, "LED_BUILTIN: %d", LED_BUILTIN);
    ESP_LOGD(TAG_PUMP, "Pin: %d", pin_);
    ESP_LOGD(TAG_PUMP, "settings: idle_time: %d ms; liters_per_minute: %.3f l/min", pump_settings_.idle_time, pump_settings_.liters_per_minute);
    ESP_LOGD(TAG_PUMP, "settings: max_off_time_ms: %d ms", pump_settings_.max_off_time_ms);
  }

  void setPumpConfig(cJSON* json) {
    deserializeSettings(json);
    saveConfig();
  }
  void serializeSettings(cJSON* json) const {
    cJSON_AddNumberToObject(json, "idle_time", pump_settings_.idle_time);
    cJSON_AddNumberToObject(json, "liters_per_minute", pump_settings_.liters_per_minute);
    cJSON_AddNumberToObject(json, "max_off_time_ms", pump_settings_.max_off_time_ms);
  }

  void deserializeSettings(cJSON* json) {
    pump_settings_.idle_time = JSON_OBJECT_NOT_NULL(json, "idle_time", pump_settings_.idle_time);
    pump_settings_.liters_per_minute = JSON_OBJECT_NOT_NULL(json, "liters_per_minute", pump_settings_.liters_per_minute);
    pump_settings_.max_off_time_ms = JSON_OBJECT_NOT_NULL(json, "max_off_time_ms", pump_settings_.max_off_time_ms);
  }
  pump_settings_t getPumpSettings() {
    return pump_settings_;
  }
  int pin_;
  bool isInverted_;
  const char* fileName_;
private:

  StaticEventGroup_t eventGroupBuffer_;
  pump_settings_t pump_settings_ = { 1000, 200.0f, 10*60000 };
};

Pump* pump = new Pump(LED_BUILTIN, false, "/spiffs/pump_config.json");

