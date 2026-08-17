/*
  This script demonstrates MQTT connection over secure websockets (WSS)

  Seth McNeill
  2026 August 17
*/

#include <WiFi.h>
#include "mqtt_client.h" // Native Espressif MQTT framework
#include "time.h"
#include <esp_crt_bundle.h>

#include "secrets.h"

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

void setup() {
    Serial.begin(115200);
    delay(2000);

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
    mqtt_cfg.credentials.client_id = "esp32_c6_mqtt_client_01";
    
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
    if (mqttConnected && (currentMillis - lastPublishTime > 10000)) {
        lastPublishTime = currentMillis;
        
        String payload = "Hello from ESP32-C6 native asynchronous WSS!";
        Serial.printf("[STATUS]  Publishing message: '%s'\n", payload.c_str());
        
        // Publish through native stack
        esp_mqtt_client_publish(client, mqtt_topic, payload.c_str(), 0, 1, 0);
    }
    
    delay(10); // Yield to prevent watchdog starving
}