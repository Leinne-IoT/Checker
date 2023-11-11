#pragma once

#include <nvs.h>
#include <string>

using namespace std;

namespace storage{
    static string deviceId = "";
    static nvs_handle_t nvsHandle;

    esp_err_t begin(){
        return nvs_open("checker", NVS_READWRITE, &nvsHandle);
    }

    string getString(string key, size_t length){
        char data[length] = {0};
        esp_err_t err = nvs_get_str(nvsHandle, key.c_str(), data, &length);
        if(err != ESP_OK){
            return "";
        }
        return string(data);
    }

    string getString(string key){
        uint16_t lenData;
        esp_err_t err = nvs_get_u16(nvsHandle, (key + "$l").c_str(), &lenData);
        if(err != ESP_OK){
            return "";
        }
        return getString(key, lenData);
    }

    bool setString(string key, string value, bool saveLength = true){
        if(saveLength && nvs_set_u16(nvsHandle, (key + "$l").c_str(), value.length() + 1) != ESP_OK){
            return false;
        }
        return nvs_set_str(nvsHandle, key.c_str(), value.c_str()) == ESP_OK;
    }

    string getDeviceId(){
        if(deviceId.length() > 0){
            return deviceId;
        }

        string id = getString("DEVICE_ID", 11);
        if(id.length() == 10){
            return deviceId = id;
        }else{
            stringstream stream;
            for(uint8_t i = 0; i < 5; ++i){
                stream << (char) random_int('a', 'z');
            }
            stream << '_';
            for(uint8_t i = 0; i < 4; ++i){
                stream << (char) random_int('0', '9');
            }
            setString("DEVICE_ID", deviceId = stream.str(), false);
        }
        return deviceId;
    }
}