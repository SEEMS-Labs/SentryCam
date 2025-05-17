#include <SentryCamera.h>

String globalSSID = "EMPTY";
String globalPassword = "EMPTY";

void SentryCamera::initCamera() {
    esp_err_t err = esp_camera_init(&esp32_camera);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x", err);
        return;
  }
}

void SentryCamera::setupWifi() {
    WiFi.mode(WIFI_MODE_APSTA);
    WiFi.begin(ssid, password);
    WiFi.setSleep(false);

    Serial.print("WiFi connecting");
    while (WiFi.status() != WL_CONNECTED) vTaskDelay(pdMS_TO_TICKS(500));
    Serial.println("");
    Serial.println("WiFi connected");
}

void SentryCamera::toggleFlashlight() {
    if(led_on) digitalWrite(cam_led, LOW);
    else digitalWrite(cam_led, HIGH);
    led_on ^= 1;
}

void SentryCamera::setSSID(String ssid) { this->ssid = ssid; }
String SentryCamera::getSSID() {return ssid; }
void SentryCamera::setPassword(String password) {this->password = password; }
String SentryCamera::getPassword() { return password; }
void SentryCamera::setIpAddress(String ip) { ipAddress = String(ip); }
String SentryCamera::getIpAddress() { return ipAddress; }
void SentryCamera::setIpSharedState(bool state) { ipShared = state; }
bool SentryCamera::getIpSharedState() { return ipShared; }

BaseType_t SentryCamera::storeWiFiSsid(const char* new_ssid) {
    ssid = String(new_ssid);
    return pdPASS;
}

BaseType_t SentryCamera::storeWifiPassword(const char *new_password) {
    password = String(new_password);
    return pdPASS;
}   