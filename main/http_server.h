#pragma once

#include "esp_vfs_fat.h"
#include "esp_spiffs.h"
#include <esp_http_server.h>
#include "mbedtls/base64.h"

#include "main.h"
#include "pump.h"


#define TAG_HTTP "HTTPD"
#define FILE_PATH_MAX (128 + 128)
#define SCRATCH_BUFSIZE (10240)


static inline bool file_exist(const char *path)
{
  FILE* f = fopen(path, "r");
  if (f == NULL) {
    fclose(f);
    return false;
  }
  fclose(f);
  return true;
}


#define CHECK_FILE_EXTENSION(filename, ext) (strcasecmp(&filename[strlen(filename) - strlen(ext)], ext) == 0)

/* Set HTTP response content type according to file extension */
static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filepath)
{
    if (CHECK_FILE_EXTENSION(filepath, ".gz")) {
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    }
    const char *type = "text/plain";
    if (CHECK_FILE_EXTENSION(filepath, ".html")) {
        type = "text/html";
    }
    else if (CHECK_FILE_EXTENSION(filepath, ".html.gz")) {
        type = "text/html";
    } else if (CHECK_FILE_EXTENSION(filepath, ".json")) {
        type = "application/json";
    } else if (CHECK_FILE_EXTENSION(filepath, ".js")) {
        type = "application/javascript";
    } else if (CHECK_FILE_EXTENSION(filepath, ".css")) {
        type = "text/css";
    } else if (CHECK_FILE_EXTENSION(filepath, ".png")) {
        type = "image/png";
    } else if (CHECK_FILE_EXTENSION(filepath, ".ico")) {
        type = "image/x-icon";
    } else if (CHECK_FILE_EXTENSION(filepath, ".svg")) {
        type = "text/xml";
    }
    return httpd_resp_set_type(req, type);
}

char chunk[1024] = { 0 };




