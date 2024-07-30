#pragma once
#include "main.h"
#include <Arduino.h>

// bits::getRange()


class PWMDecoder;

std::vector<PWMDecoder *> pwm_decoders;
bool isPWMDecoderInit = false;
class PWMDecoder
{
public:
  // static void init() {
  //   decoders.clear();
  // }
  PWMDecoder()
  {
    if (!isPWMDecoderInit) {
      pwm_decoders.clear();
      isPWMDecoderInit = true;
    }
    pwm_decoders.push_back(this);
  }
//   static void init() {
//     decoders.clear();
//     // decoders.push_back(this);
//     // Serial.println(decoders.size());
//   }
  // PWMDecoder() {
  //   Serial.println("PWM PARENT constructor");
  //   // addDecoder(this);
  //   decoders.push_back(this);
  // };
  static void decode(rmt_message_t* msg) {
    // Serial.printf("PWM decoder get %d bytes with rssi %d\n", msg->length, msg->rssi);
    pwm_message_t pwm_msg;
    pwm_msg.length = 0;
    // pwm_msg.length = msg->length / 8;
    // pwm_msg.rssi = msg->rssi;
    // pwm_msg.time = msg->time;

    for (int i = 0; i < msg->length; i++)
    {
      uint16_t d1 = msg->buf[i].duration0;
      uint16_t d2 = msg->buf[i].duration1;
      // uint8_t b = d1 < d2 ? 1 : 0;
      // if (abs(d2 - d1) < 300) b = 1;

      float diff = d1 - d2;
      float avg = (d1 + d2) / 2;
      float ratio = diff / avg;

      uint8_t b = ratio < 0.2 ? 1 : 0;

      if (i % 8 == 0)
      {
        pwm_msg.buf[i / 8] = 0;
      }

      if (b)
      {
        pwm_msg.buf[i / 8] |= (1 << (7 - i % 8));
      }

      // Serial.printf("%d", b);
      if (i % 8 == 7 || i == msg->length - 1)
      {
        pwm_msg.length++;
        // Serial.printf(" %02X ", pwm_msg.buf[(i) / 8]);
      }
    }
    // Serial.printf("\n");
    for (auto decoder : pwm_decoders)
    {
      decoder->decode_pwm(&pwm_msg, msg);
    }
  }
  virtual void decode_pwm(pwm_message_t *pwm_msg, rmt_message_t *rmt_msg) {}
};


