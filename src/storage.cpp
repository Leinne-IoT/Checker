#include <nvs.h>
#include <Arduino.h>
#include <storage.h>

bool StorageClass::begin(String name){
    esp_err_t res = nvs_open(name.c_str(), NVS_READWRITE, &handle);
    if(res != ESP_OK){
        log_e("Unable to open NVS namespace: %d", res);
        return false;
    }
    return true;
}

bool StorageClass::commit(){
    return nvs_commit(handle) == ESP_OK;
}

String StorageClass::getWiFiSSID(){
    return getString("wifi_ssid");
}

String StorageClass::getWiFiPassword(){
    return getString("wifi_pw");
}

void StorageClass::setWiFiData(String ssid, String pw){
    setString("wifi_pw", pw);
    setString("wifi_ssid", ssid);
}

String StorageClass::getDeviceId(){
    size_t length = 7;
    char data[length] = {0};
    esp_err_t err = nvs_get_str(handle, "DEVICE_ID", data, &length);
    if(err != ESP_OK){
        for(uint8_t i = 0; i < length - 1; ++i){
            data[i] = random('a', 'z');
        }
        data[6] = '\0';
        nvs_set_str(handle, "DEVICE_ID", data);
    }
    return String(data);
}

String StorageClass::getString(String key){
    if(key.length() < 1){
        return "";
    }

    uint16_t lenData;
    esp_err_t err = nvs_get_u16(handle, (key + "$l").c_str(), &lenData);
    if(err != ESP_OK){
        log_e("Failed to load data '%s$l'. length: %d, error code: %s", key.c_str(), lenData, esp_err_to_name(err));
        return "";
    }

    size_t length = lenData;
    char data[length] = {0};
    err = nvs_get_str(handle, key.c_str(), data, &length);
    if(err != ESP_OK){
        log_e("Failed to load data '%s'. length: %d, error code: %s", key.c_str(), lenData, esp_err_to_name(err));
        return "";
    }
    return String(data);
}

bool StorageClass::setString(String key, String value){
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