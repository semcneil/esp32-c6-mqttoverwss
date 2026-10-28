/* secrets.h
   
  This is a place to store information not in the repository.

  Seth McNeill
  2026 August 17
*/

#define MOD_NAME "YourModule"
#define SSID "SSID"
#define WIFI_PWD "PASSWORD"
#define STATUS_DELAY 300000  // delay between sending status messages
#define MQTT_USER "mqtt_user"
#define MQTT_PWD "mqtt_password"
#define MQTT_SERVER "mqtt_server"
#define MQTT_PATH "/mqtt"
#define MQTT_TOPIC "test/topic"

// Let's Encrypt ISRG Root X1 CA Certificate (Clean, Multi-line Version)
// Clean, exact raw string representation of the Let's Encrypt Root CA
const char* root_ca = R"EOF(-----BEGIN CERTIFICATE-----
...
-----END CERTIFICATE-----)EOF";
