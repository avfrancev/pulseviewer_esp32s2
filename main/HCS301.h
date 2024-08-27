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

  bool is_valid() const { return preamble == 0xfff; }

  void update(const uint8_t *d)
  {
    this->preamble = ((uint16_t)d[0] << 4) | ((d[1] >> 4) & 0x0F);                                                                                                                                       // 12 bits
    this->encrypted = ((uint32_t)reverse8(d[1] & 0x0F) << 28) | ((uint32_t)reverse8(d[2]) << 20) | ((uint32_t)reverse8(d[3]) << 12) | ((uint32_t)reverse8(d[4]) << 4) | ((uint32_t)reverse8(d[5]) >> 4); // 32 bits
    this->serial = ((uint32_t)reverse8(d[5] & 0x0F) << 24) | ((uint32_t)reverse8(d[6]) << 16) | ((uint32_t)reverse8(d[7]) << 8) | reverse8(d[8]);                                                        // 28 bits
    this->buttons = (d[9] >> 4) & 0xf;
    this->vlow = (d[9] >> 3) & 0x1;
    this->fixed = (d[9] >> 2) & 0x1;
  }
};


class HCS301 : public PWMDecoder {
public:
  EventGroupHandle_t eventGroup;

  /**
   * @brief Construct a new HCS301 object.
   *
   * @param serial a 28-bit serial number
   */
  HCS301(uint32_t serial = 0) {
    serial_ = serial;
    data_ = HCS301_t();
    eventGroup = xEventGroupCreateStatic(&eventGroupBuffer_);
    xTaskCreate(task_event_handler, "HCS301 event handler", 4*1024, this, 3, NULL);
  }

  /**
   * @brief Event handler task for HCS301 class.
   *
   * Waits for bits in eventGroup to be set and calls on_buttons_press_
   * callback with the set bits.
   *
   * @param args pointer to HCS301 object
   */
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

  /**
   * @brief Set a callback for button press events.
   *
   * The callback will be called with the set bits in the eventGroup as
   * argument when a new set of buttons is detected.
   *
   * @param cb a std::function<void(EventBits_t)> callback
   */
  void set_on_buttons_press(std::function<void(EventBits_t)> cb) {
    on_buttons_press_ = cb;
  }

  /**
   * @brief Decode the given PWM message and trigger button press events
   *
   * @param pwm_msg The PWM message to decode
   * @param rmt_msg The RMT message that was decoded into pwm_msg
   *
   * This function decodes the given PWM message and checks if it is a valid
   * message for the given serial number and if the encrypted value has changed.
   * If so, it triggers the button press event by setting the corresponding
   * bits in the eventGroup.
   */
  void decode_pwm(pwm_message_t *pwm_msg, rmt_message_t *rmt_msg) override {
    if (rmt_msg->length != 78) {
      return;
    }
    data_.update(pwm_msg->buf);
    if (data_.is_valid() && data_.serial == serial_ && data_.encrypted != last_encripted_) {
      xEventGroupSetBits(eventGroup, data_.buttons);
      last_encripted_ = data_.encrypted;
    }
  }



private:
  std::function<void(EventBits_t)> on_buttons_press_;
  HCS301_t data_;
  uint32_t serial_;
  uint32_t last_encripted_ = 0;
  StaticEventGroup_t eventGroupBuffer_;
};
