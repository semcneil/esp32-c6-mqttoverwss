/*
  This script demonstrates MQTT connection over secure websockets (WSS)

  Seth McNeill
  2026 August 17
*/

#include <WiFi.h>
#include "mqtt_client.h" // Native Espressif MQTT framework
#include "time.h"
#include <esp_crt_bundle.h>
#include <esp_mac.h>  // to get the full 8-byte MAC address

#include "secrets.h"

#define STATUS_DELAY 300000  // delay between sending status messages

// Allocate an 8-byte array for the EUI-64 / IEEE 802.15.4 ID
uint8_t mac8[8];
String globalDeviceId = "ID NOT SET";
String moduleName = MOD_NAME;

// --- CONFIGURATION ---
const char* ssid     = SSID;
const char* password = WIFI_PWD;

const char* mqtt_host  = MQTT_SERVER;
const int   mqtt_port  = 443;
const char* mqtt_path  = MQTT_PATH;
const char* mqtt_topic = MQTT_TOPIC;

// --- AUTHENTICATION ---
const char* mqtt_user = MQTT_USER;
const char* mqtt_pass = MQTT_PWD;

// --- GLOBAL OBJECTS ---
esp_mqtt_client_handle_t client;
unsigned long lastPublishTime = 0;
bool mqttConnected = false;

// --- NATIVE ESPRESSIF MQTT EVENT HANDLER ---
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    auto event = (esp_mqtt_event_handle_t)event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            Serial.println("\n[SUCCESS] Native MQTT Connected over WSS Layer!");
            mqttConnected = true;
            // Subscribe cleanly using the native API
            esp_mqtt_client_subscribe(client, mqtt_topic, 1);
            Serial.printf("[STATUS]  Subscribed to topic: %s\n", mqtt_topic);
            break;
            
        case MQTT_EVENT_DISCONNECTED:
            Serial.println("[ERROR]   MQTT Disconnected from server.");
            mqttConnected = false;
            break;
            
        case MQTT_EVENT_DATA:
            Serial.println("\n--- NEW MESSAGE RECEIVED ---");
            Serial.printf("Topic:   %.*s\n", event->topic_len, event->topic);
            Serial.printf("Payload: %.*s\n", event->data_len, event->data);
            Serial.println("----------------------------\n");
            break;
            
        case MQTT_EVENT_ERROR:
            Serial.println("[ERROR]   Internal Espressif MQTT protocol error occurred.");
            if (event->error_handle != NULL) {
                if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
                    Serial.printf("[ERROR]   Broker refused connection. CONNACK code: %d\n", 
                                  event->error_handle->connect_return_code);
                }
            }
            break;
            
        default:
            break;
    }
}

// Function that fetches the 8-byte ID and returns it as a formatted String
String getUniqueIdString() {
  uint8_t mac8[8];
  char buffer[24]; // Big enough to hold "XX:XX:XX:XX:XX:XX:XX:XX\0"
  
  if (esp_read_mac(mac8, ESP_MAC_IEEE802154) == ESP_OK) {
    snprintf(buffer, sizeof(buffer), "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
             mac8[0], mac8[1], mac8[2], mac8[3], 
             mac8[4], mac8[5], mac8[6], mac8[7]);
    return String(buffer);
  }
  
  return String("ERROR_READING_ID");
}

void setup() {
    Serial.begin(115200);
    delay(2000);

    globalDeviceId = getUniqueIdString();
    Serial.println("ID: " + globalDeviceId);

    // 1. Connect to Wi-Fi
    Serial.printf("[STATUS]  Connecting to Wi-Fi: %s\n", ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\n[SUCCESS] Wi-Fi Connected!");

    // 2. Sync System Clock via NTP
    Serial.println("[STATUS]  Syncing time with NTP server...");
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    time_t now = time(nullptr);
    while (now < 8 * 3600 * 2) {
        delay(500);
        Serial.print(".");
        now = time(nullptr);
    }
    Serial.println("\n[SUCCESS] Time synchronized!");
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)) {
        Serial.println("Failed to get time");
    } else {
        Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
    }

    // 3. Configure Native WSS MQTT Engine via Explicit Fields
    esp_mqtt_client_config_t mqtt_cfg = {};
    
    // Explicitly define address and transport. 
    // This bypasses string parsing bugs and guarantees Cloudflare gets the SNI.
    mqtt_cfg.broker.address.hostname = mqtt_host;
    mqtt_cfg.broker.address.transport = MQTT_TRANSPORT_OVER_WSS;
    mqtt_cfg.broker.address.port = mqtt_port;
    
    // NOTE: Ensure your MQTT_PATH variable in secrets.h starts with a forward slash (e.g., "/ws")
    mqtt_cfg.broker.address.path = mqtt_path; 

    // Authentication parameters
    mqtt_cfg.credentials.username = mqtt_user;
    mqtt_cfg.credentials.authentication.password = mqtt_pass;
    char client_id[40];
    snprintf(client_id, sizeof(client_id), "esp32_c6_%s", globalDeviceId.c_str());
    mqtt_cfg.credentials.client_id = client_id;
    
    // Security and Certificate Verification
    // Attach the built-in root CA bundle so it trusts Cloudflare's certificates
    mqtt_cfg.broker.verification.crt_bundle_attach = esp_crt_bundle_attach;
    
    // Cloudflare certificates are generally standard, but leaving these skips
    // as you had them is fine for getting the initial connection working.
    // mqtt_cfg.broker.verification.skip_cert_common_name_check = true;
    // mqtt_cfg.broker.verification.use_global_ca_store = false; 
    
    // #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
      // mqtt_cfg.broker.verification.common_name = NULL;
    // #endif

    Serial.println("[STATUS]  Initializing Native Espressif MQTT client...");
    client = esp_mqtt_client_init(&mqtt_cfg);
    
    // Register our callback loop handler
    esp_mqtt_client_register_event(client, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    
    // Fire up the asynchronous networking thread
    Serial.println("[STATUS]  Starting background WSS connection thread...");
    esp_mqtt_client_start(client);
}

void loop() {
    unsigned long currentMillis = millis();
    if (mqttConnected && ((currentMillis - lastPublishTime > STATUS_DELAY) || lastPublishTime == 0)) {
        // String payload = "Hello from ESP32-C6 native asynchronous WSS!";
        struct tm timeinfo;
        if (!getLocalTime(&timeinfo)) {
            Serial.println("Failed to obtain time");
        }

        char timeBuffer[20];
        
        // tm_year is years since 1900, tm_mon is 0-indexed (0 = January)
        sprintf(timeBuffer, "%04d-%02d-%02d %02d:%02d:%02d",
                timeinfo.tm_year + 1900,
                timeinfo.tm_mon + 1,
                timeinfo.tm_mday,
                timeinfo.tm_hour,
                timeinfo.tm_min,
                timeinfo.tm_sec);
        String payload = "ESP32-C6," + globalDeviceId + "," + String(timeBuffer) + "," + String(currentMillis);
        if(lastPublishTime == 0) {
            payload += ",boot";
        }
        Serial.printf("[STATUS]  Publishing message: '%s'\n", payload.c_str());
        
        // Publish through native stack
        esp_mqtt_client_publish(client, mqtt_topic, payload.c_str(), 0, 1, 0);
        lastPublishTime = currentMillis;
    }
    
    delay(10); // Yield to prevent watchdog starving
}