static esp_err_t send_file(httpd_req_t *req, const char* filepath)
{
    int fd = open(filepath, O_RDONLY, 0);
    if (fd < 0) {
        return ESP_FAIL;
    }
    ESP_LOGD(TAG_HTTP, "Sending file: %s", filepath);
    set_content_type_from_file(req, filepath);

    /* Send file in chunks */
    ssize_t read_bytes;
    do {
        /* Read file in chunks into the scratch buffer */
        read_bytes = read(fd, chunk, 1024);
        if (read_bytes == -1) {
            close(fd);
            ESP_LOGE(TAG_HTTP, "Failed to read file");
            /* Abort sending file */
            httpd_resp_sendstr_chunk(req, NULL);
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
            return ESP_FAIL;
        }
        /* Send the buffer contents as HTTP response chunk */
        if (httpd_resp_send_chunk(req, chunk, read_bytes) != ESP_OK) {
            close(fd);
            ESP_LOGE(TAG_HTTP, "File sending failed!");
            /* Abort sending file */
            httpd_resp_sendstr_chunk(req, NULL);
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
            return ESP_FAIL;
        }
    } while (read_bytes > 0);

    close(fd);
    // Serial.println("File sending complete");
    ESP_LOGD(TAG_HTTP, "File sent: %s", filepath);
    /* Respond with an empty chunk to signal HTTP response completion */
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t rest_common_get_handler(httpd_req_t *req)
{
  const char* uri = req->uri;
  char filepath[FILE_PATH_MAX] = "/spiffs/www";
  strlcat(filepath, uri, sizeof(filepath));

  ESP_LOGD(TAG_HTTP, "File requested: %s", filepath);

  struct stat st;
  if (stat(filepath, &st) == 0) {
    return send_file(req, filepath);
  }

  strlcat(filepath, ".gz", sizeof(filepath));
  if (stat(filepath, &st) == 0) {
    return send_file(req, filepath);
  }

  strlcpy(filepath, "/spiffs/www/index.html.gz", sizeof(filepath));
  return send_file(req, filepath);
}

esp_err_t httpd_get_JSON(httpd_req_t *req, cJSON** json) {
  char content_type[32] = {0};
  httpd_req_get_hdr_value_str(req, "Content-Type", content_type, sizeof(content_type));
  if (strcmp(content_type, "application/json") == 0) {
      char content[256];
      size_t recv_size = MIN(req->content_len, sizeof(content));
      int ret = httpd_req_recv(req, content, recv_size);
      if (ret <= 0) {  /* 0 return value indicates connection closed */
          ESP_LOGE(TAG_HTTP, "Failed to recv data");
          return ESP_FAIL;
      } else {
        *json = cJSON_Parse(content);
        if (json == NULL) {
          ESP_LOGE(TAG_HTTP, "Failed to parse json");
          return ESP_FAIL;
        }
        return ESP_OK;
      }
  }
  return ESP_FAIL;
}

static esp_err_t pump_config_post_handler(httpd_req_t *req)
{
  // use httpd_req_get_hdr_value_str(httpd_req_t *r, const char *field, char *val, size_t val_size) get content type if json
  cJSON* json = nullptr;
  if (httpd_get_JSON(req, &json) == ESP_OK) {
    Serial.println(cJSON_Print(json));

    pump->setPumpConfig(json);
    
    cJSON_free(json);
    return send_file(req, "/spiffs/pump_config.json");
  }
  cJSON_free(json);
  httpd_resp_send_500(req);
  return ESP_FAIL;
}


// static esp_err_t register_uri_handler(httpd_handle_t server, const char* uri, httpd_method_t method, esp_err_t handler)
// static esp_err_t handler_wrapper(httpd_req_t *req, esp_err_t (*handler)(httpd_req_t *req)) {
//   // *handler(req);
// }

static esp_err_t register_uri_handler(httpd_handle_t server, const char* uri, httpd_method_t method, esp_err_t (*handler)(httpd_req_t *req))
{
  auto handler_wrapper = [] (httpd_req_t *req) {
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
    esp_err_t (*handler)(httpd_req_t *) = (esp_err_t (*)(httpd_req_t *))req->user_ctx;
    return handler(req);
  };
  httpd_uri_t new_uri = {
    .uri = uri,
    .method = method,
    .handler = handler_wrapper,
    .user_ctx = (void *)handler
  };
  return httpd_register_uri_handler(server, &new_uri);
}

// // implement class, that takes freertos queue. It must register consumers callbacks. On queue recive it must send it to callbacks
// QueueHandle_t queue;
// struct item_t {
//   int values[10];
//   int len;
// } item_t;

// class QueueConsumer {
// public:
//   QueueConsumer(QueueHandle_t queue) : queue_(queue) {}

//   void registerConsumer(void (*consumer)(void *)) {
//     consumers_.push_back(consumer);
//   }

//   void start() {
//     xTaskCreate(task, "queueConsumer", 2048, this, 1, &task_handle_);
//   }

//   void stop() {
//     vTaskDelete(task_handle_);
//   }

// private:
//   static void task(void *arg) {
//     QueueConsumer *self = (QueueConsumer *)arg;

//     void* item;
//     while (xQueueReceive(self->queue_, &item, portMAX_DELAY) == pdPASS) {
//       // cast to item_t
//       item_t *item_ptr = (item_t *)item;
//       Serial.printf("received item: %d\n", item_ptr->len);
//       for (auto consumer : self->consumers_) {
//         consumer(item);
//       }
//     }
//     vTaskDelete(NULL);
//   }

//   QueueHandle_t queue_;
//   std::vector<void (*)(void *)> consumers_;
//   TaskHandle_t task_handle_;
// };




// static void testQueueProducerTask(void *pvParameters) {
//   item_t item = {
//     .values = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10},
//     .len = 10
//   };

//   for(;;) {
//     Serial.printf("sending item: %d\n", item.len);
//     xQueueSend(queue, &item, portMAX_DELAY);
//     Serial.printf("sent item: %d\n", item.len);
//     vTaskDelay(1000 / portTICK_PERIOD_MS);
//     // xQueueSend(queue, &queue, portMAX_DELAY);
//   }
//   vTaskDelete(NULL);
// }
// static void testQueueConsumer() {
//   queue = xQueueCreate(2, sizeof(&item_t));
//   QueueConsumer *consumer = new QueueConsumer(queue);
//   // consumer->registerConsumer([] (void *item) {
//   //   Serial.printf("got item: %d\n", (int)item);
//   // });
//   // consumer->registerConsumer([] (void *item) {
//   //   // cast item to item_t
//   //   item_t *item_ptr = (item_t *)item;
//   //   Serial.printf("got!!!! item: %d\n", 111);
//   // });
//   consumer->start();
//   xTaskCreate(testQueueProducerTask, "testQueueConsumer", 2048, NULL, 1, NULL);
// }


/**
 * Handler function for Server-Sent Events (SSE) request.
 * This function sets the content type to text/event-stream and sends an
 * initial retry delay of 1000ms. Then it enters an infinite loop waiting
 * for task notifications. When a notification is received, it sends a
 * data message to the client with an empty payload. The empty payload is
 * required by the SSE spec.
 *
 * The function is registered to handle requests to the URI "/events".
 */
static esp_err_t sse_handler(httpd_req_t *req)
{
  /**
   * Set the content type of the response to text/event-stream. This tells
   * the client that the response is an SSE stream.
   */
  httpd_resp_set_type(req, "text/event-stream");

  /**
   * Send an initial retry delay of 1000ms to the client. This tells the
   * client how long to wait before retrying if there is no data available.
   */
  httpd_resp_sendstr_chunk(req, "retry: 1000\n\n");

  /**
   * Create a task to handle SSE requests. The task will block indefinitely
   * waiting for task notifications. When a notification is received, it
   * will send a data message to the client with an empty payload.
   */
  // TaskData *task_data = new TaskData();
  // task_data->req = req;
  // xTaskCreatePinnedToCore(sse_task, "sse_task", 2048, task_data, 1, NULL, 0);

  /**
   * Return ESP_OK to indicate that the request was handled successfully.
   */
  return ESP_OK;
}

////////////////////////////////////////
// #include "keep_alive.h"

httpd_handle_t server = NULL;

struct async_resp_arg {
    httpd_handle_t hd;
    int fd;
};

/*
 * async send function, which we put into the httpd work queue
 */
static void ws_async_send(void *arg)
{
    static const char * data = "Async data";
    struct async_resp_arg *resp_arg = (async_resp_arg*)arg;
    httpd_handle_t hd = resp_arg->hd;
    int fd = resp_arg->fd;
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t*)data;
    ws_pkt.len = strlen(data);
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    httpd_ws_send_frame_async(hd, fd, &ws_pkt);
    free(resp_arg);
}

