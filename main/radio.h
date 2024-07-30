#pragma once
#include "main.h"
#include <ELECHOUSE_CC1101_SRC_DRV.h>
#include "driver/rmt_rx.h"

#include "http_server.h"

#include "decoders.h"
#include <stddef.h>



#define TAG_RADIO "RADIO"
// size_t rx_num_symbols = RMT_MEM_NUM_BLOCKS_4 * RMT_SYMBOLS_PER_CHANNEL_BLOCK;
// rmt_data_t rx_symbols[RMT_MEM_NUM_BLOCKS_4 * RMT_SYMBOLS_PER_CHANNEL_BLOCK];
QueueHandle_t rmt_parse_queue;
bool setup_CC1101()
{

  ELECHOUSE_cc1101.setSpiPin(CC1101_sck, CC1101_miso, CC1101_mosi, CC1101_ss);

  if (!ELECHOUSE_cc1101.getCC1101())
  {
    return false;
  }
  ELECHOUSE_cc1101.Init();         // must be set to initialize the cc1101!
  ELECHOUSE_cc1101.setMHZ(433.92); // Here you can set your basic frequency. The lib calculates the frequency automatically (default = 433.92).The cc1101 can: 300-348 MHZ, 387-464MHZ and 779-928MHZ. Read More info from datasheet.
  ELECHOUSE_cc1101.setRxBW(812.50); // Set the Receive Bandwidth in kHz. Value from 58.03 to 812.50. Default is 812.50 kHz.

  ELECHOUSE_cc1101.SetRx(); // set Receive on
  return true;
}



static void rmt_parse_task(void *pvParameters) {
  rmt_message_t msg;
  while (1)
  {
    if (xQueueReceive(rmt_parse_queue, &msg, portMAX_DELAY) == pdTRUE)
    {
      // Serial.printf("Got %d RMT Symbols with RSSI %d [ uxQueueSpacesAvailable: %d ]\n", msg.length, msg.rssi, uxQueueSpacesAvailable(rmt_parse_queue));
      // decodePWM(&msg);
      // msg.
      PWMDecoder::decode(&msg);
      if (msg.length < 2) continue;
      // Serial.printf("Got %d RMT Symbols with RSSI %d\n", msg.length, msg.rssi);
      // ws_broadcast((uint8_t *)&msg.buf, msg.length*4);
      
      // Serial.printf("sizeof(struct rmt_message_t) %d\n", sizeof(struct rmt_message_t));
      // Serial.printf("sizeof(msg) %d\n", sizeof(msg));
      // Serial.printf(" %d / %ld / %d\n", msg.length, msg.delta, msg.rssi);
      // Serial.printf(" %d / %ld / %d\n", sizeof(msg.length), sizeof(msg.delta), sizeof(msg.rssi));
      // Serial.printf("offset time: %ld\n", (long)offsetof(struct rmt_message_t, time));
      // Serial.printf("offset delta: %ld\n", (long)offsetof(struct rmt_message_t, delta));
      // Serial.printf("offset rssi: %ld\n", (long)offsetof(struct rmt_message_t, rssi));
      // Serial.printf("offset buf: %ld\n", (long)offsetof(struct rmt_message_t, buf));
      uint8_t *bytePtr = (uint8_t*)&msg;
      ws_broadcast(bytePtr, sizeof(msg));
      // ws_broadcast((uint8_t *)&msg, sizeof(msg));
    }
  }
  vTaskDelete(NULL);
}
QueueHandle_t receive_queue;

static bool example_rmt_rx_done_callback(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *edata, void *user_data)
{
    BaseType_t high_task_wakeup = pdFALSE;
    QueueHandle_t receive_queue = (QueueHandle_t)user_data;
    // send the received RMT symbols to the parser task
    xQueueSendFromISR(receive_queue, edata, &high_task_wakeup);
    return high_task_wakeup == pdTRUE;
}

