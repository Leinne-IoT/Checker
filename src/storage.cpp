#include <nvs.h>
#include <storage.h>
#include <random>
#include <string>

using namespace std;

bool StorageClass::begin(string name){
    esp_err_t res = nvs_open(name.c_str(), NVS_READWRITE, &handle);
    if(res != ESP_OK){
        //log_e("Unable to open NVS namespace: %d", res);
        return false;
    }
    return true;
}

string StorageClass::getDeviceId(){
    size_t length = 7;
    char data[length] = {0};
    esp_err_t err = nvs_get_str(handle, "DEVICE_ID", data, &length);
    if(err != ESP_OK){
        for(uint8_t i = 0; i < length - 1; ++i){
            data[i] = 'a'; // TODO: ESP32 random
        }
        data[6] = '\0';
        nvs_set_str(handle, "DEVICE_ID", data);
    }
    return string(data);
}

string StorageClass::getString(string key){
    uint16_t lenData;
    esp_err_t err = nvs_get_u16(handle, (key + "$l").c_str(), &lenData);
    if(err != ESP_OK){
        //log_e("Failed to load data '%s$l'. length: %d, error code: %s", key.c_str(), lenData, esp_err_to_name(err));
        return "";
    }

    size_t length = lenData;
    char data[length] = {0};
    err = nvs_get_str(handle, key.c_str(), data, &length);
    if(err != ESP_OK){
        //log_e("Failed to load data '%s'. length: %d, error code: %s", key.c_str(), lenData, esp_err_to_name(err));
        return "";
    }
    return string(data);
}

bool StorageClass::setString(string key, string value){
    if(
        key.length() < 1 ||
        value.length() < 1 ||
        nvs_set_u16(handle, (key + "$l").c_str(), value.length() + 1) != ESP_OK
    ){
        return false;
    }
    return nvs_set_str(handle, key.c_str(), value.c_str()) == ESP_OK;
}

StorageClass Storage;