static esp_err_t trigger_async_send(httpd_handle_t handle, httpd_req_t *req)
{
    // struct async_resp_arg *resp_arg = malloc(sizeof(struct async_resp_arg));
    struct async_resp_arg *resp_arg = new async_resp_arg;
    resp_arg->hd = req->handle;
    resp_arg->fd = httpd_req_to_sockfd(req);
    return httpd_queue_work(handle, ws_async_send, resp_arg);
}


static esp_err_t echo_handler(httpd_req_t *req)
{
  // return ESP_OK;
    if (req->method == HTTP_GET) {
        // Serial.printf("Handshake done, the new connection was opened\n");
        return ESP_OK;
    }
    httpd_ws_frame_t ws_pkt;
    uint8_t *buf = NULL;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    /* Set max_len = 0 to get the frame len */
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        // Serial.printf("httpd_ws_recv_frame failed to get frame len with %d\n", ret);
        return ret;
    }
    // Serial.printf("frame len is %d\n", ws_pkt.len);
    if (ws_pkt.len) {
        /* ws_pkt.len + 1 is for NULL termination as we are expecting a string */
        buf = (uint8_t*)calloc(1, ws_pkt.len + 1);
        if (buf == NULL) {
            // Serial.printf("Failed to calloc memory for buf\n");
            return ESP_ERR_NO_MEM;
        }
        ws_pkt.payload = buf;
        /* Set max_len = ws_pkt.len to get the frame payload */
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK) {
            // Serial.printf("httpd_ws_recv_frame failed with %d\n", ret);
            free(buf);
            return ret;
        }
        // Serial.printf("Got packet with message: %s\n", ws_pkt.payload);
    }
    // Serial.printf("Packet type: %d\n", ws_pkt.type);
    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT &&
        strcmp((char*)ws_pkt.payload,"Trigger async") == 0) {
        free(buf);
        return trigger_async_send(req->handle, req);
    }

    ret = httpd_ws_send_frame(req, &ws_pkt);
    if (ret != ESP_OK) {
        // Serial.printf("httpd_ws_send_frame failed with %d\n", ret);
    }
    free(buf);
    return ret;
}

//implement messagebuffer freertos send and recive example
#include "freertos/message_buffer.h"

static MessageBufferHandle_t wsMeassageBufferHandle = NULL;

void ws_broadcast(uint8_t *data, size_t len) {
    // Serial.printf("Broadcasting %d bytes. Buffer available: %d\n", len, xMessageBufferSpacesAvailable(wsMeassageBufferHandle));
    if (xMessageBufferSend(wsMeassageBufferHandle, data, len, 500 / portTICK_PERIOD_MS) == pdFALSE) {
        ESP_LOGE(TAG_HTTP, "Failed to send data to message buffer");
    }
}

int count = 0;

