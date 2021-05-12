/**
  * @brief Auxiliary functions for debugging over Serial
  *
  * Format used on debug functions is the same as `printf()`. Check source code for usage examples
  * Debug calls will be enabled or disabled automatically before compiling according defined `DEBUG_LEVEL`.
  *
  * If `DEBUG_ESP_PORT` is not defined library will give no debug output at all
  *
  * @file EnigmaIOTdebug.h
  * @version 0.9.7
  * @date 04/02/2021
  * @author German Martin
  */

#ifndef _DEBUG_h
#define _DEBUG_h

#include <Arduino.h>
#include "EnigmaIoTconfig.h"
#ifdef ESP32
#include <esp_log.h>
#endif

#define NO_DEBUG	0 ///< @brief Debug level that will give no debug output
#define D_ERROR	1 ///< @brief Debug level that will give error messages
#define WARN	2 ///< @brief Debug level that will give error and warning messages
#define INFO	3 ///< @brief Debug level that will give error, warning and info messages
#define DBG	    4 ///< @brief Debug level that will give error, warning,info AND dbg messages
#define VERBOSE	5 ///< @brief Debug level that will give all defined messages

#ifdef ESP8266
const char* extractFileName (const char* path);
#define DEBUG_LINE_PREFIX() DEBUG_ESP_PORT.printf_P (PSTR("[%lu][%s:%d] %s() Heap: %lu | "),millis(),extractFileName(__FILE__),__LINE__,__FUNCTION__,(unsigned long)ESP.getFreeHeap())
#endif

#ifdef DEBUG_ESP_PORT

#ifdef ESP8266
#if DEBUG_LEVEL >= VERBOSE
#define DEBUG_VERBOSE(text,...) DEBUG_ESP_PORT.print("V ");DEBUG_LINE_PREFIX();DEBUG_ESP_PORT.printf_P(PSTR(text),##__VA_ARGS__);DEBUG_ESP_PORT.println()
#else
#define DEBUG_VERBOSE(...)
#endif

#if DEBUG_LEVEL >= DBG
#define DEBUG_DBG(text,...) DEBUG_ESP_PORT.print("D ");DEBUG_LINE_PREFIX(); DEBUG_ESP_PORT.printf_P(PSTR(text),##__VA_ARGS__);DEBUG_ESP_PORT.println()
#else
#define DEBUG_DBG(...)
#endif

#if DEBUG_LEVEL >= INFO
#define DEBUG_INFO(text,...) DEBUG_ESP_PORT.print("I ");DEBUG_LINE_PREFIX();DEBUG_ESP_PORT.printf_P(PSTR(text),##__VA_ARGS__);DEBUG_ESP_PORT.println()
#else
#define DEBUG_INFO(...)
#endif

#if DEBUG_LEVEL >= WARN
#define DEBUG_WARN(text,...) DEBUG_ESP_PORT.print("W ");DEBUG_LINE_PREFIX();DEBUG_ESP_PORT.printf_P(PSTR(text),##__VA_ARGS__);DEBUG_ESP_PORT.println()
#else
#define DEBUG_WARN(...)
#endif

#if DEBUG_LEVEL >= D_ERROR
#define DEBUG_ERROR(text,...) DEBUG_ESP_PORT.print("E ");DEBUG_LINE_PREFIX();DEBUG_ESP_PORT.printf_P(PSTR(text),##__VA_ARGS__);DEBUG_ESP_PORT.println()
#else
#define DEBUG_ERROR(...)
#endif
#elif defined ESP32
#define DEFAULT_LOG_TAG "EnigmaIOT"
#define DEBUG_VERBOSE(format,...) ESP_LOGV (DEFAULT_LOG_TAG,"%d Heap: %6d. " format, millis(), ESP.getFreeHeap(), ##__VA_ARGS__)
#define DEBUG_DBG(format,...) ESP_LOGD (DEFAULT_LOG_TAG,"%d Heap: %6d " format, millis(), ESP.getFreeHeap(), ##__VA_ARGS__)
#define DEBUG_INFO(format,...) ESP_LOGI (DEFAULT_LOG_TAG,"%d Heap: %6d " format, millis(), ESP.getFreeHeap(), ##__VA_ARGS__)
#define DEBUG_WARN(format,...) ESP_LOGW (DEFAULT_LOG_TAG,"%d Heap: %6d " format, millis(), ESP.getFreeHeap(), ##__VA_ARGS__)
#define DEBUG_ERROR(format,...) ESP_LOGE (DEFAULT_LOG_TAG,"%d Heap: %6d " format, millis(), ESP.getFreeHeap(), ##__VA_ARGS__)
#endif
#else
#define DEBUG_VERBOSE(...)
#define DEBUG_DBG(...)
#define DEBUG_INFO(...)
#define DEBUG_WARN(...)
#define DEBUG_ERROR(...)
#endif



#endif

