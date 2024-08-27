#pragma once

#include "esp_vfs_fat.h"
#include "esp_spiffs.h"
#include <esp_http_server.h>
#include "mbedtls/base64.h"
#include "freertos/message_buffer.h"
#include "WiFi.h"


#include "main.h"
#include "pump.h"


#define TAG_HTTP "HTTPD"
#define FILE_PATH_MAX (128 + 128)
#define SCRATCH_BUFSIZE (10240)

char chunk[1024] = { 0 };

httpd_handle_t server = NULL;

struct async_resp_arg {
    httpd_handle_t hd;
    int fd;
};

static MessageBufferHandle_t wsMeassageBufferHandle = NULL;

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



/**
 * Sends a file in chunks as an HTTP response.
 *
 * @param req The HTTP request object.
 * @param filepath The path to the file to be sent.
 * @return ESP_OK if the file was sent successfully, ESP_FAIL otherwise.
 *
 * This function opens the file specified by `filepath`, reads it in chunks, and sends each chunk as an HTTP response.
 * The MIME type of the file is determined by its extension using the `set_content_type_from_file` function.
 * If an error occurs while reading the file or sending the response, an error message is logged and the function
 * returns ESP_FAIL.
 */
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
    ESP_LOGD(TAG_HTTP, "File sent: %s", filepath);
    /* Respond with an empty chunk to signal HTTP response completion */
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

/**
 * HTTP GET handler for common REST requests.
 *
 * @param req The HTTP request object.
 * @return ESP_OK if the file was sent successfully, ESP_FAIL otherwise.
 *
 * This function tries to find the requested file in two locations: the original
 * location (i.e., filepath) and the location with the ".gz" extension. If the file
 * is found, it is sent as an HTTP response using the send_file function. If not,
 * the "index.html.gz" file is sent.
 */
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

/**
 * @brief Retrieve JSON data from an HTTP request
 * 
 * This function retrieves JSON data from an HTTP request. It first checks
 * if the request content type is "application/json" by calling 
 * httpd_req_get_hdr_value_str and comparing the result with "application/json".
 * If the content type is correct, it receives the data from the request using
 * httpd_req_recv. The received data is then parsed into a cJSON object using
 * cJSON_Parse. If parsing is successful, the cJSON object is assigned to the
 * pointer pointed to by the second argument and ESP_OK is returned. If parsing
 * fails, ESP_FAIL is returned. If the request content type is not "application/json",
 * ESP_FAIL is returned.
 * 
 * @param req The HTTP request object
 * @param json A pointer to a pointer to a cJSON object where the parsed JSON
 *             object will be stored.
 * @return esp_err_t ESP_OK if parsing is successful, ESP_FAIL otherwise.
 */
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

/**
 * @brief Handle POST request to /pump_config. Set new pump configuration and
 *        save it to file.
 * 
 * This function handles POST requests to /pump_config. It first attempts to
 * parse the request body as JSON using httpd_get_JSON. If parsing is
 * successful, it calls pump->setPumpConfig with the parsed JSON object and
 * then sends the file /spiffs/pump_config.json as the response. If parsing fails,
 * a 500 Internal Server Error is returned.
 * 
 * @param req The HTTP request object
 * @return esp_err_t ESP_FAIL if parsing fails, ESP_OK otherwise.
 */
