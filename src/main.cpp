#include "esp_camera.h"
#include "SentryCamera.h"
#include <WiFi.h>
#include "EspNowNode.h"
#include "app_httpd.hpp"

// Struct to control camera and esp now together;
struct _cam_module {
  SentryCamera *_cam;       // Sentry Camera.
  EspNowNode *_comms_node;  // Communication Module.
};
typedef struct _cam_module CamModule;

// Create a camera module. 
void fillCamModule(SentryCamera *camera, EspNowNode *node, CamModule *module);

// Watchdog task to sync the two objects together.
TaskHandle_t camera_watchdog_handle = NULL;
void camera_watchdog_task(void *pvParams);
void startCameraWatchdogTask(CamModule *module);

// Camera stuff.
SentryCamera sc;
EspNowNode sentry_cam_esp_now(SEEMS_PCB_V2_Untouched, false);
CamModule module;

void setup() {

  // Setup counter.
  Serial.begin(115200);    
  Serial.println("Entering SentryCam Setup.");
  for(int i = 0; i < 5; i++) {
    Serial.println(".");
    delay(500);
  }

  // Create the camera module.
  fillCamModule(&sc, &sentry_cam_esp_now, &module);

  // Begin the watchdog task.
  startCameraWatchdogTask(&module);

  // Initialize the camera communication system.
  String ip = "192.168.1.1";
  sentry_cam_esp_now.addInfoToSend(ip.c_str(), "");
  sentry_cam_esp_now.registerProcessWiFiPasswordCallBack(sc.storeWifiPassword);
  sentry_cam_esp_now.registerProcessWiFiSSIDCallBack(sc.storeWiFiSsid);
  sentry_cam_esp_now.start();
 
}

// Empty loop.
void loop() {}

// Create the camera watchdog task.
void startCameraWatchdogTask(CamModule *module) {
  BaseType_t res = xTaskCreatePinnedToCore(
    &camera_watchdog_task,    // Pointer to task function.
    "camera_watchdog_task",   // Task name.
    ESPNOW_TASK_DEPTH,        // Size of stack allocated to the task (in bytes).
    module,                     // Pointer to parameters used for task creation.
    1,                        // Task priority level.
    &camera_watchdog_handle,  // Pointer to task handle.
    1                         // Core that the task will run on.
  );
  if(res == pdFAIL) log_e("Failed to create Camera Watchdog Task.");
}

// Camera watchdog task function.
void camera_watchdog_task(void *pvParams) {
  // Setup.
  CamModule *module = static_cast<CamModule *>(pvParams);
  SentryCamera *camera = module->_cam;
  EspNowNode *commNode = module->_comms_node;
  String oldPassword = camera->getPassword();
  String oldSsid = camera->getSSID();
  String oldCameraIp = camera->getIpAddress();

  // Task Loop.
  for(;;) {
    // Wait indefinitely until password and ssid are changed via background tasks.
    while(oldPassword == camera->getPassword() || oldSsid == camera->getSSID()) {
      vTaskDelay(pdMS_TO_TICKS(500));
    }

    // Wait 10 seconds to allow Master a chance to receive ack and connect to WiFi.
    vTaskDelay(pdMS_TO_TICKS(10000));

    // Pause esp now tranmission once those things are changed and setup the camera web server.
    commNode->pause();

    // Initialize Sentry Camera Module and connect to Wi-Fi.
    camera->initCamera();
    log_e("init camera.");
    camera->setupWifi();
    log_e("start up wifi.");
    startCameraServer();
    log_e("start up cam server");

    // Check to see if camera ip has been altered.
    if(oldCameraIp != WiFi.localIP().toString()) {

      // Update camera IP.
      camera->setIpAddress(WiFi.localIP().toString().c_str());
      log_e("set ip address.");
      commNode->addInfoToSend(sc.getIpAddress().c_str(), "");

      // Reregister.
      commNode->reRegister();
      Serial.printf("WiFi.localIP().toString().c_str() = %s\n", WiFi.localIP().toString().c_str());
      log_e("sent over new up to esp now obj.");
    }
    else {
      commNode->addInfoToSend("Invalid", "IP");
      log_e("Updated to old AP.");
    }

    // Sanity delay.
    vTaskDelay(10000);
    
    // Unpause esp now transmisison and delete task.
    commNode->unpause();
    vTaskDelete(NULL);
  }

}

// Create camera module struct.
void fillCamModule(SentryCamera *camera, EspNowNode *node, CamModule *module) {
  module->_cam = camera;
  module->_comms_node = node;
}

