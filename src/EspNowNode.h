#ifndef ESP_NOW_NODE
#define ESP_NOW_NODE

#include <Arduino.h>
#include <ESP32_NOW.h>
#include <WiFi.h>
#include <esp_mac.h>

#define STATUS_PIN 4
#define TaskDelayLength pdMS_TO_TICKS(2000)

typedef BaseType_t (* ProcessDataCallback)(const char *);

const uint8_t ESPNOW_WIFI_CHANNEL = 6;
const uint8_t ESPNOW_DATA_SIZE = 64;
const int ESPNOW_TASK_DEPTH = 8192;

// ESP32-S3 Mac addrresses.
const uint8_t dev_S3_A[] = {0x24, 0xEC, 0x4A, 0x09, 0xC8, 0x00};
const uint8_t dev_S3_B[] = {0x24, 0xEC, 0x4A, 0x09, 0xC8, 0x00};
const uint8_t Seems_PCB_V1[] = {0xFC, 0xE8, 0xC0, 0xCD, 0xA3, 0xCC};
const uint8_t Seems_PCB_V2[] = {0x24, 0xEC, 0x4A, 0x09, 0xC8, 0x00};
const uint8_t Seems_PCB_V2_Weeks[]  = {0xD8, 0x3B, 0xDA, 0x72, 0xD5, 0x80};
const uint8_t SEEMS_PCB_V2_Untouched[] = {0x48, 0xCA, 0x43, 0x09, 0x5E, 0x70};

// ESP32-CAM Mac addresses.
const uint8_t dev_Cam_A[] = {0xFC, 0xE8, 0xC0, 0xCD, 0xA3, 0xCC};
const uint8_t dev_Cam_B[] = {0xFC, 0xE8, 0xC0, 0xCD, 0xA3, 0xCC};

enum _header : uint8_t {
    ACK,
    HANDSHAKE,
    WIFI_SSID,
    WIFI_PASSWORD,
    CAMERA_IP,
    WAVE,
    PING
};
typedef enum _header Header;

enum _ack_messages : char {
    Received_Handshake = 'H',
    Received_WiFi_Password = 'P',
    Received_WiFi_SSID = 'S',
    Received_Camera_IP = 'C',
    Received_Wave = 'W',
    Received_Ping = 'G'
};
typedef enum _ack_messages AckMessage;

#define HS_MSG "Received Handshake Request"
#define WP_MSG "Received WiFi password Request"
#define WS_MSG "Recevied Wifi SSID Request"
#define CIP_MSG "Received Camera IP Request"
#define WV_MSG "Received Wave Request"

struct _network_info {
    char data1[ESPNOW_DATA_SIZE];
    char data2[ESPNOW_DATA_SIZE];
};
typedef struct _network_info NetworkInfo;

struct _esp_now_packet {
    Header header;                  // Holds info on the kind of data being exchanged.
    AckMessage ack;                 // Holds info on the kind of data last received by the sending node.
    char data[ESPNOW_DATA_SIZE];    // Holds data currently being exchanged.
};
typedef struct _esp_now_packet ESP_NOW_PACKET;

extern TaskHandle_t esp_now_tx_rx_handle;
extern TaskHandle_t esp_now_process_data_handle;
extern TaskHandle_t esp_now_broadcast_handle;

void esp_now_tx_rx_task(void *pvParams);
void esp_now_process_data_task(void *pvParams);
void esp_now_broadcast_task(void *pvParams);

class EspNowNode : ESP_NOW_Peer {
    private:
        inline static bool isMaster = false;
        bool esp_now_setup = false;
        bool waitingForData = false;
        bool justStarted = true;
        bool dataExchangeComplete = false;
        bool isPaused = false;
        bool hasFoundPeer = false;
        
        uint8_t peerMacAddress[6];
        inline static NetworkInfo infoToSend;
        inline static ESP_NOW_PACKET outgoingData;
        inline static ESP_NOW_PACKET incomingData;

        void initWifi();
        void initESPNOW();
        bool send_message();
        void buildTransmission(Header head, AckMessage ack, String data);
        void reRegisterPeer();

        Header getHeaderToProcess();
        AckMessage getAckToProcess();
        const char* getDataToProcess();

        void initTasks();
        BaseType_t beginCommunicationTask();
        BaseType_t beginProcessDataTask();
        static BaseType_t slaveProcessAck(const char *data);
        static BaseType_t masterProcessAck(const char *data);
        static BaseType_t processHandshake(const char* data);
        static BaseType_t masterProcessWifiPasswordReceived(const char *data);
        BaseType_t callMasterProcessDataCallback();
        BaseType_t callSlaveProcessDataCallback();

        String findMasterNextData();
        String findSlaveNextData();

        ProcessDataCallback processAckCallback = NULL;
        ProcessDataCallback processHandshakeCallback = processHandshake;
        ProcessDataCallback processWiFiSSIDCallback = NULL;
        ProcessDataCallback processWiFiPasswordCallback = NULL;
        ProcessDataCallback processCameraIPCallback = NULL;

    public:
        EspNowNode( 
                const uint8_t* peerMacAddress,
                bool masterMode
            ) :
            ESP_NOW_Peer(peerMacAddress, ESPNOW_WIFI_CHANNEL, WIFI_IF_STA, NULL)
        {   
            // Set the peer mac address.
            for(int i = 0; i < 6; i++) this->peerMacAddress[i] = peerMacAddress[i];

            // Initialize outgoing data packet. First to be transmitted by the Master. 
            outgoingData.header = Header::HANDSHAKE;
            outgoingData.ack = AckMessage::Received_Handshake;
            String tmp = "Nothing outgoing";
            sprintf(outgoingData.data, "%s", tmp.c_str());
            
            // Initialize incoming data packet.
            incomingData.header = Header::HANDSHAKE;
            incomingData.ack = AckMessage::Received_Handshake;
            tmp = "Nothing incoming";
            sprintf(incomingData.data, "%s", tmp.c_str());

            // Initialize info packet.
            sprintf(infoToSend.data1, "%s", "Empty");
            sprintf(infoToSend.data2, "%s", "Empty");

            // Select mode. Master begins ready to transmit. Slave begins waiting.
            isMaster = masterMode;
            waitingForData = (masterMode) ? false : true;

            // Register ack processing callback based on mode.
            processAckCallback = (masterMode) ? masterProcessAck : slaveProcessAck;
            processWiFiPasswordCallback = (masterMode) ? masterProcessWifiPasswordReceived : NULL;
        }
        
        ~EspNowNode() { remove(); }
        
        bool registerProcessWiFiSSIDCallBack(ProcessDataCallback pcb);
        bool registerProcessWiFiPasswordCallBack(ProcessDataCallback pcb);
        bool registerProcessCameraIPCallBack(ProcessDataCallback pcb);

        bool start();
        bool end();
        void pause();
        void unpause();
        bool isTransmissionPaused();
        
        void onReceive(const uint8_t *data, size_t len, bool broadcast) override;
        void onSent(bool success) override;
        bool is_esp_now_setup();
        bool isNodeMaster();

        bool transmit(Header head, AckMessage ack, String data);
        bool readyToTransmit(); 
        void setReadyToTransmit(bool status);
        Header determineNextHeader();
        AckMessage determineNextAck();
        String getNextData();

        bool callProcessDataCallback();
        void addInfoToSend(const char *info1, const char *info2);
        bool credentialsPassedThrough();
        void reRegister();
        String getThisMacAddress();
        String getPeerMacAddress();
    };


#endif