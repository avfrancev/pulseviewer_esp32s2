#pragma once
#include <Arduino.h>
#include <cJSON.h>
#include "main.h"

#define TAG_PUMP "PUMP"

#define PUMP_BIT_OFF BIT0
#define PUMP_BIT_ON BIT1
#define PUMP_BITS (PUMP_BIT_OFF | PUMP_BIT_ON)


class Pump {
  struct pump_settings_t {
    int idle_time;
    float liters_per_minute;
    int max_off_time_ms;
  };
  
  /**
   * @brief Task function for the Pump class.
   *
   * This function waits for bits in the event group to be set and then
   * performs the appropriate action based on the set bits. If the
   * PUMP_BIT_ON bit is set, the pump is turned on and a timer is started.
   * If the PUMP_BIT_OFF bit is set, the pump is turned off and the timer
   * is stopped.
   *
   * @param arg A pointer to the Pump object.
   */
  static void task(void* arg) {
    Pump* this_ = static_cast<Pump*>(arg);
    EventBits_t uxBits;
    for(;;) {
      uxBits = xEventGroupWaitBits(this_->eventGroup, PUMP_BITS, pdTRUE, pdFALSE, portMAX_DELAY);
      if (uxBits & PUMP_BIT_ON) {
        ESP_LOGW(TAG_PUMP, "ON");
        startTimer(this_);
        digitalWrite(this_->pin_, this_->isInverted_ ? LOW : HIGH);
        this_->state = State::ON;
      } else if (uxBits & PUMP_BIT_OFF) {
        ESP_LOGW(TAG_PUMP, "OFF");
        digitalWrite(this_->pin_, this_->isInverted_ ? HIGH : LOW);
        stopTimer(this_);
        this_->state = State::OFF;
      }
    }
    vTaskDelete(NULL);
  }

  /**
   * @brief Starts the timer for the pump.
   *
   * This function starts the timer for the pump. If the offTime is 0, then
   * the function returns immediately. Otherwise, it stops the current timer
   * (if it is running) and starts a new timer with period equal to the offTime
   * in milliseconds. When the timer expires, it sets the PUMP_BIT_OFF bit in
   * the event group, which triggers the pump to turn off.
   *
   * @param arg A pointer to the Pump object.
   */
  static void startTimer(void* arg) {
    Pump *this_ = static_cast<Pump*>(arg);
    if (this_->offTime == 0)
      return;
    stopTimer(this_);
    this_->timer = timerBegin(1000000);
    timerAttachInterruptArg(this_->timer, [](void* arg) {
      EventGroupHandle_t eventGroup = static_cast<EventGroupHandle_t>(arg);
      xEventGroupSetBits(eventGroup, PUMP_BIT_OFF);
    }, this_->eventGroup);
    timerAlarm(this_->timer, this_->offTime * 1000, false, 0);
    ESP_LOGD(TAG_PUMP, "startTimer < %d >", this_->offTime);
  }  

  /**
   * @brief Stops the timer for the pump.
   *
   * This function stops the timer for the pump. If the timer is running,
   * it is stopped and the timer handle is set to null.
   *
   * @param arg A pointer to the Pump object.
   */
  static void stopTimer(void* arg) {
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
  /**
   * @brief Starts the pump by the given number of liters.
   *
   * This function calculates the time in milliseconds that the pump needs to run
   * to deliver the given number of liters based on the pump settings. The time
   * is then set as the offTime for the pump. The function then sets the state of
   * the pump to ON.
   *
   * @param liters The number of liters to deliver.
   */
  void startByLiters(float liters) {
    int time_ms = liters / pump_settings_.liters_per_minute * 60000;
    offTime = time_ms;
    ESP_LOGD(TAG_PUMP, "startByLiters: %f time_ms: %d", liters, time_ms);
    setState(State::ON);
  }
  /**
   * @brief Adds time to the timer.
   *
   * This function adds the given time to the timer. If the timer is not started,
   * then a log error is printed. If the resulting offTime is negative, then the 
   * pump is stopped.
   *
   * @param time_ms The time to add, in milliseconds.
   */
  void addTime(int time_ms) {
    if (timer != nullptr) {
      uint64_t t = timerReadMilis(timer);
      offTime += time_ms - t;
      if (offTime < 0) {
        stop();
        return;
      }
      startTimer(this);
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

