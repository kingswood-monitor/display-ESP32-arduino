// #define BLYNK_DEBUG // Optional, this enables lots of prints
#define BLYNK_PRINT Serial

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>

#include <PubSubClient.h>
#include <Preferences.h>
#include "FastLED.h"

#include "DisplayLED.h"
#include "sensor-utils.h"
#include "secrets.h"

#define FIRMWARE_NAME "Environment Sensor"
#define FIRMWARE_VERSION "0.2"
#define FIRMWARE_SLUG "display-ESP32_DEVKITC-arduino"

/*************************** Configuration Settings *******************************************************
 *  Configuration settings stored in permanent FLASH memory                            
 */
#define WRITE_SETTINGS false

#define CFG_DEVICE_TYPE "ESP32_DEVKITC"
#define CFG_CLIENT_ID 30 // 3X = display nodes
#define CFG_NUM_LEDS 2   // Number of active LEDs in display

Preferences preferences;
String cfg_device_type;
int cfg_client_id;
int cfg_num_leds;
/**********************************************************************************************************/

// MQTT
IPAddress mqtt_server(192, 168, 1, 30);
WiFiClient espClient;
PubSubClient mqttClient(espClient);
void mqtt_callback(char *topic, byte *payload, unsigned int length);

char info_topic[20];
char co2_topic[20];
char temp_topic[20];
char humidity_topic[20];
char light_topic[20];
char power_topic[20];

// DisplayLED
DisplayLED displayLED;
CRGB data_colour = CRGB::White;
CRGB wifi_colour = CRGB::Blue;
CRGB error_colour = CRGB::Red;

char device_id[20];

int colour_index = 0;
int co2 = 0;
float temp = 0.0;
int humidity = 0;
int light = 0;
int power = 0;

typedef struct
{
  int min;
  int max;
} display_range_t;

display_range_t co2_display_range = (display_range_t){.min = 450, .max = 1200};

BlynkTimer reconnect_mqtt_timer;
BlynkTimer breather_timer;

void reconnect_mqtt();
void colourChange();
void make_topic(char *buf, int client_id, char *topic);

void setup()
{
  Serial.begin(115200);
  delay(2000);

  //preferences **************************************************************************
  preferences.begin("display", false);

  if (WRITE_SETTINGS)
  {
    preferences.putString("device_type", CFG_DEVICE_TYPE);
    preferences.putInt("client_id", CFG_CLIENT_ID);
    preferences.putBool("num_leds", CFG_NUM_LEDS);

    cfg_device_type = CFG_DEVICE_TYPE;
    cfg_client_id = CFG_CLIENT_ID;
    cfg_num_leds = CFG_NUM_LEDS;
  }
  else
  {
    cfg_device_type = preferences.getString("device_type", CFG_DEVICE_TYPE);
    cfg_client_id = preferences.getInt("client_id", -1);
    cfg_num_leds = preferences.getBool("num_leds", false);
  }

  // preferences ************************************************************************

  displayLED.off(1);

  make_topic(info_topic, cfg_client_id, "info/status");
  make_topic(co2_topic, cfg_client_id, "data/co2");
  make_topic(temp_topic, cfg_client_id, "temp/co2");
  make_topic(humidity_topic, cfg_client_id, "humidity/co2");
  make_topic(light_topic, cfg_client_id, "light/co2");
  make_topic(power_topic, cfg_client_id, "power/co2");

  sprintf(device_id, "%s-%02d", cfg_device_type.c_str(), cfg_client_id);
  utils::printBanner(FIRMWARE_NAME, FIRMWARE_SLUG, FIRMWARE_VERSION, device_id);

  // setup_wifi();
  mqttClient.setServer(mqtt_server, 1883);
  mqttClient.setCallback(mqtt_callback);

  reconnect_mqtt_timer.setInterval(1000L, reconnect_mqtt);
  breather_timer.setInterval(10L, colourChange);
  Blynk.begin(BLYNK_AUTH, SSID, PASSWD);
}

void loop()
{
  mqttClient.loop();
  Blynk.run();
  reconnect_mqtt_timer.run();
  breather_timer.run();
}

void make_topic(char *buf, int client_id, char *topic)
{
  sprintf(buf, "%d/%s", client_id, topic);
}

void colourChange()
{
  // displayLED.colour(0, co2_ppm, 400, 1200);
  displayLED.colour(1, co2, co2_display_range.min, co2_display_range.max);
}
/************** WiFi / MQTT  *******************************************************************/

void reconnect_mqtt()
{
  // Loop until we're reconnected
  while (!mqttClient.connected())
  {
    // statusLED.flash(error_colour, 50);

    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqttClient.connect(device_id))
    {
      Serial.println("connected");

      // Once connected, publish an announcement...
      Serial.print("DEBUG:cfg_client_id=");
      Serial.println(cfg_client_id);

      mqttClient.publish(info_topic, "ONLINE");

      // ... and resubscribe
      mqttClient.subscribe(co2_topic);
      mqttClient.subscribe(temp_topic);
      mqttClient.subscribe(humidity_topic);
      mqttClient.subscribe(light_topic);
      mqttClient.subscribe(power_topic);
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

int intFromPayload(byte *payload, unsigned int length)
{
  char buffer[50];
  int i;
  for (i = 0; i < length; ++i)
  {
    buffer[i] = payload[i];
  }
  buffer[i + 1] = '\0';
  return strtol(buffer, NULL, 10); //Serial.println(strtod(buffer,NULL));
}

float floatFromPayload(byte *payload, unsigned int length)
{
  char buffer[50];
  int i;
  for (i = 0; i < length; ++i)
  {
    buffer[i] = payload[i];
  }
  buffer[i + 1] = '\0';
  return strtod(buffer, NULL);
}

void mqtt_callback(char *topic, byte *payload, unsigned int length)
{

  if (strcmp(co2_topic, topic) == 0)
    co2 = intFromPayload(payload, length);
  else if (strcmp(temp_topic, topic) == 0)
    temp = floatFromPayload(payload, length);
  else if (strcmp(humidity_topic, topic) == 0)
    humidity = intFromPayload(payload, length);
  else if (strcmp(light_topic, topic) == 0)
    light = intFromPayload(payload, length);
  else if (strcmp(power_topic, topic) == 0)
    power = intFromPayload(payload, length);

  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

/***********************************************************************************************/

/************************************************************************************************
 * Blynk callbacks
 */

BLYNK_WRITE(V1)
{
  co2_display_range.max = param.asInt();
  Serial.print("DEBUG: New CO2 max=");
  Serial.println(co2_display_range.max);
}