#include "EspNowNode.h"
#include <Arduino.h>
#include <ESP32_NOW.h>
#include <WiFi.h>
#include <esp_mac.h>

// Define task handles.
TaskHandle_t esp_now_tx_rx_handle = NULL;
TaskHandle_t esp_now_process_data_handle = NULL;

void esp_now_tx_rx_task(void *pvParams) {
    // Setup.
    EspNowNode *node = static_cast<EspNowNode *>(pvParams);
    int msg_count = 0;
    String str;
    int bufferSize = 100;
    char buffer[bufferSize];
    bool success = false;
    Header nextHeader;
    AckMessage nextAck;
    String nextData;

    // Task loop.
    for(;;) {

        // End if possible.
        if(node->credentialsPassedThrough() && !node->isTransmissionPaused()) {
            node->end();
        }

        // Ready to transmit.
        if(node->readyToTransmit() == true && !node->isTransmissionPaused()) {
            if(STATUS_PIN > 0) digitalWrite(STATUS_PIN, LOW);
            
            nextHeader = node->determineNextHeader();
            nextAck = node->determineNextAck();
            nextData = node->getNextData();
            
            Serial.println("------------------------------------------------");
            Serial.printf("WiFi Channel: %d\n",  WiFi.channel());
            Serial.printf("Header Transmitted: %d\n", nextHeader);
            Serial.printf("Ack Msg Transmitted: %c\n", nextAck);
            Serial.printf("Data Transmitted: %s\n", nextData.c_str());
            Serial.println("------------------------------------------------\n");
            
            node->reRegister();
            success = node->transmit(nextHeader, nextAck, nextData);
            if(success == false) {
                Serial.println("Failed Transmission");
                node->reRegister();
            }

        }
        
        // Ready to receive.
        else {
            if(STATUS_PIN > 0) digitalWrite(STATUS_PIN, HIGH);

            if(node->isNodeMaster()) Serial.println("Waiting for Acknowledgment From Slave(SentryCam)");
            else Serial.println("Waiting for acknowledgement from Master (Sentry)");
        }
        vTaskDelay(pdMS_TO_TICKS(TaskDelayLength));
    }
}

void esp_now_process_data_task(void *pvParams) {
    // Setup.
    EspNowNode *node = static_cast<EspNowNode *>(pvParams);
    bool success = false;

    // Task loop.
    for(;;) {
        // Wait for notifcation before processing data.
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);  
        
        // End if possible.
        if(node->credentialsPassedThrough()) {
            node->end();
        }
        
        // Call the proper call back based on data sent.
        //log_e("processing data called");
        success = node->callProcessDataCallback();
        if(success) node->setReadyToTransmit(true);
        //else log_e("Data processing failed.");
        
        // No need to delay due to blocking by notifcation waiting.
    }
}

bool EspNowNode::send_message() {
    bool res = true;
    if(!send((uint8_t *) &outgoingData, sizeof(ESP_NOW_PACKET))) {
        //log_e("Failed to broadcast message!");
        res = false;
    }
    return res;
}

void EspNowNode::initWifi() {
    WiFi.mode(WIFI_MODE_APSTA);
    WiFi.setChannel(ESPNOW_WIFI_CHANNEL);

    while(!WiFi.STA.started()) vTaskDelay(pdMS_TO_TICKS(100));

}

void EspNowNode::initESPNOW() {

    // Begin ESP NOW and add Peer to the network.
    bool success = true;
    if(!ESP_NOW.begin()) {
        //log_e("Failed to init ESP-NOW!");
        success = false;
    }
    if(!success || !add()) {
        //log_e("Failed to register broadcast peer!");
        success = false;
    } 

    if(!success) {
        Serial.println("Failed to initilize ESP NOW.");
        Serial.println("Rebooting");
        vTaskDelay(pdMS_TO_TICKS(5000));
        ESP.restart();
    }
    esp_now_setup = true;
    Serial.println("Per Has Begun Broadcasting");
    Serial.println("Communication info:");
    Serial.println("\t Mode: " + String(WiFi.getMode()));
    Serial.println("\t Node Mac Address: " + getThisMacAddress());
    Serial.println("\t Peer Mac Address: " + getPeerMacAddress());
    Serial.println("\t Channel: " + String(WiFi.channel()));
}