static void ws_broadcast_buf( uint8_t *buf, size_t len) {
    static const size_t max_clients = 8;
    size_t clients = max_clients;
    int    client_fds[max_clients];
    httpd_ws_frame_t ws_pkt;
    
    if (httpd_get_client_list(server, &clients, client_fds) == ESP_OK) {
      // Serial.printf("Active clients: %d\n", clients);
      for (size_t i=0; i < clients; ++i) {
        int sock = client_fds[i];
        if (httpd_ws_get_fd_info(server, sock) == HTTPD_WS_CLIENT_WEBSOCKET) {
            // Serial.printf("Active client (fd=%d) -> sending async message (length: %d)\n", sock, len);
            ESP_LOGD(TAG_HTTP, "Active client (fd=%d) -> sending async message (length: %d)\n", sock, len);
            // send meassge syncronously
            // httpd_ws_send_data(*server, sock, "Trigger async", HTTPD_WS_TYPE_TEXT);

            // struct async_resp_arg *resp_arg = malloc(sizeof(struct async_resp_arg));
            struct async_resp_arg *resp_arg = new async_resp_arg;
            resp_arg->hd = server;
            resp_arg->fd = sock;
            memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
            ws_pkt.payload = buf;
            ws_pkt.len = len;
            ws_pkt.type = HTTPD_WS_TYPE_BINARY;

            if (httpd_ws_send_frame_async(resp_arg->hd, resp_arg->fd, &ws_pkt) != ESP_OK) {
                ESP_LOGE(TAG_HTTP, "httpd_ws_send_frame_async failed!");
            }
            // Serial.printf("httpd_ws_send_frame_async done FD=%d!\n", resp_arg->fd);
            // httpd_ws_send_frame_async(resp_arg->hd, resp_arg->fd, &ws_pkt);
            // free(resp_arg);
            vTaskDelay(5 / portTICK_PERIOD_MS);
            // if (httpd_queue_work(resp_arg->hd, send_hello, resp_arg) != ESP_OK) {
            //     Serial.printf("httpd_queue_work failed!\n");
            //     send_messages = false;
            //     break;
            // }
        }
      }
    } else {
      ESP_LOGE(TAG_HTTP, "httpd_get_client_list failed!");
      return;
    }
}

static void ws_broadcast_task(void *pvParameters)
{
    httpd_handle_t* server = (httpd_handle_t*)pvParameters;




    wsMeassageBufferHandle = xMessageBufferCreate(512 * 6);
    assert(wsMeassageBufferHandle != NULL);


    char data[512*4];
    size_t len = 512*4;
    while (1) {
        size_t len_out = xMessageBufferReceive(wsMeassageBufferHandle, data, len, portMAX_DELAY);
        if (len_out > 0) {
            // Serial.printf("xMessageBufferReceive len_out: %d\n", len_out);
            ws_broadcast_buf((uint8_t *)data, len_out);
        } else {
            // Serial.printf("xMessageBufferReceive failed\n");
            ESP_LOGE(TAG_HTTP, "xMessageBufferReceive failed");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
        // vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}




void start_webserver()
{
  // testQueueConsumer();
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.stack_size = 1024 * 10;
  config.lru_purge_enable = true;
  config.uri_match_fn = httpd_uri_match_wildcard;
  config.lru_purge_enable = true;


  if (httpd_start(&server, &config) == ESP_OK)
  {
    register_uri_handler(server, "/pump_config", HTTP_GET, [](httpd_req_t *req) {
      send_file(req, "/spiffs/pump_config.json");
      return ESP_OK;
    });
    
    register_uri_handler(server, "/pump_config", HTTP_POST, pump_config_post_handler);
    register_uri_handler(server, "/events", HTTP_GET, sse_handler);

    // initWSServer(&server);
    static const httpd_uri_t ws = {
      .uri        = "/ws",
      .method     = HTTP_GET,
      .handler    = echo_handler,
      .user_ctx   = NULL,
      .is_websocket = true
    };

    httpd_register_uri_handler(server, &ws);
    
    xTaskCreate(ws_broadcast_task, "ws_broadcast_task", 8*1024, &server, 1, NULL);

    register_uri_handler(server, "/*", HTTP_OPTIONS, [](httpd_req_t *req) {
      httpd_resp_set_status(req, "200 OK");
      httpd_resp_send(req, NULL, 0);
      return ESP_OK;
    });


    register_uri_handler(server, "/*", HTTP_GET, rest_common_get_handler);


    ESP_LOGD(TAG_HTTP, "HTTP server started at port %d", config.server_port);
    // return server;
  }

  // return NULL;
}

