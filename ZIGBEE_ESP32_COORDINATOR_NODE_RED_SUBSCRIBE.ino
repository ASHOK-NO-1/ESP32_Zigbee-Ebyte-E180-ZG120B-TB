// Libraries
#include "ZIGBEE_Data.h"
#include "ESPFileServer.h"
#include <SPIFFS.h>         
#include <ArduinoJson.h>
#include <map>


// EspFlash filename
const char* SENSOR_ADDRESS_FILE = "/sensor_addresses.json";

// Library classes
//MQTT mqtt("KIoT", "1234567899", "192.168.1.103", 1883);
//MQTT mqtt("ASHOK", "1234567890", "192.168.208.135", 1883);
//MQTT mqtt("192.168.244.135", 1883);
//MQTT mqtt("192.168.1.103", 1883);


MQTT mqtt;
ZIGBEE zigbee(19, 18);
ZIGBEE_Data zigbee_data;
ZIGBEE_Query zigbee_query;
Autoconnect autoconnect;
ESPFileServer flash_server;
JsonFileManager jsonManager(SENSOR_ADDRESS_FILE);
DynamicJsonDocument sensorAddresses(1024);

// mapping flash json data to compare existing zigbee data
std::map<uint16_t, uint16_t> shortAddressToEspIdMap;

// Struct for Zigbee info Mapping
struct ShortAddressMapping {
    uint8_t FloorNo;
    uint8_t RoomNo;
    uint8_t SensorID;
    uint8_t SensorNo;
    uint16_t ShortAddress;
};


// Function prototypes
void setup();
void ZIGBEE_Read(void *pvParameters);
void ZIGBEE_Serial_Write(void *pvParameters);
void WiFi_Client_Connection(void *pvParameters);
void Process_Zigbee_Command(const uint8_t* byteCommand, size_t length);
ShortAddressMapping readShortAddressMapping();
void Save_Short_Address_Mapping(const ShortAddressMapping& mapping);
uint8_t calculateXORChecksum(const uint8_t* array, int startIndex, int endIndex);
void loop();

// Setup function
void setup() {
    Serial.begin(115200);
    // Initialize SPIFFS
    if (!SPIFFS.begin(true)) {
        Serial.println("An error occurred while mounting SPIFFS");
        //return; // Return a default-constructed MQTT instance
    }
    
    // Load JSON data from file
    File configFile = SPIFFS.open("/co-ordinator_config.json", "r"); // r stands for read mode
    if (!configFile) {
        Serial.println("Failed to open config file");
        //return; // Return a default-constructed MQTT instance
    }
    
    // Parse JSON data
    size_t size = configFile.size();
    std::unique_ptr<char[]> buffer(new char[size]);
    configFile.readBytes(buffer.get(), size);
    configFile.close();
    
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, buffer.get());
    if (error) {
        Serial.println("Failed to parse config file");
        //return; // Return a default-constructed MQTT instance
    }

    const char* mqtt_ip = doc["MQTT"]["MqttID"];
    int mqtt_port = doc["MQTT"]["MqttPortNo"];
    // Print extracted MQTT parameters
    Serial.print("MQTT Broker IP: ");
    Serial.println(mqtt_ip);
    Serial.print("MQTT Port: ");
    Serial.println(mqtt_port);
    // Initialize and return MQTT object with extracted parameters
    mqtt = MQTT(mqtt_ip, mqtt_port);
    
    mqtt.begin();
    zigbee.begin();
    flash_server.begin();
  
    if (!SPIFFS.begin(true)) {
        Serial.println("An error occurred while mounting SPIFFS");
        return;
    }
    
    if (!jsonManager.loadJsonFromFile(sensorAddresses)) {
        Serial.println("Failed to load JSON from file");
    }
    jsonManager.saveJsonToFile(sensorAddresses);
    
    xTaskCreatePinnedToCore(ZIGBEE_Read, "Task1", 30000, NULL, 9, NULL, 0); 
    xTaskCreatePinnedToCore(ZIGBEE_Serial_Write, "Task2", 30000, NULL, 9, NULL, 0); 
    xTaskCreatePinnedToCore(WiFi_Client_Connection, "Task3", 8000, NULL, 9, NULL, 0);
   // xTaskCreatePinnedToCore(Mqtt_Subscribe, "Task4", 20000, NULL, 9, NULL, 0);

}

// Task to read Zigbee data
void ZIGBEE_Read(void *pvParameters) {
    (void) pvParameters; 
    for (;;) {
        zigbee_data.Serial_read_Zigbee(); 
        vTaskDelay(10);
    }
}

// Task to write Zigbee data
void ZIGBEE_Serial_Write(void *pvParameters) {
    (void) pvParameters; 
    for (;;) {
        zigbee_data.Serial_write_Zigbee();
        zigbee_query.Query();
        vTaskDelay(10);
    }
}

// Task for WiFi client connection
void WiFi_Client_Connection(void *pvParameters) {
    (void) pvParameters; 
    for (;;) {
        
        mqtt.reconnect_client();
        //client.loop();
        vTaskDelay(10);
    }
}


//Task Mqtt Subscription
// Task to read Zigbee data


