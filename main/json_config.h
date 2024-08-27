#pragma once
#include <Arduino.h>
#include "SPIFFS.h"
#include <cJSON.h>

#include "main.h"

#define JSON_CONFIG "JSON_CONFIG"

class JsonConfig {
  public:
    /**
     * @brief Save JSON data to a file.
     *
     * @param filename The name of the file to save the JSON data to.
     * @param data The JSON data to save.
     * @return true if the save operation was successful, false otherwise.
     */
    static bool save(const char* filename, const cJSON* data) {
      char* json = cJSON_Print(data);
      FILE *fp = fopen(filename, "w");
      if (fp == NULL) {
        ESP_LOGD(JSON_CONFIG, "ERROR: Could not open file for writing");
        return false;
      }
      fprintf(fp, "%s", json);
      fclose(fp);
      cJSON_free(json);
      return true;
    }

    /**
     * @brief Load JSON data from a file.
     *
     * @param filename The name of the file to load the JSON data from.
     * @param data A pointer to a cJSON object to store the loaded JSON data.
     * @return True if the load operation was successful, false otherwise.
     */
    static bool load(const char* filename, cJSON** data) {
      FILE *fp = fopen(filename, "r");
      if (fp == NULL) {
        ESP_LOGD(JSON_CONFIG, "ERROR: Could not open file for reading");
        return false;
      }
      fseek(fp, 0, SEEK_END);
      long size = ftell(fp);
      fseek(fp, 0, SEEK_SET);
      char* json = (char*)malloc(size+1);
      fread(json, 1, size, fp);
      json[size] = 0;
      fclose(fp);
      
      *data = cJSON_Parse(json);
      free(json);
      if (!*data) {
        ESP_LOGD(JSON_CONFIG, "JSON parse error");
        return false;
      }
      return true;
    }
};