static esp_err_t pump_config_post_handler(httpd_req_t *req)
{
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



/**
 * @brief Register a URI handler for the given method and URI.
 *
 * This function registers a URI handler for the given method and URI. The
 * handler_wrapper function is used to set the necessary headers for CORS and
 * then calls the actual handler function. The handler function is stored in
 * the user_ctx field of the httpd_uri_t struct.
 *
 * @param server The HTTP server handle.
 * @param uri The URI to register the handler for.
 * @param method The HTTP method to register the handler for.
 * @param handler The handler function to register.
 * @return esp_err_t ESP_OK on success, ESP_FAIL on error.
 */
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


/**
 * @brief Send a WebSocket frame asynchronously.
 *
 * This function is a callback function for sending a WebSocket frame
 * asynchronously. It takes a void pointer to an argument struct
 * `async_resp_arg`, which contains the HTTP server handle `hd` and
 * the file descriptor `fd`. It sends a WebSocket text frame with the
 * payload "Async data" to the client. After sending the frame, it
 * frees the argument struct.
 *
 * @param arg A void pointer to an argument struct `async_resp_arg`.
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
    struct async_resp_arg *resp_arg = new async_resp_arg;
    resp_arg->hd = req->handle;
    resp_arg->fd = httpd_req_to_sockfd(req);
    return httpd_queue_work(handle, ws_async_send, resp_arg);
}


static esp_err_t echo_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        return ESP_OK;
    }
    httpd_ws_frame_t ws_pkt;
    uint8_t *buf = NULL;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    /* Set max_len = 0 to get the frame len */
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        return ret;
    }
    if (ws_pkt.len) {
        /* ws_pkt.len + 1 is for NULL termination as we are expecting a string */
        buf = (uint8_t*)calloc(1, ws_pkt.len + 1);
        if (buf == NULL) {
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
    }
    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT &&
        strcmp((char*)ws_pkt.payload,"Trigger async") == 0) {
        free(buf);
        return trigger_async_send(req->handle, req);
    }

    ret = httpd_ws_send_frame(req, &ws_pkt);
    free(buf);
    return ret;
}



/**
 * @brief Broadcasts a binary message to all connected websocket clients.
 *
 * This function sends a binary message to all connected websocket clients.
 * It first checks if the device is connected to a WiFi network. If not,
 * this function returns without doing anything.
 *
 * If the device is connected to WiFi, this function sends the binary
 * message to the message buffer using `xMessageBufferSend`.
 *
 * @param data Pointer to the binary message to be sent.
 * @param len Length of the binary message.
 *
 * @return void
 *
 * @throws None
 */
void ws_broadcast(uint8_t *data, size_t len) {
    if (xMessageBufferSend(wsMeassageBufferHandle, data, len, 500 / portTICK_PERIOD_MS) == pdFALSE) {
        ESP_LOGE(TAG_HTTP, "Failed to send data to message buffer");
    }
}


/**
 * @brief Broadcasts a binary message to all connected websocket clients.
 *
 * This function sends a binary message to all connected websocket clients.
 * It first checks if the device is connected to a WiFi network. If not,
 * this function returns without doing anything.
 *
 * If the device is connected to WiFi, this function retrieves the list of
 * connected clients and sends a binary message to each client. The binary
 * message is sent asynchronously using `httpd_ws_send_frame_async()`.
 *
 * @param buf Pointer to the buffer containing the binary message.
 * @param len Length of the binary message.
 *
 * @return void
 */
static void ws_broadcast_buf( uint8_t *buf, size_t len) {
    if (WiFi.status() != WL_CONNECTED) {
      ESP_LOGE(TAG_HTTP, "ws_broadcast_buf: Not connected to WiFi");
      return;
    }
    static const size_t max_clients = 8;
    size_t clients = max_clients;
    int    client_fds[max_clients];
    httpd_ws_frame_t ws_pkt;
    
    if (httpd_get_client_list(server, &clients, client_fds) == ESP_OK) {
      for (size_t i=0; i < clients; ++i) {
        int sock = client_fds[i];
        if (httpd_ws_get_fd_info(server, sock) == HTTPD_WS_CLIENT_WEBSOCKET) {
            ESP_LOGD(TAG_HTTP, "Active client (fd=%d) -> sending async message (length: %d)\n", sock, len);

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
            vTaskDelay(5 / portTICK_PERIOD_MS);
        }
      }
    } else {
      ESP_LOGE(TAG_HTTP, "httpd_get_client_list failed!");
      return;
    }
}

/**
 * @brief Task that receives message buffer messages and broadcasts
 * them to all websocket clients.
 *
 * This task is created by `start_webserver()` and is responsible for
 * receiving messages from the message buffer and sending them to all
 * connected websocket clients.
 *
 * @param pvParameters Pointer to `httpd_handle_t*` server handle.
 */
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
            ws_broadcast_buf((uint8_t *)data, len_out);
        } else {
            ESP_LOGE(TAG_HTTP, "xMessageBufferReceive failed");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }
}



void start_webserver()
{
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
  }
}