void EspNowNode::initTasks() {
    BaseType_t res = pdFAIL;

    // Begin the communication task.
    res = beginCommunicationTask();
    if(res != pdPASS) Serial.println("ESP Now Communication Task Not Started!");
    else Serial.println("ESP Now Communication Task Started Succesfully!");

    // Begin data processing task.
    res = beginProcessDataTask();
    if(res != pdPASS) Serial.println("ESP Now Process Data Task Not Started!");
    else Serial.println("ESP Now PRocess Data Task Started Succesfully!");
}

BaseType_t EspNowNode::beginCommunicationTask() {
    return xTaskCreatePinnedToCore(
        &esp_now_tx_rx_task,   // Pointer to task function.
        "communication_task",  // Task name.
        ESPNOW_TASK_DEPTH,       // Size of stack allocated to the task (in bytes).
        this,                   // Pointer to parameters used for task creation.
        1,           // Task priority level.
        &esp_now_tx_rx_handle, // Pointer to task handle.
        1                       // Core that the task will run on.
    );
}

BaseType_t EspNowNode::beginProcessDataTask() {
    return xTaskCreatePinnedToCore(
        &esp_now_process_data_task,     // Pointer to task function.
        "process_Data_task",            // Task name.
        ESPNOW_TASK_DEPTH,              // Size of stack allocated to the task (in bytes).
        this,                           // Pointer to parameters used for task creation.
        1,                              // Task priority level.
        &esp_now_process_data_handle,   // Pointer to task handle.
        1                               // Core that the task will run on.
    );
}

BaseType_t EspNowNode::slaveProcessAck(const char *data) {
    // Compare ack message with last header sent.
    Header headerTransmitted = outgoingData.header;
    Header headerReceived = incomingData.header;

    AckMessage ackMsgTransmitted = outgoingData.ack;
    AckMessage ackMsgReceived = incomingData.ack;

    // Headers + acks should match.
    bool headersMatch = (headerReceived == headerTransmitted);
    bool ackMsgsMatch = (ackMsgReceived == ackMsgTransmitted);

    //if(headersMatch == ackMsgsMatch) log_e("Processing Error. Headers or Acks matched.");
    return (headersMatch != ackMsgReceived) ? pdPASS : pdFAIL;
}

BaseType_t EspNowNode::masterProcessAck(const char * data) {
    // Compare ack message with last header sent.
    Header headerTransmitted = outgoingData.header;
    Header headerReceived = incomingData.header;

    AckMessage ackMsgTransmitted = outgoingData.ack;
    AckMessage ackMsgReceived = incomingData.ack;

    // Headers + acks should match.
    bool headersMatch = (headerReceived == headerTransmitted);
    bool ackMsgsMatch = (ackMsgReceived == ackMsgTransmitted);

    //if(headersMatch != ackMsgsMatch) log_e("Processing Error. Headers or Acks didn't match.");
    return (headersMatch && ackMsgReceived) ? pdPASS : pdFAIL;
    
}

BaseType_t EspNowNode::processHandshake(const char * data) {
    // Return pass always for now.
    return pdPASS;
}

void EspNowNode::buildTransmission(Header head, AckMessage ack, String data) {
    outgoingData.header = head;
    outgoingData.ack = ack;
    sprintf(outgoingData.data, "%s", data.c_str());
}

bool EspNowNode::registerProcessWiFiSSIDCallBack(ProcessDataCallback pcb) {
    // Only SentryCam needs this callback.
    if(isMaster == false) {
        processWiFiSSIDCallback = pcb;
        return true;
    }
    return false;
}

bool EspNowNode::registerProcessWiFiPasswordCallBack(ProcessDataCallback pcb) {
    // Only SentryCam needs this callback.
    if(isMaster == false) {
        processWiFiPasswordCallback = pcb;
        return true;
    }
    return false;
}

bool EspNowNode::registerProcessCameraIPCallBack(ProcessDataCallback pcb) {
    // Only Sentry needs this callback.
    if(isMaster == true) {
        processCameraIPCallback = pcb;
        return true;
    }
    return false;
}   

