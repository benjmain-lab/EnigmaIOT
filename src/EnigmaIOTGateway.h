/**
  * @file EnigmaIOTGateway.h
  * @version 0.9.7
  * @date 04/02/2021
  * @author German Martin
  * @brief Library to build a gateway for EnigmaIoT system
  */

#ifndef _ENIGMAIOTGATEWAY_h
#define _ENIGMAIOTGATEWAY_h

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif
#include "EnigmaIoTconfig.h"
#include "NodeList.h"
#include "Filter.h"
#include "Comms_hal.h"
#include <queue>
#if ENABLE_ASYNC_WIFIMANAGER
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>
#include <DNSServer.h>
#endif
#include <queue>
#include "EnigmaIOTRingBuffer.h"
#if ENABLE_REST_API
#include "GatewayAPI.h"
#endif // ENABLE_REST_API

#include "helperFunctions.h"

#define LED_ON LOW
#define LED_OFF !LED_ON

/**
  * @brief Message code definition
  */
enum gatewayMessageType_t {
	SENSOR_DATA = 0x01, /**< Data message from sensor node */
	SENSOR_BRCAST_DATA = 0x81, /**< Data broadcast message from sensor node */
	UNENCRYPTED_NODE_DATA = 0x11, /**< Data message from sensor node. Unencrypted */
	DOWNSTREAM_DATA_SET = 0x02, /**< Data message from gateway. Downstream data for user commands */
	DOWNSTREAM_BRCAST_DATA_SET = 0x82, /**< Data broadcast message from gateway. Downstream data for user commands */
	DOWNSTREAM_DATA_GET = 0x12, /**< Data message from gateway. Downstream data for user commands */
	DOWNSTREAM_BRCAST_DATA_GET = 0x92, /**< Data broadcast message from gateway. Downstream data for user commands */
	CONTROL_DATA = 0x03, /**< Internal control message from sensor to gateway. Used for OTA, settings configuration, etc */
	DOWNSTREAM_CTRL_DATA = 0x04, /**< Internal control message from gateway to sensor. Used for OTA, settings configuration, etc */
	DOWNSTREAM_BRCAST_CTRL_DATA = 0x84, /**< Internal control broadcast message from gateway to sensor. Used for OTA, settings configuration, etc */
    HA_DISCOVERY_MESSAGE = 0x08, /**< This sends gateway needed information to build a Home Assistant discovery MQTT message to allow automatic entities provision */
    CLOCK_REQUEST = 0x05, /**< Clock request message from node */
	CLOCK_RESPONSE = 0x06, /**< Clock response message from gateway */
	NODE_NAME_SET = 0x07, /**< Message from node to signal its own custom node name */
	NODE_NAME_RESULT = 0x17, /**< Message from gateway to get result after set node name */
	BROADCAST_KEY_REQUEST = 0x08, /**< Message from node to request broadcast key */
	BROADCAST_KEY_RESPONSE = 0x18, /**< Message from gateway with broadcast key */
	CLIENT_HELLO = 0xFF, /**< ClientHello message from sensor node */
	SERVER_HELLO = 0xFE, /**< ServerHello message from gateway */
	INVALIDATE_KEY = 0xFB /**< InvalidateKey message from gateway */
};

enum gatewayPayloadEncoding_t {
	RAW = 0x00, /**< Raw data without specific format */
	CAYENNELPP = 0x81, /**< CayenneLPP packed data */
	PROT_BUF = 0x82, /**< Data packed using Protocol Buffers. NOT IMPLEMENTED */
	MSG_PACK = 0x83, /**< Data packed using MessagePack */
	BSON = 0x84, /**< Data packed using BSON. NOT IMPLEMENTED */
	CBOR = 0x85, /**< Data packed using CBOR. NOT IMPLEMENTED */
	SMILE = 0x86, /**< Data packed using SMILE. NOT IMPLEMENTED */
	ENIGMAIOT = 0xFF
};

/**
  * @brief Key invalidation reason definition
  */
