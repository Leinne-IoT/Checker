#include <nvs.h>
#include <random>
#include <string>

#include "storage.h"

using namespace std;

bool StorageClass::begin(string name){
    esp_err_t res = nvs_open(name.c_str(), NVS_READWRITE, &handle);
    if(res != ESP_OK){
        //log_e("Unable to open NVS namespace: %d", res);
        return false;
    }
    return true;
}

string StorageClass::getString(string key){
    uint16_t lenData;
    esp_err_t err = nvs_get_u16(handle, (key + "$l").c_str(), &lenData);
    if(err != ESP_OK){
        //log_e("Failed to load data '%s$l'. length: %d, error code: %s", key.c_str(), lenData, esp_err_to_name(err));
        return "";
    }
    return getString(key, lenData);
}

string StorageClass::getString(string key, size_t length){
    char data[length] = {0};
    esp_err_t err = nvs_get_str(handle, key.c_str(), data, &length);
    if(err != ESP_OK){
        //log_e("Failed to load data '%s'. length: %d, error code: %s", key.c_str(), lenData, esp_err_to_name(err));
        return "";
    }
    return string(data);
}

bool StorageClass::setString(string key, string value, bool saveLength){
    if(saveLength && nvs_set_u16(handle, (key + "$l").c_str(), value.length() + 1) != ESP_OK){
        return false;
    }
    return nvs_set_str(handle, key.c_str(), value.c_str()) == ESP_OK;
}