bool EspNowNode::start() {

    // Ensure proper callbacks are registered based on node type.
    bool success = false;
    success = (processAckCallback != NULL && processHandshakeCallback != NULL);
    if(isMaster) success &= (processCameraIPCallback != NULL);
    else success &= (processWiFiPasswordCallback != NULL && processWiFiSSIDCallback != NULL);

    success &= (strcmp(infoToSend.data1, infoToSend.data2) != 0 );

    // Only start if callbacks are all good.
    if(success) {
        initWifi();
        initESPNOW();
        initTasks();
    }
    //else log_e("ESP Now Not started. Ensure proper callbacks are registred");
    
    // Return.
    return success;
}

bool EspNowNode::end() {
    bool res = true;

    // Delete tasks to free up the scheduler.
    vTaskDelete(esp_now_tx_rx_handle);
    vTaskDelete(esp_now_process_data_handle);
    esp_now_tx_rx_handle = NULL;
    esp_now_process_data_handle = NULL;

    // Deinitialize ESP NOW.
    this->remove();
    esp_now_deinit();

    // Return.
    return res;
}

void EspNowNode::pause() { 
    isPaused = true; 
    log_e("Paused ESP NOW.");
}

void EspNowNode::unpause() { 
    isPaused = false; 
    log_e("Unaused ESP NOW.");
}

bool EspNowNode::isTransmissionPaused() { return isPaused; }

void EspNowNode::onReceive(const uint8_t *data, size_t len, bool broadcast) {

    // Print out.
    ESP_NOW_PACKET *dataReceived = (ESP_NOW_PACKET *) data;
    Serial.println("------------------------------------------------");
    Serial.printf("Header Received: %d\n", dataReceived->header);
    Serial.printf("Ack Msg Received: %c\n", dataReceived->ack);
    Serial.printf("Data Received: %s\n", dataReceived->data);
    Serial.println("------------------------------------------------\n");

    // Save data received.
    incomingData.header = dataReceived->header;
    incomingData.ack = dataReceived->ack;
    sprintf(incomingData.data, "%s", dataReceived->data);

    // Notify the process Data task.
    xTaskNotify(esp_now_process_data_handle, -1, eNoAction);
}

void EspNowNode::onSent(bool success) {
    this->waitingForData = true;
}

bool EspNowNode::is_esp_now_setup() { return esp_now_setup; }

bool EspNowNode::isNodeMaster() { return isMaster;}

bool EspNowNode::transmit(Header head, AckMessage ack, String data) { 
    buildTransmission(head, ack, data);
    int len = sizeof(ESP_NOW_PACKET);
    return send_message(); 
}

bool EspNowNode::readyToTransmit() { return !waitingForData; }

Header EspNowNode::getHeaderToProcess() { return incomingData.header; }

AckMessage EspNowNode::getAckToProcess() { return incomingData.ack; }

const char* EspNowNode::getDataToProcess() { return incomingData.data; }

void EspNowNode::setReadyToTransmit(bool status) {
    waitingForData = !status;
}

bool EspNowNode::callProcessDataCallback() {
    // Retreive data to deal with.
    BaseType_t res = pdFAIL;
    
    if(isMaster) res = callMasterProcessDataCallback();
    else res = callSlaveProcessDataCallback();
    
    // Clear the waiting for data flag and return.
    waitingForData = false;
    return (res == pdPASS);
}

// This call back is intended to swap from no Wifi mode to Wifi mode.
BaseType_t EspNowNode::masterProcessWifiPasswordReceived(const char *data) {
    if(!isMaster) {
        //log_e("Attempted Dynamic WiFi Connection by the Slave");
        return pdFAIL;
    }
    //else log_e("Attempting Dynamic WiFi Connection on The Master.");

    vTaskDelay(pdMS_TO_TICKS(TaskDelayLength * 5));

    // Grab WiFi Credentials.
    String ssid = infoToSend.data1;
    String password = infoToSend.data2;

    // Connect in Access Point/Station mode to facilitate ESP NOW.
    WiFi.mode(WIFI_MODE_APSTA);
    WiFi.begin(ssid, password);
    WiFi.setSleep(false);

    Serial.print("Dynimcally WiFi connecting");
    while (WiFi.status() != WL_CONNECTED) vTaskDelay(pdMS_TO_TICKS(500));
    Serial.println("");
    Serial.println("WiFi connected Dynmically.");

    vTaskDelay(TaskDelayLength * 5);
    return pdPASS;
}

