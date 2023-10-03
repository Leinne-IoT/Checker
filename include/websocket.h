#pragma once

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
    bool connectServer = false;
    esp_websocket_client_handle_t webSocket = NULL;

    void sendDoorState(DoorState state){
        uint8_t len = 3;
        uint8_t buffer[32] = {0x00, 0x02, state.open};
        int64_t current = millis() - state.updateTime;
        for(uint8_t max = len + 8; len < max; ++len){
            buffer[len] = (current >> (8 * (max - len - 1))) & 0xFF;
        }
        esp_websocket_client_send_with_opcode(webSocket, WS_TRANSPORT_OPCODES_BINARY, buffer, len, portMAX_DELAY);
    }

    void sendWelcome(){
        uint8_t len = 2;
        uint8_t buffer[32] = {0x00, 0x01};
        auto device = storage::getDeviceId();
        for(uint8_t first = len, max = len + device.length(); len < max; ++len){
            buffer[len] = device[len - first];
        }
        esp_websocket_client_send_with_opcode(webSocket, WS_TRANSPORT_OPCODES_BINARY, buffer, len, portMAX_DELAY);
    }

    bool isConnected(){
        return esp_websocket_client_is_connected(webSocket);
    }

    static void eventHandler(void* object, esp_event_base_t base, int32_t eventId, void* eventData){
        esp_websocket_event_data_t* data = (esp_websocket_event_data_t*) eventData;
        if(eventId == WEBSOCKET_EVENT_CONNECTED){
            sendWelcome();
        }else if(eventId == WEBSOCKET_EVENT_DISCONNECTED || eventId == WEBSOCKET_EVENT_ERROR){
            connectServer = false;
        }else if(eventId == WEBSOCKET_EVENT_DATA){
            switch(data->op_code){
                case STRING:{
                    if(connectServer){
                        break;
                    }
                    string device(data->data_ptr, data->data_len);
                    if(storage::getDeviceId() == device){
                        connectServer = true;
                        debug("[WS] Connect successful.\n");
                    }else{
                        debug("[WS] Connect failed.");
                    }
                    break;
                }
                case BINARY:
                    // TODO: 웹소켓 데이터 수신으로 LED, 부저 등 제어
                    switch(data->data_ptr[0]){
                        case 0x10: // LED LIGHT
                            gpio_set_level(LED_BUILTIN, data->data_ptr[1]);
                            debug("[WS] LED 점등\n");
                            break;
                    }
                    break;
                case QUIT:
                    debug("[WS] Received closed message with code=%d\n", 256 * data->data_ptr[0] + data->data_ptr[1]);
                    break;
            }
        }
    }

    void start(esp_event_handler_t handler){
        auto url = storage::getString("websocket_url");
        if(url.find_first_of("ws") == string::npos){
            storage::setString("websocket_url", url = "ws://leinne.net:33877/");
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