enum gwInvalidateReason_t {
	UNKNOWN_ERROR = 0x00, /**< Unknown error. Not used by the moment */
	WRONG_CLIENT_HELLO = 0x01, /**< ClientHello message received was invalid */
	//WRONG_EXCHANGE_FINISHED = 0x02, /**< KeyExchangeFinished message received was invalid. Probably this means an error on shared key */
	WRONG_DATA = 0x03, /**< Data message received could not be decrypted successfuly */
	UNREGISTERED_NODE = 0x04, /**< Data received from an unregistered node*/
	KEY_EXPIRED = 0x05, /**< Node key has reached maximum validity time */
	KICKED = 0x06 /**< Node key has been forcibly unregistered */
};

#if defined ARDUINO_ARCH_ESP8266 || defined ARDUINO_ARCH_ESP32
#include <functional>
typedef std::function<void (uint8_t* mac, uint8_t* buf, uint8_t len, uint16_t lostMessages, bool control, gatewayPayloadEncoding_t payload_type, char* nodeName)> onGwDataRx_t;
#if SUPPORT_HA_DISCOVERY
typedef std::function<void (const char* topic, char *message, size_t len)> onHADiscovery_t;
#endif
typedef std::function<void (uint8_t* mac, uint16_t node_id, char* nodeName)> onNewNode_t;
typedef std::function<void (uint8_t* mac, gwInvalidateReason_t reason)> onNodeDisconnected_t;
#if ENABLE_ASYNC_WIFIMANAGER
typedef std::function<void (boolean status)> onWiFiManagerExit_t;
#endif
typedef std::function<void (void)> simpleEventHandler_t;

#else
typedef void (*onGwDataRx_t)(uint8_t* mac, uint8_t* data, uint8_t len, uint16_t lostMessages, bool control, gatewayPayloadEncoding_t payload_type, char* nodeName);
typedef void (*onNewNode_t)(uint8_t* mac, uint16_t node_id, char* nodeName);
typedef void (*onNodeDisconnected_t)(uint8_t* mac, gwInvalidateReason_t reason);
#if ENABLE_ASYNC_WIFIMANAGER
typedef void (*onWiFiManagerExit_t)(boolean status);
#endif
typedef void (*simpleEventHandler_t)(void);
#endif

typedef struct {
	uint8_t channel = DEFAULT_CHANNEL; /**< Channel used for communications*/
	uint8_t networkKey[KEY_LENGTH];   /**< Network key to protect key agreement*/
	char networkName[NETWORK_NAME_LENGTH];   /**< Network name, used to help nodes to find gateway*/
} gateway_config_t;

typedef struct {
	uint8_t addr[ENIGMAIOT_ADDR_LEN]; /**< Message address*/
	uint8_t data[MAX_MESSAGE_LENGTH]; /**< Message buffer*/
	size_t len; /**< Message length*/
} msg_queue_item_t;



/**
  * @brief Ring buffer class. Used to implement message buffer - vector based
  *
  */
 #include <vector>
template <typename Telement>
class EnigmaIOTRingBufferVector {
protected:
	int maxSize; ///< @brief Buffer size
	// Telement* buffer; ///< @brief Actual buffer
	std::vector<Telement*> buffer;

public:
	/**
	  * @brief Creates a ring buffer to hold `Telement` objects
	  * @param range Buffer depth
	  */
	EnigmaIOTRingBufferVector <Telement> (int range) : maxSize (range) {
		// buffer = new Telement[maxSize];
	}
    
    /**
      * @brief EnigmaIOTRingBuffer destructor 
      * @param range Free up buffer memory
      */
    ~EnigmaIOTRingBufferVector () {
        maxSize = 0;
        // delete[] (buffer);
		buffer.clear();
    }

	/**
	  * @brief Returns actual number of elements that buffer holds
	  * @return Returns Actual number of elements that buffer holds
	  */
	int size () { return buffer.size(); }