BaseType_t EspNowNode::callMasterProcessDataCallback() {

    AckMessage ackToProcess = getAckToProcess();
    const char* dataToProcess = getDataToProcess();
    BaseType_t res = pdFAIL;

    // Select and fire the appropriate callback.
    switch (ackToProcess) {
        // Explicitly handle the receipt of camera IP address.
        case AckMessage::Received_Camera_IP : 
            if(processCameraIPCallback == NULL) {
                //log_e("Processed Camera IP Callback is NULL.");
                break;
            }
            res = processCameraIPCallback(dataToProcess);
            break;
        
        // Explicitly handle the receipt of Wi-Fi Password.
        case AckMessage::Received_WiFi_Password : 
            // Initiate connection to Wi-Fi at this point.
            if(processWiFiPasswordCallback == NULL) {
                //log_e("Processed WiFi Password Callback is NULL.");
                break;
            }
            res = processWiFiPasswordCallback(dataToProcess);
            break;

        // Generically handle other ACKs.
        default:
            if(processAckCallback == NULL) {
                //log_e("Processed Ack Callback is NULL.");
                break;
            }
            res = processAckCallback(dataToProcess);
            break;
    }

    // Return.
    return res;
}

void EspNowNode::reRegisterPeer() {
    bool removed = this->remove();
    //log_e("Peer remove status: %s", removed ? "success" : "failed");

    this->setChannel(WiFi.channel());

    bool added = this->add();
    //log_e("Peer add status: %s", added ? "success" : "failed");
}

BaseType_t EspNowNode::callSlaveProcessDataCallback() {

    Header headerToProcess = getHeaderToProcess();
    const char* dataToProcess = getDataToProcess();
    BaseType_t res = pdFAIL;

    // Select and fire the appropriate callback.
    switch (headerToProcess) {
        // Explicitly handle the receipt of WiFi SSID.
        case Header::WIFI_SSID : 
            if(processWiFiSSIDCallback == NULL) {
                //log_e("Processed WiFi SSID Callback is NULL.");
                break;
            }
            res = processWiFiSSIDCallback(dataToProcess);
            break;
        
        // Explicitly handle the receipt of WiFi Password.
        case Header::WIFI_PASSWORD : 
            if(processWiFiPasswordCallback == NULL) {
                //log_e("Processed WiFi Password Callback is NULL.");
                break;
            }
            res = processWiFiPasswordCallback(dataToProcess);
            break;
        
        // Generically handle other ACKs.
        default:
            if(processAckCallback== NULL) {
                //log_e("Processed Ack Callback is NULL.");
                break;
            }
            res = processAckCallback(dataToProcess);
            break;
    }

    // Return.
    return res;
}

void EspNowNode::addInfoToSend(const char *info1, const char *info2) {
    sprintf(infoToSend.data1, "%s", info1);
    Serial.printf("Info: %s\n", info1);
    Serial.printf("infoToSend.data1: \n", infoToSend.data1);
    sprintf(infoToSend.data2, "%s", info2);
}

// Only for Master.
Header EspNowNode::determineNextHeader() {

    Header prevHeaderSent = outgoingData.header;
    Header nextHeaderToSend = Header::HANDSHAKE;
    
    // Choose next header based on last header if node didn't just start.
    if(!justStarted) {
        switch(prevHeaderSent) {
        
            case Header::HANDSHAKE : 
                nextHeaderToSend = Header::WIFI_SSID;
                break;
            
            // Time to disconnect.
            case Header::WAVE :
                nextHeaderToSend = Header::HANDSHAKE;
                dataExchangeComplete = true;
                break;
    
            case Header::CAMERA_IP : 
                nextHeaderToSend = Header::WAVE;
                break;
    
            case Header::WIFI_SSID :
                nextHeaderToSend = Header::WIFI_PASSWORD;
                break;
    
            case Header::WIFI_PASSWORD : 
                nextHeaderToSend = Header::CAMERA_IP;
                break;
                
            default:
                Serial.println("You should never get here.");
                break;
        }
    }

    // Clear the just started flag for future pass throuhghs.
    else justStarted = false;

    // Return;
    return nextHeaderToSend;
}

