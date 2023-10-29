#pragma once

#include <atomic>
#include <driver/gpio.h>
#include <esp_http_client.h>
#include <esp_websocket_client.h>

#include "door.h"
#include "utils.h"
#include "storage.h"

typedef enum{
    CONTINUITY,
    STRING,
    BINARY,
    QUIT = 0x08,
    PING,
    PONG
} websocket_opcode_t;

namespace ws{
    atomic<bool> sendPhase = false;
    atomic<bool> connectServer = false;
    esp_websocket_client_handle_t webSocket = NULL;

    void sendWelcome(){
        auto device = storage::getDeviceId();
        uint8_t buffer[16] = {0x01, 0x01, getBatteryLevel()}; //{device type, data type, battery level}
        for(uint8_t i = 0; i< device.length(); ++i){
            buffer[3 + i] = device[i];
        }
        esp_websocket_client_send_with_opcode(webSocket, WS_TRANSPORT_OPCODES_BINARY, buffer, device.length() + 3, portMAX_DELAY);
        printf("[WS] Send welcome message\n");
    }

    void sendDoorState(DoorState state){
        sendPhase = true;
        printf("[WS] Start send(%s).\n", state.open ? "open" : "close");
        uint8_t buffer[7] = {0x01, 0x02, getBatteryLevel()}; //{device type, data type, open:1 battery level:7}
        buffer[2] = state.open ? buffer[2] | 0b10000000 : buffer[2] & 0b01111111;
        uint8_t count = 0;
        int64_t delay = 0;
        do{
            if(millis() - delay < 1000){
                continue;
            }

            if(++count > 3){
                printf("[WS] Send failed.\n");
                return;
            }
            delay = millis();
            int64_t current = millis() - state.updateTime;
            for(uint8_t byte = 0; byte < 4; ++byte){
                buffer[3 + byte] = (current >> 8 * (3 - byte)) & 0b11111111;
            }
            esp_websocket_client_send_with_opcode(webSocket, WS_TRANSPORT_OPCODES_BINARY, buffer, 7, portMAX_DELAY);
        }while(sendPhase);
    }

    bool isConnected(){
        return esp_websocket_client_is_connected(webSocket);
    }

    static void eventHandler(void* object, esp_event_base_t base, int32_t eventId, void* eventData){
        esp_websocket_event_data_t* data = (esp_websocket_event_data_t*) eventData;
        if(eventId == WEBSOCKET_EVENT_CONNECTED){
            sendWelcome();
        }else if(eventId == WEBSOCKET_EVENT_DISCONNECTED || eventId == WEBSOCKET_EVENT_ERROR){
            if(connectServer){
                printf("[WS] Disconnected WebSocket\n");
            }
            connectServer = false;
        }else if(eventId == WEBSOCKET_EVENT_DATA){
            if(data->op_code == STRING && !connectServer){
                string device(data->data_ptr, data->data_len);
                if(storage::getDeviceId() == device){
                    connectServer = true;
                    printf("[WS] Connect successful.\n");
                }else{
                    printf("[WS] FAILED. device: %s, receive: %s, len: %d\n", storage::getDeviceId().c_str(), device.c_str(), data->data_len);
                }
            }else if(data->op_code == BINARY){
                sendPhase = false;
            }
        }
    }

    void start(esp_event_handler_t handler){
        auto url = storage::getString("websocket_url");
        if(url.find_first_of("ws") == string::npos){
            storage::setString("websocket_url", url = DEFAULT_WEBSOCKET_URL);
        }

        esp_websocket_client_config_t websocket_cfg = {
            .uri = url.c_str(),
            .reconnect_timeout_ms = 250,
        };

        while(webSocket == NULL){
            webSocket = esp_websocket_client_init(&websocket_cfg);
        }
        esp_websocket_register_events(webSocket, WEBSOCKET_EVENT_ANY, handler, NULL);
        esp_websocket_register_events(webSocket, WEBSOCKET_EVENT_ANY, eventHandler, NULL);

        esp_err_t err = ESP_FAIL;
        while(err != ESP_OK){
            err = esp_websocket_client_start(webSocket);
        }
    }
}