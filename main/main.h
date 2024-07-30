#ifndef main_app_h
#define main_app_h

#include <cJSON.h>
#include "json_config.h"
#include "wifi.h"

#define CC1101_sck 36
#define CC1101_miso 37
#define CC1101_mosi 35
#define CC1101_ss 34
#define CC1101_gdo0 18
#define CC1101_gdo2 33

#define JSON_OBJECT_NOT_NULL(jsonThing, name, default_val) \
    (cJSON_GetObjectItem(jsonThing, name) != NULL ? \
    cJSON_GetNumberValue(cJSON_GetObjectItem(jsonThing, name)) : default_val)


typedef struct rmt_message_t
{
  uint16_t length;
  unsigned long time;
  int64_t delta;
  int rssi;
  rmt_data_t buf[RMT_MEM_NUM_BLOCKS_4 * RMT_SYMBOLS_PER_CHANNEL_BLOCK];
} rmt_message_t;

typedef struct pwm_message_t
{
  uint8_t buf[RMT_MEM_NUM_BLOCKS_4 * RMT_SYMBOLS_PER_CHANNEL_BLOCK / 8];
  uint8_t length;
} pwm_message_t;


namespace bits
{
  inline uint8_t getBit(uint8_t byte, uint8_t bitIndex)
  {
    return (byte >> bitIndex) & 1;
  }

  inline void setBit(uint8_t &byte, uint8_t bitIndex, uint8_t value)
  {
    byte = (byte & ~(1 << bitIndex)) | (value << bitIndex);
  }

  inline void toggleBit(uint8_t &byte, uint8_t bitIndex)
  {
    byte ^= 1 << bitIndex;
  }
  inline uint8_t reverseBits(uint8_t byte) {
    byte = (byte & 0xF0) >> 4 | (byte & 0x0F) << 4;
    byte = (byte & 0xCC) >> 2 | (byte & 0x33) << 2;
    byte = (byte & 0xAA) >> 1 | (byte & 0x55) << 1;
    return byte;
  }
  inline uint8_t getRange(uint8_t byte, uint8_t start, uint8_t end) {
    return (byte >> start) & ((1 << (end - start + 1)) - 1);
  }
}


bool setup_CC1101();
void setup_wifi();
void start_webserver();

#endif
