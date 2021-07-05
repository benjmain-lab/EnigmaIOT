/**
  * @file EnigmaIOTjsonController.h
  * @version 0.9.7
  * @date 04/02/2021
  * @author German Martin
  * @brief Prototype for JSON/MSGPACK based controller node
  */

#ifndef _ENIGMAIOTJSONCONTROLLER_h
#define _ENIGMAIOTJSONCONTROLLER_h

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#include <EnigmaIOTNode.h>
#include <ArduinoJson.h>
#if SUPPORT_HA_DISCOVERY    
#include <queue>
#endif

#if defined ESP8266 || defined ESP32
#include <functional>
typedef std::function<bool (const uint8_t* data, size_t len, nodePayloadEncoding_t payloadEncoding, dataMessageType_t dataMsgType)> sendData_cb; /**< Data send callback definition */
#if SUPPORT_HA_DISCOVERY
typedef std::function<void ()> haDiscovery_call_t; /**< Function called to send HA discovery data */
#endif // SUPPORT_HA_DISCOVERY
#else
#error This code only supports ESP8266 or ESP32 platforms
#endif // defined ESP8266 || defined ESP32

class EnigmaIOTjsonController {
protected:
	sendData_cb sendData;
    EnigmaIOTNodeClass* enigmaIotNode;
#if SUPPORT_HA_DISCOVERY
    std::queue<haDiscovery_call_t> haCallQueue;
    bool doSendHAdiscovery = false;
    clock_t sendHAtime;
    clock_t sendHAdelay = HA_FIRST_DISCOVERY_DELAY;
#endif // SUPPORT_HA_DISCOVERY

public:
   /**
	 * @brief Initialize data structures
 	 * @param node Pointer to EnigmaIOT node instance
	 * @param config Pointer to configuration structure. If it is `NULL` then it tries to load configuration from flash
	 */
	virtual void setup (EnigmaIOTNodeClass* node, void* config = NULL) = 0;

	/**
	 * @brief This should be called periodically for module handling
	 */
	virtual void loop () = 0;

	/**
	 * @brief Called to process a downlink command
	 * @param mac Address of sender
	 * @param buffer Message bytes
	 * @param length Message length
	 * @param command Type of command. nodeMessageType_t
	 * @param payloadEncoding Payload encoding of type nodePayloadEncoding_t
	 * @return `true` on success
	 */
	virtual bool processRxCommand (
		const uint8_t* mac, const uint8_t* buffer, uint8_t length, nodeMessageType_t command, nodePayloadEncoding_t payloadEncoding) = 0;

	/**
	 * @brief Register send data callback to run when module needs to send a message
	 * @param cb Callback with sendData_cb format
	 */
	void sendDataCallback (sendData_cb cb) {
		sendData = cb;
	}

	/**
	 * @brief Used to notify controller that it is registered on EnigmaIOT network
	 */
    void connectInform () {
        DEBUG_INFO ("Connect inform");
        sendStartAnouncement ();
#if SUPPORT_HA_DISCOVERY
        if (enigmaIotNode->getNode ()->getSleepy ()) {
            sendHAdelay = HA_FIRST_DISCOVERY_DELAY_SLEEPY;
        }
        doSendHAdiscovery = true;
        sendHAtime = millis ();
#endif // SUPPORT_HA_DISCOVERY
    }

    /**
     * @brief Used to notify controller that it is unregistered on EnigmaIOT network
     */
    virtual void disconnectInform (nodeInvalidateReason_t reason){}

	/**
	 * @brief Called when wifi manager starts config portal
	 */
	virtual void configManagerStart () = 0;

	/**
	 * @brief Called when wifi manager exits config portal
	 * @param status `true` if configuration was successful
	 */
	virtual void configManagerExit (bool status) = 0;

	/**
	 * @brief Loads output module configuration
	 * @return Returns `true` if load was successful. `false` otherwise
	 */
    virtual bool loadConfig () = 0;

#if SUPPORT_HA_DISCOVERY    
    void callHAdiscoveryCalls () {
        if (doSendHAdiscovery && millis () - sendHAtime > sendHAdelay) {
            haDiscovery_call_t hacall = 0;
            DEBUG_INFO ("Call HA discovery");
            if (haCallQueue.size ()) {
                hacall = haCallQueue.front ();
            }
            DEBUG_DBG ("haCallQueue size is %d", haCallQueue.size ());
            if (hacall) {
                DEBUG_DBG ("Execute hacall");
                hacall ();
                haCallQueue.pop ();
                sendHAtime = millis ();
                if (enigmaIotNode->getNode ()->getSleepy ()) {
                    sendHAdelay = HA_NEXT_DISCOVERY_DELAY_SLEEPY;
                } else {
                    sendHAdelay = HA_NEXT_DISCOVERY_DELAY;
                }
            } else {
                doSendHAdiscovery = false;
            }
            DEBUG_INFO (" Exit call HA discovery");
        }
    }
#endif
    
protected:

	/**
	  * @brief Sends command processing response acknowledge
	  * @param command Command name
	  * @param result Command execution success
	  * @return Returns `true` if message sending was successful. `false` otherwise
	  */
	virtual bool sendCommandResp (const char* command, bool result) = 0;

	/**
	  * @brief Send a message to notify node has started running
	  * @return Returns `true` if message sending was successful. `false` otherwise
	  */
	virtual bool sendStartAnouncement () = 0;

	/**
	  * @brief Saves output module configuration
	  * @return Returns `true` if save was successful. `false` otherwise
	  */
	virtual bool saveConfig () = 0;

	/**
	  * @brief Sends a JSON encoded message to lower layer
	  * @return Returns `true` if message sending was successful. `false` otherwise
	  */
	bool sendJson (DynamicJsonDocument& json) {
		int len = measureMsgPack (json) + 1;
		uint8_t* buffer = (uint8_t*)malloc (len);
		len = serializeMsgPack (json, (char*)buffer, len);

		size_t strLen = measureJson (json) + 1;
		char* strBuffer = (char*)calloc (sizeof (uint8_t), strLen);

		/*Serial.printf ("Trying to send: %s\n", printHexBuffer (
			buffer, len));*/
		serializeJson (json, strBuffer, strLen);
		DEBUG_INFO ("Trying to send: %s", strBuffer);
		bool result = false;
		if (sendData)
            result = sendData (buffer, len, MSG_PACK, DATA_TYPE);
		if (!result) {
			DEBUG_WARN ("---- Error sending data");
		} else {
			DEBUG_INFO ("---- Data sent");
        }
        if (buffer) {
            free (buffer);        
        }
        if (strBuffer) {
            free (strBuffer);
        }
		return result;
    }

#if SUPPORT_HA_DISCOVERY    
    void addHACall (haDiscovery_call_t HACall) {
        haCallQueue.push (HACall);
    }

    bool sendHADiscovery (uint8_t* data, size_t len) {
        if (!data || !len) {
            DEBUG_WARN ("Empty HA message");
            return false;
        }
        bool result = false;
        if (sendData)
            result = sendData (data, len, MSG_PACK, HA_DISC_TYPE);
        if (!result) {
            DEBUG_WARN ("---- Error sending data");
        } else {
            DEBUG_INFO ("---- Data sent");
        }
        return result;
    }
#endif
};

#endif // _ENIGMAIOTJSONCONTROLLER_h

