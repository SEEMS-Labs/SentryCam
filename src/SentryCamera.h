
// Include gaurd.
#ifndef SENTRY_CAMERA
#define SENTRY_CAMERA 

#include <cstdint>
#include <Arduino.h>
#include "esp_camera.h"
#include <Wire.h>
#include <WiFi.h>
#include <ESP32_NOW.h>
#include <esp_mac.h>
#include <vector>

// ESP Now stuff/
#define ESPNPW_WIFI_CHANNEL 6

// Camera Pin Defintions.
#define PWDN_GPIO_NUM  32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM  0
#define SIOD_GPIO_NUM  26
#define SIOC_GPIO_NUM  27

#define Y9_GPIO_NUM    35
#define Y8_GPIO_NUM    34
#define Y7_GPIO_NUM    39
#define Y6_GPIO_NUM    36
#define Y5_GPIO_NUM    21
#define Y4_GPIO_NUM    19
#define Y3_GPIO_NUM    18
#define Y2_GPIO_NUM    5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM  23
#define PCLK_GPIO_NUM  22

// 4 for flash led or 33 for normal led
#define LED_GPIO_NUM   4

extern String globalSSID;
extern String globalPassword;

class SentryCamera {

    private:
        const uint8_t cam_led = LED_GPIO_NUM;           // Camera flashlight pin.
        bool led_on = false;                            // Camera flashlight monitoring variable.
        String ipAddress = "NULL";                      // Ip address for camera once connected to internet.
        camera_config_t esp32_camera;                   // Configuration for the camera.    
        inline static String ssid = "EMPTY";            // Network id for internet.
        inline static String password = "EMPTY";        // Password for internet.
        bool ipShared = false;                          // Testing var. 
        
    public:
        SentryCamera() { 
            // Setup pin for flashlight.
            pinMode(cam_led, OUTPUT); 

            // Camera config
            esp32_camera.ledc_channel = LEDC_CHANNEL_0;
            esp32_camera.ledc_timer = LEDC_TIMER_0;
            esp32_camera.pin_d0 = Y2_GPIO_NUM;
            esp32_camera.pin_d1 = Y3_GPIO_NUM;
            esp32_camera.pin_d2 = Y4_GPIO_NUM;
            esp32_camera.pin_d3 = Y5_GPIO_NUM;
            esp32_camera.pin_d4 = Y6_GPIO_NUM;
            esp32_camera.pin_d5 = Y7_GPIO_NUM;
            esp32_camera.pin_d6 = Y8_GPIO_NUM;
            esp32_camera.pin_d7 = Y9_GPIO_NUM;
            esp32_camera.pin_xclk = XCLK_GPIO_NUM;
            esp32_camera.pin_pclk = PCLK_GPIO_NUM;
            esp32_camera.pin_vsync = VSYNC_GPIO_NUM;
            esp32_camera.pin_href = HREF_GPIO_NUM;
            esp32_camera.pin_sccb_sda = SIOD_GPIO_NUM;
            esp32_camera.pin_sccb_scl = SIOC_GPIO_NUM;
            esp32_camera.pin_pwdn = PWDN_GPIO_NUM;
            esp32_camera.pin_reset = RESET_GPIO_NUM;
            esp32_camera.xclk_freq_hz = 24 * 1000000;
            esp32_camera.frame_size = FRAMESIZE_CIF;       
            esp32_camera.pixel_format = PIXFORMAT_JPEG;
            esp32_camera.grab_mode = CAMERA_GRAB_LATEST;
            esp32_camera.fb_location = CAMERA_FB_IN_PSRAM;
            esp32_camera.jpeg_quality = 10;                 // Good balance: 20 = decent quality, lower memory
            esp32_camera.fb_count = 3;
        }

        // Camera functions.
        void initCamera();
        void setupWifi();
        void toggleFlashlight();

        // Call backs.
        static BaseType_t storeWiFiSsid(const char* new_ssid);
        static BaseType_t storeWifiPassword(const char *new_password);

        // Setters and getters for ssid, password, ip address.
        void setSSID(String ssid);
        String getSSID();
        void setPassword(String password);
        String getPassword();
        void setIpAddress(String ip);
        String getIpAddress();
        void setIpSharedState(bool state);
        bool getIpSharedState();
};

#endif /* SentryCamera.h*/