	/**
	  * @brief Checks if buffer is full
	  * @return Returns `true`if buffer is full, `false` otherwise
	  */
	bool isFull () { return buffer.size() == maxSize; }

	/**
	  * @brief Checks if buffer is empty
	  * @return Returns `true`if buffer has no elements stored, `false` otherwise
	  */
	bool empty () { return (buffer.size() == 0); }

	/**
	  * @brief Adds a new item to buffer, deleting older element if it is full
	  * @param item Element to add to buffer
	  * @return Returns `false` if buffer was full before inserting the new element, `true` otherwise
	  */
	bool push (Telement* item) {
		//TODO Handle max size
		Telement message = new Telement;

		message.len = item->len;
		memcpy (message.data, item->data, item->len);
		memcpy (message.addr, item->addr, ENIGMAIOT_ADDR_LEN);

		buffer.insert(buffer.end(),&message);
		return true;

	}

	/**
	  * @brief Deletes older item from buffer, if buffer is not empty
	  * @return Returns `false` if buffer was empty before trying to delete element, `true` otherwise
	  */
	bool pop () {
		bool wasEmpty = empty ();
		if (!wasEmpty)
		{
			buffer.erase(buffer.begin());
		}
		return !wasEmpty;
	}

	/**
	  * @brief Gets a pointer to older item in buffer, if buffer is not empty
	  * @return Returns pointer to element. If buffer was empty before calling this method it returns `NULL`
	  */
	Telement* front () {
		if (!empty ()) {
			return *buffer.begin();
		} else {
			return NULL;
		}
	}
};



/**
  * @brief Main gateway class. Manages communication with nodes and sends data to upper layer
  *
  */
class EnigmaIOTGatewayClass {
protected:
	uint8_t myPublicKey[KEY_LENGTH]; ///< @brief Temporary public key store used during key agreement
	bool flashTx = false; ///< @brief `true` if Tx LED should flash
	volatile bool flashRx = false; ///< @brief `true` if Rx LED should flash
	node_t node; ///< @brief temporary store to keep node data while processing a message
	NodeList nodelist; ///< @brief Node database that keeps status and shared keys
	Comms_halClass* comm; ///< @brief Instance of physical communication layer
	int8_t txled = -1; ///< @brief I/O pin to connect a led that flashes when gateway transmits data
	int8_t rxled = -1; ///< @brief I/O pin to connect a led that flashes when gateway receives data
	unsigned long txLedOnTime; ///< @brief Flash duration for Tx LED
	unsigned long rxLedOnTime; ///< @brief Flash duration for Rx LED
    onGwDataRx_t notifyData; ///< @brief Callback function that will be invoked when data is received from a node
#if SUPPORT_HA_DISCOVERY
    onHADiscovery_t notifyHADiscovery; ///< @brief Callback function that will be invoked when HomeAssistant discovery message is received from a node
#endif
	onNewNode_t notifyNewNode; ///< @brief Callback function that will be invoked when a new node is connected
	onNodeDisconnected_t notifyNodeDisconnection; ///< @brief Callback function that will be invoked when a node gets disconnected
	simpleEventHandler_t notifyRestartRequested; ///< @brief Callback function that will be invoked when a hardware restart is requested
	bool useCounter = true; ///< @brief `true` if counter is used to check data messages order
	gateway_config_t gwConfig; ///< @brief Gateway specific configuration to be stored on flash memory
	char plainNetKey[KEY_LENGTH];
#ifdef ESP32
	portMUX_TYPE myMutex = portMUX_INITIALIZER_UNLOCKED; ///< @brief Handle to control critical sections
#endif
	msg_queue_item_t tempBuffer; ///< @brief Temporary storage for input message got from buffer

	EnigmaIOTRingBuffer<msg_queue_item_t>* input_queue; ///< @brief Input messages buffer. It acts as a FIFO queue
	#if ENABLE_ASYNC_WIFIMANAGER
	AsyncWebServer* server; ///< @brief WebServer that holds configuration portal
	DNSServer* dns; ///< @brief DNS server used by configuration portal
	AsyncWiFiManager* wifiManager; ///< @brief Wifi configuration portal
	onWiFiManagerExit_t notifyWiFiManagerExit; ///< @brief Function called when configuration portal exits
	#endif
	simpleEventHandler_t notifyWiFiManagerStarted; ///< @brief Function called when configuration portal is started

