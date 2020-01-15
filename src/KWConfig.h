/**
 * @file       KWConfig.h
 * @author     Richard Lyon
 * @license    This project is released under the MIT License (MIT)
 * @copyright  Copyright (c) 2020 Richard Lyon
 * @date       Jan 2020
 * @brief      Configuration of different aspects of the library
 
 */

#pragma once

#define FIRMWARE_NAME "Environment Display"
#define FIRMWARE_VERSION "0.2.1"
#define DEVICE_TYPE "ESP32"

// default display mode
#define KW_DEFAULT_DISPLAY_MODE TEMPERATURE

// default display brightness (1.0 = Maximum)
#define KW_DEFAULT_DISPLAY_BRIGHTNESS 1.0

// temperature defaults
#define KW_DEFAULT_TEMPERATURE_MIN 18.0
#define KW_DEFAULT_TEMPERATURE_MAX 25.0

// humidity defaults
#define KW_DEFAULT_HUMIDITY_MIN 0
#define KW_DEFAULT_HUMIDITY_MAX 100

// CO2 defaults
#define KW_DEFAULT_CO2_MIN 440
#define KW_DEFAULT_CO2_MAX 1000

// power defaults
#define KW_DEFAULT_POWER_MIN 0.0
#define KW_DEFAULT_POWER_MAX 1500.0

// gas defaults
#define KW_DEFAULT_GAS_MIN 0
#define KW_DEFAULT_GAS_MAX 1