static void rmt_recive_task(void *pvParameters) {
  // ESP_LOGD(TAG, "create RMT RX channel");
  rmt_rx_channel_config_t rx_channel_cfg = {
      .gpio_num = (gpio_num_t)CC1101_gdo2,
      .clk_src = RMT_CLK_SRC_DEFAULT,
      .resolution_hz = 1000000,
      .mem_block_symbols = 256, // amount of RMT symbols that the channel can store at a time
  };
  rmt_channel_handle_t rx_channel = NULL;
  ESP_ERROR_CHECK(rmt_new_rx_channel(&rx_channel_cfg, &rx_channel));

  // ESP_LOGD(TAG, "register RX done callback");
  QueueHandle_t receive_queue = xQueueCreate(3, sizeof(rmt_rx_done_event_data_t));
  assert(receive_queue);

  rmt_rx_event_callbacks_t cbs = {
      .on_recv_done = example_rmt_rx_done_callback,
  };
  ESP_ERROR_CHECK(rmt_rx_register_event_callbacks(rx_channel, &cbs, receive_queue));

  // the following timing requirement is based on NEC protocol
  rmt_receive_config_t receive_config = {
      .signal_range_min_ns = 1250,     // the shortest duration for NEC signal is 560us, 1250ns < 560us, valid signal won't be treated as noise
      .signal_range_max_ns = 12000000, // the longest duration for NEC signal is 9000us, 12000000ns > 9000us, the receive won't stop early
  };
  ESP_ERROR_CHECK(rmt_enable(rx_channel));
  rmt_symbol_word_t raw_symbols[256];
  ESP_ERROR_CHECK(rmt_receive(rx_channel, raw_symbols, sizeof(raw_symbols), &receive_config));
  rmt_rx_done_event_data_t rx_data;

  int64_t time_start = 0;
  int64_t delta = 0;
  rmt_message_t message;

  while (1) {
    time_start = esp_timer_get_time() - delta;

    if (xQueueReceive(receive_queue, &rx_data, portMAX_DELAY) == pdPASS) {
      // Serial.printf("Got %d RMT Symbols\n", rx_data.num_symbols);

      int64_t now = esp_timer_get_time();
      delta = now - time_start;
      
      if (rx_data.num_symbols <= 3)
      {
        ESP_ERROR_CHECK(rmt_receive(rx_channel, raw_symbols, sizeof(raw_symbols), &receive_config));
        continue;
      }

      message.rssi = ELECHOUSE_cc1101.getRssi();
      message.length = rx_data.num_symbols;
      message.time = millis();
      message.delta = delta;
      memcpy(message.buf, rx_data.received_symbols, rx_data.num_symbols * 4);
      // Serial.printf("Got %d symbols, RSSI: %d, delta: %ld\n", message.length, message.rssi, delta);
      ESP_LOGD(TAG_RADIO, "Got %d symbols, RSSI: %d, delta: %lld", message.length, message.rssi, delta);

      xQueueSend(rmt_parse_queue, &message, 0);
      delta = 0;
      ESP_ERROR_CHECK(rmt_receive(rx_channel, raw_symbols, sizeof(raw_symbols), &receive_config));
    }
  }
}

static void initRadio()
{
  if (!setup_CC1101()) {
    ESP_LOGE(TAG_RADIO, "Failed to setup CC1101");
    return;
  }
  rmt_parse_queue = xQueueCreate(4, sizeof(rmt_message_t));
  if (rmt_parse_queue == NULL) {
    ESP_LOGE(TAG_RADIO, "Failed to create RMT queue");
    return;
  }  
  xTaskCreate(rmt_recive_task, "rmt_recive_task", 1024 * 8, NULL, 6, NULL);

  xTaskCreate(rmt_parse_task, "rmt_parse_task", 1024 * 8, NULL, 1, NULL);

  ESP_LOGD(TAG_RADIO, "OK");
}