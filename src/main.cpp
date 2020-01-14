#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

#include "secrets.h"
#include "sensor-utils.h"

#define CFG_DEVICE_TYPE "ESP32_DEVKITC"
#define CFG_CLIENT_ID 30

#define FIRMWARE_NAME "Environment Sensor"
#define FIRMWARE_SLUG "display_co2-ESP32_DEVKITC-arduino"
#define FIRMWARE_VERSION "0.2"

String device_id;
String make_device_id(String device_type, int id);

// WiFi / MQTT configuration
const char *ssid = SSID;
const char *passwd = PASSWD;

IPAddress mqtt_server(192, 168, 1, 30);
WiFiClient espClient;
PubSubClient mqttClient(espClient);

void setup_wifi();
void reconnect();

void setup()
{
  Serial.begin(115200);
  delay(2000);

  device_id = make_device_id(CFG_DEVICE_TYPE, CFG_CLIENT_ID); // TODO refactor this into printBanner
  utils::printBanner(FIRMWARE_NAME, FIRMWARE_SLUG, FIRMWARE_VERSION, device_id.c_str());

  setup_wifi();
  mqttClient.setServer(mqtt_server, 1883);
}

void loop()
{
  mqttClient.loop();
}

String make_device_id(String device_type, int id)
{
  return device_type + "-" + id;
}

/************** WiFi / MQTT  *******************************************************************/

void setup_wifi()
{

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(SSID);

  WiFi.begin(SSID, PASSWD);

  while (WiFi.status() != WL_CONNECTED)
  {
    // statusLED.flash(wifi_colour, 50);
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect()
{
  // Loop until we're reconnected
  while (!mqttClient.connected())
  {
    // statusLED.flash(error_colour, 50);

    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqttClient.connect(device_id.c_str()))
    {
      Serial.println("connected");
      // Once connected, publish an announcement...
      mqttClient.publish("outTopic", "hello world");
      // ... and resubscribe
      mqttClient.subscribe("inTopic");
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void publish_float(int client, char *topic, float val)
{
  char val_buf[10];
  char topic_buf[20];

  sprintf(topic_buf, "%d/%s/", client, topic);
  sprintf(val_buf, "%.1f", val);

  mqttClient.publish(topic_buf, val_buf);
}

void publish_int(int client, char *topic, int val)
{
  char val_buf[10];
  char topic_buf[20];

  sprintf(topic_buf, "%d/%s/", client, topic);
  sprintf(val_buf, "%d", val);

  mqttClient.publish(topic_buf, val_buf);
}

/***********************************************************************************************/