	friend class GatewayAPI;

	/**
	 * @brief Activates a flag that signals that configuration has to be saved
	 */
	static void doSave (void);
    
    /**
     * @brief Activates a flag that signals that configuration has to be saved
     */
    static void doResetConfig (void);


	/**
	 * @brief Build a **ServerHello** message and send it to node
	 * @param key Node public key to be used on Diffie Hellman algorithm
	 * @param node Entry in node list database where node will be registered
	 * @return Returns `true` if ServerHello message was successfully sent. `false` otherwise
	 */
	bool serverHello (const uint8_t* key, Node* node);

	/**
	 * @brief Sends broadcast key to node if it has requested it explicitly or it has notified during handshake
	 * @param node Entry in node list database to get destination address
	 * @return Returns `true` if message was successfully sent. `false` otherwise
	 */
	bool sendBroadcastKey (Node* node);

	/**
	 * @brief Gets a buffer containing a **ClientHello** message and process it. This carries node public key to be used on Diffie Hellman algorithm
	 * @param mac Address where this message was received from
	 * @param buf Pointer to the buffer that contains the message
	 * @param count Message length in number of bytes of ClientHello message
	 * @param node Node entry that Client Hello message comes from
	 * @return Returns `true` if message could be correcly processed
	 */
	bool processClientHello (const uint8_t mac[ENIGMAIOT_ADDR_LEN], const uint8_t* buf, size_t count, Node* node);

	/**
	 * @brief Starts clock sync procedure from node to gateway
	 * @param mac Address where this message was received from
	 * @param buf Pointer to the buffer that contains the message
	 * @param count Message length in number of bytes of ClockRequest message
	 * @param node Node entry that Client Hello message comes from
	 * @return Returns `true` if message could be correcly processed
	 */
	bool processClockRequest (const uint8_t mac[ENIGMAIOT_ADDR_LEN], const uint8_t* buf, size_t count, Node* node);

	/**
	 * @brief Returns timestaps needed so that node can calculate time difference
	 * @param node Pointer to data that corresponds to originating node
     * @param t1 Origin clock
     * @param t2 Received clock
	 * @return Returns `true` if message could be correcly processed
	 */
    bool clockResponse (Node* node, uint64_t t1, uint64_t t2);

	/**
	 * @brief Creates an **InvalidateKey** message and sned it. This trigger a new key agreement to start on related node
	 * @param node Node to send Invalidate Key message to
	 * @param reason Reason that produced key invalidation in gwInvalidateReason_t format
	 * @return Returns `true` if message could be correcly sent
	 */
	bool invalidateKey (Node* node, gwInvalidateReason_t reason);

	/**
	 * @brief Processes data message from node
	 * @param mac Node address
	 * @param buf Buffer that stores received message
	 * @param count Length of received data
	 * @param node Node where data message comes from
	 * @param encrypted `true` if received message is encrypted
	 * @return Returns `true` if message could be correcly decoded
	 */
	bool processDataMessage (const uint8_t mac[ENIGMAIOT_ADDR_LEN], uint8_t* buf, size_t count, Node* node, bool encrypted = true);

	/**
	 * @brief Processes unencrypted data message from node
	 * @param mac Node address
	 * @param buf Buffer that stores received message
	 * @param count Length of received data
	 * @param node Node where data message comes from
	 * @return Returns `true` if message could be correcly decoded
	 */
	bool processUnencryptedDataMessage (const uint8_t mac[ENIGMAIOT_ADDR_LEN], uint8_t* buf, size_t count, Node* node);

