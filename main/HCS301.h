#pragma once

#include "main.h"
#include "decoders.h"

#define HCS_BTNS_EVENTS_BITS BIT0 | BIT1 | BIT2 | BIT3

inline uint8_t reverse8(uint8_t b)
{
  b = (b & 0b11110000) >> 4 | (b & 0b00001111) << 4;
  b = (b & 0b11001100) >> 2 | (b & 0b00110011) << 2;
  b = (b & 0b10101010) >> 1 | (b & 0b01010101) << 1;
  return b;
}



struct __attribute__((packed)) HCS301_t
{
  uint16_t preamble : 12;
  uint32_t encrypted : 32;
  uint32_t serial : 28;
  uint8_t buttons : 4;
  uint8_t vlow : 1;
  uint8_t fixed : 1;

  HCS301_t() : preamble(0), encrypted(0), serial(0), buttons(0), vlow(0), fixed(0) {}

  // HCS301_t(const uint8_t *d)
  // {
  //   this->update(d);
  // }

  void update(const uint8_t *d)
  {
    this->preamble = ((uint16_t)d[0] << 4) | ((d[1] >> 4) & 0x0F);                                                                                                                                       // 12 bits
    this->encrypted = ((uint32_t)reverse8(d[1] & 0x0F) << 28) | ((uint32_t)reverse8(d[2]) << 20) | ((uint32_t)reverse8(d[3]) << 12) | ((uint32_t)reverse8(d[4]) << 4) | ((uint32_t)reverse8(d[5]) >> 4); // 32 bits
    this->serial = ((uint32_t)reverse8(d[5] & 0x0F) << 24) | ((uint32_t)reverse8(d[6]) << 16) | ((uint32_t)reverse8(d[7]) << 8) | reverse8(d[8]);                                                        // 28 bits
    this->buttons = (d[9] >> 4) & 0xf;
    this->vlow = (d[9] >> 3) & 0x1;
    this->fixed = (d[9] >> 2) & 0x1;
    // Serial.printf("fixed: %d vlow: %d buttons: %d serial: %08X encrypted: %08X preamble: %04X\n", fixed, vlow, buttons, serial, encrypted, preamble);
  }

  bool is_valid() const { return preamble == 0xfff; }

  // bool is_serial(uint32_t serial) const { return this->serial == serial; }

  // explicit operator bool() const { return crc_ok; }
};


class HCS301 : public PWMDecoder {
public:
  EventGroupHandle_t eventGroup;

  HCS301(uint32_t serial = 0) {
    serial_ = serial;
    data_ = HCS301_t();
    eventGroup = xEventGroupCreateStatic(&eventGroupBuffer_);
    xTaskCreate(task_event_handler, "HCS301 event handler", 4*1024, this, 3, NULL);
  }

  void set_on_buttons_press(std::function<void(EventBits_t)> cb) {
    on_buttons_press_ = cb;
  }

  void decode_pwm(pwm_message_t *pwm_msg, rmt_message_t *rmt_msg) override {
    if (rmt_msg->length != 78) {
      return;
    }
    data_.update(pwm_msg->buf);
    // Serial.printf("HCS301: data_.serial = %08X; serial_ = %08X\n", data_.serial, serial_);
    if (data_.is_valid() && data_.serial == serial_ && data_.encrypted != last_encripted_) {
      // Serial.printf("HCS301: PWM decoder get %d bytes with rssi %d\n", pwm_msg->length, rmt_msg->rssi);
      xEventGroupSetBits(eventGroup, data_.buttons);
      last_encripted_ = data_.encrypted;
    }
  }

  static void task_event_handler(void *args) {
    HCS301 *this_ = (HCS301 *)args;
    EventBits_t uxBits;
    for(;;) {
      uxBits = xEventGroupWaitBits(this_->eventGroup, HCS_BTNS_EVENTS_BITS, pdTRUE, pdFALSE, portMAX_DELAY);
      if (this_->on_buttons_press_) {
        this_->on_buttons_press_(uxBits);
      }
    }
    vTaskDelete(NULL);
  }

private:
  std::function<void(EventBits_t)> on_buttons_press_;
  HCS301_t data_;
  uint32_t serial_;
  uint32_t last_encripted_ = 0;
  StaticEventGroup_t eventGroupBuffer_;
};


