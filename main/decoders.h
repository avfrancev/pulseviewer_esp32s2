#pragma once
#include "main.h"
#include <Arduino.h>


class PWMDecoder;

std::vector<PWMDecoder *> pwm_decoders;
bool isPWMDecoderInit = false;

class PWMDecoder
{
public:
  /**
   * @brief Constructor for PWMDecoder class. This constructor adds the newly created object to the pwm_decoders vector.
   *        It also initializes the vector if it has not been initialized before.
   */
  PWMDecoder()
  {
    if (!isPWMDecoderInit) {
      pwm_decoders.clear();
      isPWMDecoderInit = true;
    }
    pwm_decoders.push_back(this);
  }
  /**
   * @brief Decodes the given RMT message and calls decode_pwm on all registered PWM decoders.
   * @param msg The RMT message to decode.
   *
   * This function implements the decoding of a RMT message (a message that was received using the RMT library) into a PWM message.
   * The decoding is done as follows: It loops over the given RMT message and for each pair of durations (duration0 and duration1) it calculates
   * the difference and the average. If the difference is less than 20% of the average, it is considered a 0, otherwise it is considered a 1. These
   * values are then packed into uint8_t values, with the first bit of each value being the first bit of the first pair, the second bit of each value
   * being the second bit of the first pair and so on. The length of the PWM message is the number of these uint8_t values.
   * After the PWM message is decoded, it calls decode_pwm on all registered PWM decoders.
   */
  static void decode(rmt_message_t* msg) {
    pwm_message_t pwm_msg;
    pwm_msg.length = 0;

    for (int i = 0; i < msg->length; i++)
    {
      uint16_t d1 = msg->buf[i].duration0;
      uint16_t d2 = msg->buf[i].duration1;

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

      if (i % 8 == 7 || i == msg->length - 1)
      {
        pwm_msg.length++;
      }
    }
    for (auto decoder : pwm_decoders)
    {
      decoder->decode_pwm(&pwm_msg, msg);
    }
  }
  virtual void decode_pwm(pwm_message_t *pwm_msg, rmt_message_t *rmt_msg) {}
};