// Only for Slave.
AckMessage EspNowNode::determineNextAck() {
    Header prevHeaderReceived = incomingData.header;
    AckMessage nextAckToSend = AckMessage::Received_Handshake;
    
    // Choose next header based on last header if node didn't just start.
    if(!justStarted) {
        if(this->isMaster) nextAckToSend = AckMessage::Received_Handshake;
        switch(prevHeaderReceived) {
        
            case Header::HANDSHAKE : 
                nextAckToSend = AckMessage::Received_Handshake;
                break;
            
            // Time to disconnect.
            case Header::WAVE :
                nextAckToSend = AckMessage::Received_Wave;
                dataExchangeComplete = true;
                break;
    
            case Header::CAMERA_IP : 
                nextAckToSend = AckMessage::Received_Camera_IP;
                break;
    
            case Header::WIFI_SSID :
                nextAckToSend = AckMessage::Received_WiFi_SSID;
                break;
    
            case Header::WIFI_PASSWORD : 
                nextAckToSend = AckMessage::Received_WiFi_Password;
                break;
                
            default:
                Serial.println("You should never get here.");
                break;
        }
    }

    // Clear the just started flag for future pass throuhghs.
    else justStarted = false;

    // Return;
    return nextAckToSend; 
}

String EspNowNode::getNextData() {
    
    String nextDataToSend = "";
    
    // Choose next header based on last header if node didn't just start.
    if(!justStarted) {
        if(isMaster) nextDataToSend = findMasterNextData();
        else nextDataToSend = findSlaveNextData();
    }

    // Clear the just started flag for future pass throuhghs.
    else justStarted = false;

    // Return;
    return nextDataToSend;
}

String EspNowNode::findMasterNextData() {
    AckMessage lastAckReceived = incomingData.ack;
    String nextDataToSend = "";

    switch(lastAckReceived) {
        case AckMessage::Received_Camera_IP :
            nextDataToSend = String(CIP_MSG);
            break;
        
        case AckMessage::Received_Handshake :
            nextDataToSend = String(infoToSend.data1);
            break;
        
        case AckMessage::Received_Wave :
            dataExchangeComplete = true;
            nextDataToSend = String(WV_MSG);
            break;

        case AckMessage::Received_WiFi_Password :
            nextDataToSend = String(CIP_MSG);
            break;

        case AckMessage::Received_WiFi_SSID :
            nextDataToSend = String(infoToSend.data2);
            break;

        default :
            Serial.println("Not suppose to reach this point.");
            break;
    }

    return nextDataToSend;
}

String EspNowNode::findSlaveNextData() {
    Header lastHeaderReceived = incomingData.header;
    String nextDataToSend = "";

    switch(lastHeaderReceived) {
            case Header::HANDSHAKE : 
                nextDataToSend = String(HS_MSG);
                break;
            
            // Time to disconnect.
            case Header::WAVE :
                nextDataToSend = String(WV_MSG);
                dataExchangeComplete = true;
                break;
    
            case Header::CAMERA_IP : 
                nextDataToSend = String(infoToSend.data1);
                break;
    
            case Header::WIFI_SSID :
                nextDataToSend = String(WS_MSG);
                break;
    
            case Header::WIFI_PASSWORD : 
                nextDataToSend = String(WP_MSG);
                break;
                
            default:
                Serial.println("You should never get here.");
                break;
        
    }

    return nextDataToSend;
}

bool EspNowNode::credentialsPassedThrough() { return dataExchangeComplete; }

void EspNowNode::reRegister() { reRegisterPeer(); }

String EspNowNode::getThisMacAddress() {
    char macStr[18] = {0};
    sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", 
                peerMacAddress[0],
                peerMacAddress[1], 
                peerMacAddress[2], 
                peerMacAddress[3], 
                peerMacAddress[4], 
                peerMacAddress[5]
            );
    return String(macStr);
}

String EspNowNode::getPeerMacAddress() {
    return String(WiFi.macAddress());
}