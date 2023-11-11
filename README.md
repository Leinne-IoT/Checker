# Door/Window Open Checker
ESP32를 활용한 문 열림 감지 IoT 기기입니다.  
여러분의 문/창문의 상태를 언제 어디서든 확인보세요!

## 준비물
### 필수
1. WiFi 연결이 가능한 MCU(ex. ESP32)  
추천 기기: **XIAO ESP32S3**
2. 마그네틱 도어 센서(NO 스위치)
3. 리튬 배터리(+충전 모듈)  
배터리 모듈 비내장 기기 경우 **tp4056** 필요
### 선택
1. LED
2. 피에조 부저

## 주요 기능 구현 목록
* [x] **문 상태 확인**
* [x] **피에조 부저**
* [x] **LED 점등**
* [x] **배터리 잔량 확인**
* [x] **WebSocket 데이터 송/수신**

## FAQ
### "esp_websocket_client.h"를 찾을 수 없습니다
* [websocket-library](https://components.espressif.com/components/espressif/esp_websocket_client)를 lib폴더에 넣어주세요
### ESP8266으로는 불가능할까요?  
다음과 같은 이유로 비추천합니다
* 깊은 잠(Deep Sleep Mode)에서 GPIO로(RTC wake up) 깨울 수 없음
* 다중코어 활용 불가능으로 네트워크 통신중 다른기능이 중단됨