// Process Zigbee command
void Process_Zigbee_Command(const uint8_t* byteCommand, size_t length) {
    for (size_t i = 0; i < length;) {
        uint8_t header = byteCommand[i]; 
        uint8_t len = byteCommand[i + 1];
        int totalLength = len + 2;
        
        if (i + totalLength <= length) {    // Extract command-specific information     
            uint8_t checksumRange = totalLength - 2;
            uint8_t realChecksum = byteCommand[i + totalLength - 1];
            uint8_t serialFrameNumber = byteCommand[i + 8];
            uint8_t xorChecksum = calculateXORChecksum(byteCommand, 2, checksumRange);
            
            
            if(header == 0x55 && realChecksum == xorChecksum ){
                uint16_t shortAddress = (static_cast<uint16_t>(byteCommand[i + 5]) << 8) | static_cast<uint16_t>(byteCommand[i + 6]);
                uint8_t floorNo = byteCommand[i+16];
                uint8_t roomNo = byteCommand[i+17];
                uint8_t sensorID = byteCommand[i+18];
                uint8_t sensorNo = byteCommand[i+19];
                //uint16_t espID =  (static_cast<uint16_t>(byteCommand[i + 16]) << 8) | static_cast<uint16_t>(byteCommand[i + 17]);
                
                zigbee_data.Read_Publish_Zigbee_Json_data(byteCommand, length, floorNo,roomNo, sensorID,sensorNo, shortAddress);
                
                // Accessing variables from the zigbee_data object to store in flash memory
                uint8_t zigbeeFloorNo = zigbee_data._Floor_No;
                uint8_t zigbeeRoomNo = zigbee_data._Room_No;
                uint8_t zigbeeSensorID = zigbee_data._Sensor_ID;
                uint8_t zigbeeSensorNo = zigbee_data._Sensor_No;
                uint16_t zigbeeShortAddress = zigbee_data._Short_Address;
                               
                
                if(zigbeeSensorID !=00){
                   ShortAddressMapping mapping;
                   mapping.FloorNo = zigbeeFloorNo;
                   mapping.RoomNo = zigbeeRoomNo; 
                   mapping.SensorID = zigbeeSensorID;
                   mapping.SensorNo = zigbeeSensorNo; 
                   mapping.ShortAddress = zigbeeShortAddress;
                   Save_Short_Address_Mapping(mapping);
                }
                if(totalLength == 44){
                    zigbee_query.SelfQuery(byteCommand, length);
                }
                else if(totalLength == 60){
                    Serial.println(shortAddress, HEX);
                    zigbee_query.Node_Query(byteCommand, length);
                }
                else if(totalLength == 63){
                    Serial.println(" Sensor_Node_Query ");
                    Serial.println(shortAddress, HEX);
                    zigbee_query.Sensor_Node_Query(byteCommand, length);
                }
                
                // Move to the next command
                i += totalLength;
            }
            else{
                ++i; // Invalid checksum, move to the next byte
            }
        }
        else {
            break; // Insufficient bytes for the current command, exit loop
        }
    }
}

// Read Short Address Mapping from JSON file
ShortAddressMapping readShortAddressMapping() {
    ShortAddressMapping mapping;
    
    if (!jsonManager.loadJsonFromFile(sensorAddresses)) {
        Serial.println("Failed to load JSON from file");
        return mapping;
    }
    
    JsonObject root = sensorAddresses.as<JsonObject>();
    
    if (!root.containsKey("mappings")) {
        Serial.println("JSON structure is invalid");
        return mapping;
    }

    JsonArray mappingsArray = root["mappings"].as<JsonArray>();
    if (mappingsArray.size() > 0) {
        JsonObject firstMapping = mappingsArray[0];
        mapping.FloorNo = firstMapping["FloorNo"].as<uint8_t>();
        mapping.RoomNo = firstMapping["RoomNo"].as<uint8_t>();
        mapping.SensorID = firstMapping["SensorID"].as<uint8_t>();
        mapping.SensorNo = firstMapping["SensorNo"].as<uint8_t>();
        mapping.ShortAddress = firstMapping["ShortAddress"].as<uint16_t>();
    }

    return mapping;
}


// Save Short Address Mapping to JSON file
void Save_Short_Address_Mapping(const ShortAddressMapping& mapping) {
    if (!jsonManager.loadJsonFromFile(sensorAddresses)) {
        Serial.println("Failed to load JSON from file");
        return;
    }
    
    String shortAddressHex = "0x" + String(mapping.ShortAddress, HEX);
    bool shortAddressExists = false;
    
    for (int i = 0; i < sensorAddresses["mappings"].size(); i++) {
        JsonObject existingMapping = sensorAddresses["mappings"][i];
        if (existingMapping.containsKey("ShortAddress") && String(existingMapping["ShortAddress"].as<String>()) == shortAddressHex) {
            shortAddressExists = true;
            break;
        }
    } 
    if (!shortAddressExists) {
        
       
        JsonObject mappingObj = sensorAddresses["mappings"].createNestedObject();
        mappingObj["FloorNo"] = mapping.FloorNo;
        mappingObj["RoomNo"] = mapping.RoomNo;
        mappingObj["SensorID"] = mapping.SensorID;
        mappingObj["SensorNo"] = mapping.SensorNo;
        mappingObj["ShortAddress"] = shortAddressHex;
        
        if (jsonManager.saveJsonToFile(sensorAddresses)) {
            Serial.println("Sensor Addresses JSON:");
            serializeJsonPretty(sensorAddresses, Serial);
        } else {
            Serial.println("Failed to save JSON to file");
        }
    }
}


// Loop function
void loop() {
    autoconnect.Autoconnect_loop();
    flash_server.handleClient();
    delay(10);
}

// Function to calculate XOR checksum
uint8_t calculateXORChecksum(const uint8_t* array, int startIndex, int endIndex) {
    uint8_t checksum = 0;
  
    for (int i = startIndex; i <= endIndex; i++) {
        checksum ^= array[i];
    }

    return checksum;
}
