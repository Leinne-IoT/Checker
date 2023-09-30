#pragma once

#include <nvs.h>
#include <string>

using namespace std;

static string devicdId = "";
static nvs_handle_t handle;

namespace storage{
    esp_err_t begin(){
        esp_err_t res = nvs_open("checker", NVS_READWRITE, &handle);
        if(res != ESP_OK){
            //log_e("Unable to open NVS namespace: %d", res);
        }
        return res;
    }

    string getString(string key, size_t length){
        char data[length] = {0};
        esp_err_t err = nvs_get_str(handle, key.c_str(), data, &length);
        if(err != ESP_OK){
            //log_e("Failed to load data '%s'. length: %d, error code: %s", key.c_str(), lenData, esp_err_to_name(err));
            return "";
        }
        return string(data);
    }

    string getString(string key){
        uint16_t lenData;
        esp_err_t err = nvs_get_u16(handle, (key + "$l").c_str(), &lenData);
        if(err != ESP_OK){
            //log_e("Failed to load data '%s$l'. length: %d, error code: %s", key.c_str(), lenData, esp_err_to_name(err));
            return "";
        }
        return getString(key, lenData);
    }

    bool setString(string key, string value, bool saveLength = false){
        if(saveLength && nvs_set_u16(handle, (key + "$l").c_str(), value.length() + 1) != ESP_OK){
            return false;
        }
        return nvs_set_str(handle, key.c_str(), value.c_str()) == ESP_OK;
    }

    string getDeviceId(){
        if(devicdId.length() == 6){
            return devicdId;
        }

        string id = getString("DEVICE_ID", 7);
        if(id.length() == 6){
            return devicdId = id;
        }else{
            stringstream stream;
            for(uint8_t i = 0; i < 6; ++i){
                stream << (char) random('a', 'z');
            }
            setString("DEVICE_ID", devicdId = stream.str(), false);
        }
        return devicdId;
    }
}