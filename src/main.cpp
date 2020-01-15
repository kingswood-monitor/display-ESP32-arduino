// #define BLYNK_DEBUG // Optional, this enables lots of prints
// #define BLYNK_PRINT Serial
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

// displayable measures
float temperature = 0.0;
int humidity = 0;
int co2 = 0;
float power = 0.0;
int gas = 0;

// display brightness
float display_brightness = KW_DEFAULT_DISPLAY_BRIGHTNESS;

// MQTT topics
char info_topic[20];
char temp_topic[20];
char humidity_topic[20];
char co2_topic[20];
char power_topic[20];
char gas_topic[20];

// DisplayLED
DisplayLED displayLED;
CRGB data_colour = CRGB::White;
CRGB wifi_colour = CRGB::Blue;
CRGB error_colour = CRGB::Red;

int colour_index = 0;

enum DisplayMode
{
  TEMPERATURE = 1,
  HUMIDITY = 2,
  CO2 = 3,
  POWER = 4,
  GAS = 5,
};

int display_mode = KW_DEFAULT_DISPLAY_MODE;

typedef struct
{
  int min;
  int max;
} i_display_range_t;

typedef struct
{
  float min;
  float max;
} f_display_range_t;

f_display_range_t temperature_display_range =
    (f_display_range_t){
        .min = KW_DEFAULT_TEMPERATURE_MIN,
        .max = KW_DEFAULT_TEMPERATURE_MAX};

i_display_range_t humidity_display_range =
    (i_display_range_t){
        .min = KW_DEFAULT_HUMIDITY_MIN,
        .max = KW_DEFAULT_HUMIDITY_MAX};

i_display_range_t co2_display_range =
    (i_display_range_t){
        .min = KW_DEFAULT_CO2_MIN,
        .max = KW_DEFAULT_CO2_MAX};

f_display_range_t power_display_range =
    (f_display_range_t){
        .min = KW_DEFAULT_POWER_MIN,
        .max = KW_DEFAULT_POWER_MAX};

i_display_range_t gas_display_range =
    (i_display_range_t){
        .min = KW_DEFAULT_GAS_MIN,
        .max = KW_DEFAULT_GAS_MAX};

BlynkTimer reconnect_mqtt_timer;
BlynkTimer breather_timer;

char chip_id[20];

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
  make_topic(temp_topic, cfg_client_id, "data/temperature");
  make_topic(humidity_topic, cfg_client_id, "data/humidity");
  make_topic(power_topic, cfg_client_id, "data/power");
  make_topic(gas_topic, cfg_client_id, "data/gas");
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
  switch (display_mode)
  {
  case TEMPERATURE:
    displayLED.colour(temperature, temperature_display_range.min, temperature_display_range.max, display_brightness);
    break;
  case HUMIDITY:
    displayLED.colour(humidity, humidity_display_range.min, humidity_display_range.max, display_brightness);
    break;
  case CO2:
    displayLED.colour(co2, co2_display_range.min, co2_display_range.max, display_brightness);
    break;
  case POWER:
    displayLED.colour(power, power_display_range.min, power_display_range.max, display_brightness);
    break;
  case GAS:
    displayLED.colour(gas, gas_display_range.min, gas_display_range.max, display_brightness);
    break;
  }
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
      mqttClient.subscribe(temp_topic);
      BLYNK_LOG("MQTT subscribed to [%s]", temp_topic);
      mqttClient.subscribe(humidity_topic);
      BLYNK_LOG("MQTT subscribed to [%s]", humidity_topic);
      mqttClient.subscribe(co2_topic);
      BLYNK_LOG("MQTT subscribed to [%s]", co2_topic);
      mqttClient.subscribe(power_topic);
      BLYNK_LOG("MQTT subscribed to [%s]", power_topic);
      mqttClient.subscribe(gas_topic);
      BLYNK_LOG("MQTT subscribed to [%s]", gas_topic);
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
  return strtof(buffer, NULL);
}

void mqtt_callback(char *topic, byte *payload, unsigned int length)
{

  if (strcmp(temp_topic, topic) == 0)
  {
    temperature = floatFromPayload(payload, length);
    BLYNK_LOG("MQTT Message arrived [%s] %f", topic, temperature);
  }
  else if (strcmp(humidity_topic, topic) == 0)
  {
    humidity = intFromPayload(payload, length);
    BLYNK_LOG("MQTT Message arrived [%s] %d", topic, humidity);
  }
  else if (strcmp(co2_topic, topic) == 0)
  {
    co2 = intFromPayload(payload, length);
    BLYNK_LOG("MQTT Message arrived [%s] %d", topic, co2);
  }
  else if (strcmp(power_topic, topic) == 0)
  {
    power = floatFromPayload(payload, length);
    BLYNK_LOG("MQTT Message arrived [%s] %f", topic, power);
  }
  else if (strcmp(gas_topic, topic) == 0)
  {
    gas = intFromPayload(payload, length);
    BLYNK_LOG("MQTT Message arrived [%s] %d", topic, gas);
  }
}

/************************************************************************************************
 * Blynk callbacks
 */

BLYNK_WRITE(V1)
{
  display_brightness = param.asFloat();
  BLYNK_LOG("New display brightness: %.3f", display_brightness);
}

BLYNK_WRITE(V2)
{
  display_mode = param.asInt();
  switch (display_mode)
  {
  case TEMPERATURE:
    BLYNK_LOG("Switched to Temperature display, range(%.1f, %.1f)", temperature_display_range.min, temperature_display_range.max);
    break;
  case HUMIDITY:
    BLYNK_LOG("Switched to Humidity display, range(%d, %d)", humidity_display_range.min, humidity_display_range.max);
    break;
  case CO2:
    BLYNK_LOG("Switched to CO2 display, range(%d, %d)", co2_display_range.min, co2_display_range.max);
    break;
  case POWER:
    BLYNK_LOG("Switched to Power display, range(%.0f, %.0f)", power_display_range.min, power_display_range.max);
    break;
  case GAS:
    BLYNK_LOG("Switched to Gas display, range(%d, %d)", gas_display_range.min, gas_display_range.max);
    break;
  }
}
