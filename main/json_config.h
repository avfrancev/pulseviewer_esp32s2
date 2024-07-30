#pragma once
#include <Arduino.h>
#include "SPIFFS.h"
#include <cJSON.h>

#include "main.h"



// bool spiffs_initialized = false;

class JsonConfig {
  public:

    // static bool initSPIFFS() {
    //   if (!spiffs_initialized) {
    //     if (SPIFFS.begin(true)) {
    //       spiffs_initialized = true;
    //       return true;
    //     } else {
    //       return false;
    //     }
    //   }
    //   return spiffs_initialized;
    // }
    static bool save(const char* filename, const cJSON* data) {
      // if (!initSPIFFS()) return false;
      char* json = cJSON_Print(data);
      // File f = SPIFFS.open(filename, "w");
      // write json to file using fopen function
      FILE *fp = fopen(filename, "w");
      if (fp == NULL) {
        Serial.println("ERROR: Could not open file for writing");
        return false;
      }
      fprintf(fp, "%s", json);
      fclose(fp);
      cJSON_free(json);
      return true;
    }

    static bool load(const char* filename, cJSON** data) {
      // if (!initSPIFFS()) return false;
      // load json to file using fopen function
      FILE *fp = fopen(filename, "r");
      if (fp == NULL) {
        Serial.println("ERROR: Could not open file for reading");
        return false;
      }
      fseek(fp, 0, SEEK_END);
      long size = ftell(fp);
      fseek(fp, 0, SEEK_SET);
      char* json = (char*)malloc(size+1);
      fread(json, 1, size, fp);
      json[size] = 0;
      fclose(fp);
      
      // File f = SPIFFS.open(filename, "r");
      // if (!f) return false;
      // size_t size = f.size();
      // char* json = (char*)malloc(size+1);
      // f.readBytes(json, size);
      // json[size] = 0;
      // f.close();
      *data = cJSON_Parse(json);
      free(json);
      if (!*data) {
        Serial.println("JSON parse error");
        return false;
      }
      return true;
    }
};