	/**
	 * @brief Builds, encrypts and sends a **DownstreamData** message.
	 * @param node Node that downstream data message is going to
	 * @param data Buffer to store payload to be sent
	 * @param len Length of payload data
	 * @param controlData Content data type if control data
	 * @param encoding Identifies data encoding of payload. It can be RAW, CAYENNELPP, MSGPACK
	 * @return Returns `true` if message could be correcly sent or scheduled
	 */
	bool downstreamDataMessage (Node* node, const uint8_t* data, size_t len, control_message_type_t controlData, gatewayPayloadEncoding_t encoding = ENIGMAIOT);

	/**
	* @brief Processes control message from node
	* @param mac Node address
	* @param buf Buffer that stores received message
	* @param count Length of received data
	* @param node Node where data message comes from
	* @return Returns `true` if message could be correcly decoded
	*/
	bool processControlMessage (const uint8_t mac[ENIGMAIOT_ADDR_LEN], uint8_t* buf, size_t count, Node* node);

	/**
	* @brief Processes new node name request fromn node
	* @param mac Node address
	* @param buf Buffer that stores received message
	* @param count Length of received data
	* @param node Node where data message comes from
	* @return Returns `true` if message could be correcly decoded
	*/
	bool processNodeNameSet (const uint8_t mac[ENIGMAIOT_ADDR_LEN], uint8_t* buf, size_t count, Node* node);

	/**
	 * @brief Send back set name response
	 * @param node Pointer to data that corresponds to originating node
	 * @param error Result code of name set procedure (0: OK, -1: Already used, -2: Too long, -3: Empty name, -4: Message error)
	 * @return Returns `true` if message could be correcly processed
	 */
	bool nodeNameSetRespose (Node* node, int8_t error);

	/**
	 * @brief Process every received message.
	 *
	 * It starts clasiffying message usint the first byte. After that it passes it to the corresponding method for decoding
	 * @param mac Address of message sender
	 * @param buf Buffer that stores message bytes
	 * @param count Length of message in number of bytes
	 */
	void manageMessage (const uint8_t* mac, uint8_t* buf, uint8_t count);

	/**
	 * @brief Function that will be called anytime this gateway receives a message
	 * @param mac_addr Address of message sender
	 * @param data Buffer that stores message bytes
	 * @param len Length of message in number of bytes
	 */
	static void rx_cb (uint8_t* mac_addr, uint8_t* data, uint8_t len);

	/**
	 * @brief Function that will be called anytime this gateway sends a message
	 * to indicate status result of sending process
	 * @param mac_addr Address of message destination
	 * @param status Result of sending process
	 */
	static void tx_cb (uint8_t* mac_addr, uint8_t status);

	/**
	 * @brief Functrion to debug send status.
	 * @param mac_addr Address of message sender
	 * @param status Result status code
	 */
	void getStatus (uint8_t* mac_addr, uint8_t status);

	/**
	* @brief Loads configuration from flash memory
	* @return Returns `true` if data was read successfuly. `false` otherwise
	*/
	bool loadFlashData ();

	/**
	 * 
	 *
	 */
	 void setGwConfigData(uint8_t channel, const char* networkKey, const char* networkName);

   /**
	* @brief Saves configuration to flash memory
	* @return Returns `true` if data could be written successfuly. `false` otherwise
	*/
    bool saveFlashData ();
    
#if SUPPORT_HA_DISCOVERY
    /**
    * @brief Sends a Home Assistant discovery message after receiving it from node
    * @param address Node physical address
    * @param data MsgPack input buffer
    * @param len Input buffer length
    * @param networkName EnigmaIOT network name
    * @param nodeName Node name. Can be NULL
    * @return Returns `true` if data could be written successfuly. `false` otherwise
    */
    bool sendHADiscoveryJSON (uint8_t* address, uint8_t* data, size_t len, const char* networkName, const char* nodeName);
#endif
    
public:
   /**
	* @brief Gets flag that indicates if configuration should be saved
	* @return Returns `true` if config data should be saved. `false` otherwise
	*/
	bool getShouldSave ();

