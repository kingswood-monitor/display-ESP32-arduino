// #define BLYNK_DEBUG // Optional, this enables lots of prints
#define BLYNK_PRINT Serial
#define BLYNK_NO_FANCY_LOGO

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

#include "KWConfig.h"

/*************************** Device-specific Configuration Settings *******************************************************
 *  Configuration settings stored in permanent FLASH memory                            
 */
#define WRITE_SETTINGS false

#define CFG_CLIENT_ID 30 // 3X = display nodes
#define CFG_NUM_LEDS 2   // Number of active LEDs in display

Preferences preferences;
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

char chip_id[20];

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

display_range_t co2_display_range = (display_range_t){.min = KW_DEFAULT_CO2_MIN, .max = KW_DEFAULT_CO2_MAX};

BlynkTimer reconnect_mqtt_timer;
BlynkTimer breather_timer;

void logo(char *title, char *version, char *type);
void generate_chip_id();
void reconnect_mqtt();
void make_topic(char *buf, int client_id, char *topic);
void colourChange();

void setup()
{
  Serial.begin(115200);
  delay(2000);
  displayLED.off(1);

  //preferences **************************************************************************
  preferences.begin("display", false);

  if (WRITE_SETTINGS)
  {
    preferences.putInt("client_id", CFG_CLIENT_ID);
    preferences.putBool("num_leds", CFG_NUM_LEDS);

    cfg_client_id = CFG_CLIENT_ID;
    cfg_num_leds = CFG_NUM_LEDS;
  }
  else
  {
    cfg_client_id = preferences.getInt("client_id", -1);
    cfg_num_leds = preferences.getBool("num_leds", false);
  }
  // preferences ************************************************************************

  logo(FIRMWARE_NAME, FIRMWARE_VERSION, DEVICE_TYPE);

  generate_chip_id();
  BLYNK_LOG("Chip ID: %s", chip_id);

  // setup MQTT
  mqttClient.setServer(mqtt_server, 1883);
  mqttClient.setCallback(mqtt_callback);
  make_topic(info_topic, cfg_client_id, "info/status");
  make_topic(co2_topic, cfg_client_id, "data/co2");
  make_topic(temp_topic, cfg_client_id, "data/temp");
  make_topic(humidity_topic, cfg_client_id, "data/humidity");
  make_topic(light_topic, cfg_client_id, "data/light");
  make_topic(power_topic, cfg_client_id, "data/power");
  reconnect_mqtt_timer.setInterval(1000L, reconnect_mqtt);

  breather_timer.setInterval(1L, colourChange);
  Blynk.begin(BLYNK_AUTH, SSID, PASSWD);
}

void loop()
{
  Blynk.run();
  mqttClient.loop();
  reconnect_mqtt_timer.run();
  breather_timer.run();
}

void colourChange()
{
  // displayLED.colour(0, co2_ppm, 400, 1200);
  displayLED.colour(1, co2, co2_display_range.min, co2_display_range.max);
}

void generate_chip_id()
{
  uint64_t chipid = ESP.getEfuseMac(); // The chip ID is essentially its MAC address(length: 6 bytes).
  uint16_t chip = (uint16_t)(chipid >> 32);
  sprintf(chip_id, "ESP32-%04X-%08X", chip, (uint32_t)chipid);
}

void logo(char *title, char *version, char *type)
{
  char strap_line[200];
  sprintf(strap_line, "                  |___/  %s v%s on %s", title, version, type);

  Serial.println("  _  __ _                                                _ ");
  Serial.println(" | |/ /(_) _ __    __ _  ___ __      __ ___    ___    __| |");
  Serial.println(" | ' / | || '_ \\  / _` |/ __|\\ \\ /\\ / // _ \\  / _ \\  / _` |");
  Serial.println(" | . \\ | || | | || (_| |\\__ \\ \\ V  V /| (_) || (_) || (_| |");
  Serial.println(" |_|\\_\\|_||_| |_| \\__, ||___/  \\_/\\_/  \\___/  \\___/  \\__,_|");
  Serial.println(strap_line);
  Serial.println();
}

/************** WiFi / MQTT  *******************************************************************/

void reconnect_mqtt()
{
  if (!mqttClient.connected())
  {
    // statusLED.flash(error_colour, 50);

    BLYNK_LOG("Attempting MQTT connection...");
    // Attempt to connect
    if (mqttClient.connect(chip_id))
    {
      BLYNK_LOG("MQTT connected");

      // Once connected, publish an announcement...
      mqttClient.publish(info_topic, "ONLINE");
      BLYNK_LOG("MQTT Published ONLINE to [%s]", info_topic);

      // ... and resubscribe
      mqttClient.subscribe(co2_topic);
      BLYNK_LOG("MQTT subscribed to [%s]", co2_topic);
      mqttClient.subscribe(temp_topic);
      BLYNK_LOG("MQTT subscribed to [%s]", temp_topic);
      mqttClient.subscribe(humidity_topic);
      BLYNK_LOG("MQTT subscribed to [%s]", humidity_topic);
      mqttClient.subscribe(light_topic);
      BLYNK_LOG("MQTT subscribed to [%s]", light_topic);
      mqttClient.subscribe(power_topic);
      BLYNK_LOG("MQTT subscribed to [%s]", power_topic);
    }
    else
    {
      BLYNK_LOG("MQTT reconnect failed, rc=%d", mqttClient.state());
    }
  }
}

void make_topic(char *buf, int client_id, char *topic)
{
  sprintf(buf, "%d/%s", client_id, topic);
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

  char buf[length + 1];
  for (int i = 0; i < length; i++)
  {
    buf[i] = (char)payload[i];
  }
  buf[length + 1] = '\0';
  BLYNK_LOG("MQTT Message arrived [%s] %s", topic, buf);
}

/************************************************************************************************
 * Blynk callbacks
 */

BLYNK_WRITE(V1)
{
  co2_display_range.max = param.asInt();
  BLYNK_LOG("New CO2 max=%d", co2_display_range.max);
}
