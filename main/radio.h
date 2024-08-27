#pragma once
#include "main.h"
#include <ELECHOUSE_CC1101_SRC_DRV.h>
#include "driver/rmt_rx.h"

#include "http_server.h"

#include "decoders.h"
#include <stddef.h>



#define TAG_RADIO "RADIO"
QueueHandle_t rmt_parse_queue;
QueueHandle_t receive_queue;

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
    if (xQueueReceive(rmt_parse_queue, &msg, portMAX_DELAY) == pdTRUE) {
      PWMDecoder::decode(&msg);
      if (msg.length < 2) continue;
      uint8_t *bytePtr = (uint8_t*)&msg;
      ws_broadcast(bytePtr, sizeof(msg));
    }
  }
  vTaskDelete(NULL);
}

/**
 * @brief This function is a callback for the RMT driver which is invoked when a RX is done.
 * Also, it sends the received data to the rmt_parse_task for decoding.
 *
 * @param channel The RMT channel which has received the data.
 * @param edata Pointer to the RX done event data.
 * @param user_data Pointer to a `QueueHandle_t` which is the queue to send the data to.
 *
 * @return `true` if a higher priority task was woken by this function, `false` otherwise.
 */
static bool rmt_rx_done_callback(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *edata, void *user_data)
{
    BaseType_t high_task_wakeup = pdFALSE;
    QueueHandle_t receive_queue = (QueueHandle_t)user_data;
    xQueueSendFromISR(receive_queue, edata, &high_task_wakeup);
    return high_task_wakeup == pdTRUE;
}

/**
 * @brief This is the task that receives RMT symbols over the RX channel and
 * passes them to the rmt_parse_task for decoding.
 *
 * @param pvParameters unused
 *
 * This function creates an RMT RX channel and registers an on_recv_done callback
 * that passes received symbols to the rmt_parse_task for decoding. The timing
 * range is set to meet the NEC protocol specification. 
 *
 * The function runs in an infinite loop, waiting for RMT symbols to be received.
 * Once symbols are received, it checks if the number of symbols is less than or
 * equal to 3. If so, it discards the symbols and continues to wait for more. If
 * the number of symbols is greater than 3, it creates a message with the RSSI,
 * the length of the symbols, the time the symbols were received, and the time
 * difference between the start of reception and the current time. It then sends
 * the message to the rmt_parse_task for decoding.
 *
 * This function should be run in a task with a high priority to ensure that
 * received symbols are processed as quickly as possible.
 */
static void rmt_recive_task(void *pvParameters) {
  ESP_LOGD(TAG_RADIO, "create RMT RX channel");
  rmt_rx_channel_config_t rx_channel_cfg = {
      .gpio_num = (gpio_num_t)CC1101_gdo2,
      .clk_src = RMT_CLK_SRC_DEFAULT,
      .resolution_hz = 1000000,
      .mem_block_symbols = 256, // amount of RMT symbols that the channel can store at a time
  };
  rmt_channel_handle_t rx_channel = NULL;
  ESP_ERROR_CHECK(rmt_new_rx_channel(&rx_channel_cfg, &rx_channel));

  ESP_LOGD(TAG_RADIO, "register RX done callback");
  QueueHandle_t receive_queue = xQueueCreate(3, sizeof(rmt_rx_done_event_data_t));
  assert(receive_queue);

  rmt_rx_event_callbacks_t cbs = {
      .on_recv_done = rmt_rx_done_callback,
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