   /**
	* @brief Gets EnigmaIOT network name
	* @return Returns EnigmaIOT network name
	*/
	char* getNetworkName () {
		return gwConfig.networkName;
	}

   /**
	* @brief Gets hashed EnigmaIOT network key
	* @return Returns hashed EnigmaIOT network key
	*/
	char* getNetworkKey (bool plain = false) {
		if (plain)
			return (char*)(plainNetKey);
		else
			return (char*)(gwConfig.networkKey);
	}
	#if ENABLE_ASYNC_WIFIMANAGER
   /**
	* @brief Adds a parameter to configuration portal
	* @param p Configuration parameter
	*/
	void addWiFiManagerParameter (AsyncWiFiManagerParameter* p) {
		if (wifiManager) {
			wifiManager->addParameter (p);
		}
	}

   /**
	* @brief Register callback to be called on wifi manager exit
	* @param handle Callback function pointer
	*/
	void onWiFiManagerExit (onWiFiManagerExit_t handle) {
		notifyWiFiManagerExit = handle;
	}

   /**
	* @brief Register callback to be called on wifi manager start
	* @param handle Callback function pointer
	*/
    void onWiFiManagerStarted (simpleEventHandler_t handle) {
		notifyWiFiManagerStarted = handle;
	}

	/**
	* @brief Starts configuration AP and web server and gets settings from it
	* @return Returns `true` if data was been correctly configured. `false` otherwise
	*/
	bool configWiFiManager ();
	#endif
	
	/**
	 * @brief Initalizes communication basic data and starts accepting node registration
	 * @param comm Physical layer to be used on this network
	 * @param networkKey Network key to protect shared key agreement
	 * @param useDataCounter Indicates if a counter is going to be added to every message data to check message sequence. `true` by default
	 */
	void begin (Comms_halClass* comm, uint8_t* networkKey = NULL, bool useDataCounter = true);

	/**
	 * 
	 */
	void begin(Comms_halClass* comm, char* networkName, uint8_t* networkKey, uint8_t channel, bool useDataCounter);

	/**
	 * @brief This method should be called periodically for instance inside `loop()` function.
	 * It is used for internal gateway maintenance tasks
	 */
	void handle ();

	/**
	 * @brief Sets a LED to be flashed every time a message is transmitted
	 * @param led LED I/O pin
	 * @param onTime Flash duration. 100ms by default.
	 */
	void setTxLed (uint8_t led, time_t onTime = FLASH_LED_TIME);

	/**
	 * @brief Sets a LED to be flashed every time a message is received
	 * @param led LED I/O pin
	 * @param onTime Flash duration. 100ms by default.
	 */
	void setRxLed (uint8_t led, time_t onTime = FLASH_LED_TIME);

	/**
	 * @brief Defines a function callback that will be called on every downlink data message that is received from a node
	 *
	 * Use example:
	 * ``` C++
	 * // First define the callback function
	 * void processRxData (const uint8_t* mac, const uint8_t* buffer, uint8_t length, uint16_t lostMessages) {
	 *   // Do whatever you need with received data
	 * }
	 *
	 * void setup () {
	 *   .....
	 *   // Now register function as data message handler
	 *   EnigmaIOTNode.onDataRx (processRxData);
	 *   .....
	 * }
	 *
	 * void loop {
	 *   .....
	 *   EnigmaIOTNode.handle();
	 *   .....
	 * }
	 * ```
	 * @param handler Pointer to the function
	 */
	void onDataRx (onGwDataRx_t handler) {
		notifyData = handler;
    }

#if SUPPORT_HA_DISCOVERY
    /**
     * @brief Defines a function callback that will be called when a Home Assistant discovery message is received from a node
 	 * @param handler Pointer to the function
     */
    void onHADiscovery (onHADiscovery_t handler) {
        notifyHADiscovery = handler;
    }
#endif

	/**
	 * @brief Gets packet error rate of node that has a specific address
	 * @param address Node address
	 * @return Packet error rate
	 */
	double getPER (uint8_t* address);

	/**
	 * @brief Gets total packets sent by node that has a specific address
	 * @param address Node address
	 * @return Node total sent packets
	 */
	uint32_t getTotalPackets (uint8_t* address);

	/**
	 * @brief Gets number of errored packets of node that has a specific address
	 * @param address Node address
	 * @return Node errored packets
	 */
	uint32_t getErrorPackets (uint8_t* address);

	/**
	 * @brief Gets packet rate sent by node that has a specific address, in packets per hour
	 * @param address Node address
	 * @return Node packet rate
	 */
	double getPacketsHour (uint8_t* address);

	/**
	 * @brief Starts a downstream data message transmission
	 * @param mac Node address
	 * @param data Payload buffer
	 * @param len Payload length
	 * @param controlData Indicates if data is control data and its class
	 * @param payload_type Identifies data encoding of payload. It can be RAW, CAYENNELPP, MSGPACK
	 * @param nodeName Causes data to be sent to a node with this name instead of numeric address
	 * @return Returns true if everything went ok
	 */
	bool sendDownstream (uint8_t* mac, const uint8_t* data, size_t len, control_message_type_t controlData, gatewayPayloadEncoding_t payload_type = RAW, char* nodeName = NULL);

	/**
	 * @brief Defines a function callback that will be called every time a node gets connected or reconnected
	 *
	 * Use example:
	 * ``` C++
	 * // First define the callback function
	 * void newNodeConnected (uint8_t* mac) {
	 *   // Do whatever you need new node address
	 * }
	 *
	 * void setup () {
	 *   .....
	 *   // Now register function as new node condition handler
	 *   EnigmaIOTGateway.onNewNode (newNodeConnected);
	 *   .....
	 * }
	 *
	 * void loop {
	 *   .....
	 *   EnigmaIOTNode.handle();
	 *   .....
	 * }
	 * ```
	 * @param handler Pointer to the function
	 */
	void onNewNode (onNewNode_t handler) {
		notifyNewNode = handler;
	}

	/**
	 * @brief Defines a function callback that will be called every time a node is disconnected
	 *
	 * Use example:
	 * ``` C++
	 * // First define the callback function
	 * void nodeDisconnected (uint8_t* mac, gwInvalidateReason_t reason) {
	 *   // Do whatever you need node address and disconnection reason
	 * }
	 *
	 * void setup () {
	 *   .....
	 *   // Now register function as new node condition handler
	 *   EnigmaIOTGateway.onNodeDisconnected (nodeDisconnected);
	 *   .....
	 * }
	 *
	 * void loop {
	 *   .....
	 *   EnigmaIOTNode.handle();
	 *   .....
	 * }
	 * ```
	 * @param handler Pointer to the function
	 */
	void onNodeDisconnected (onNodeDisconnected_t handler) {
		notifyNodeDisconnection = handler;
	}

	/**
	 * @brief Defines a function callback that will process a gateway restart request
	 * @param handler Pointer to the function
	 */
	void onGatewayRestartRequested (simpleEventHandler_t handler) {
		notifyRestartRequested = handler;
	}
    
   /**
	 * @brief Add message to input queue
	 * @param addr Origin address
	 * @param msg EnigmaIoT message
	 * @param len Message length
	 */
	bool addInputMsgQueue (const uint8_t* addr, const uint8_t* msg, size_t len);

	 /**
	 * @brief Gets next item in the queue
	 * @return Next message to be processed
	 */
	msg_queue_item_t* getInputMsgQueue (msg_queue_item_t* buffer);

   /**
	 * @brief Deletes next item in the queue
	 */
	void popInputMsgQueue ();

	/**
	 * @brief Gets number of active nodes
	 * @return Number of registered nodes
	 */
	int getActiveNodesNumber () {
		return nodelist.countActiveNodes ();
	}

	/**
	 * @brief Gets nodes data structure
	 * @return All nodes data structure
	 */
	NodeList* getNodes () {
		return &nodelist;
	}

};

extern EnigmaIOTGatewayClass EnigmaIOTGateway;

#endif