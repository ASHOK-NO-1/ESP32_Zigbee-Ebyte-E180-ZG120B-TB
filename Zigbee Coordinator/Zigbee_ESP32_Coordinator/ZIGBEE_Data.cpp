#include "ZIGBEE_Data.h"
#include "arduino.h"                // arduino.h helps access arduino core function access
#include <Arduino.h>
#include <WiFi.h>

#include <HardwareSerial.h>
#include <freertos/task.h>
#include <ArduinoJson.h>

#include "ESPFileServer.h"

extern WebServer server;

#define FIRMWARE_VERSION  "1.1.12-dev"
const char* fw_ver = FIRMWARE_VERSION; 
#include <AutoConnect.h>
AutoConnect Portal(server);
AutoConnectConfig config; 
AutoConnectUpdate update;
AutoConnectAux MQTT_Setting;                                                  // adding the current version of the sketch to the OTA caption.
AutoConnectAux* auxPage;
AutoConnectAux* Save;





WiFiClient espClient;
PubSubClient client(espClient);

extern void Process_Zigbee_Command(const uint8_t* byteCommand, size_t length);

// Constructor for MQTT class
MQTT::MQTT(const char* mqtt_broker, int mqtt_port) {
    
    _mqtt_broker = mqtt_broker;
    _mqtt_port = mqtt_port;
}

// Constructor for ZIGBEE class
ZIGBEE::ZIGBEE(int rx_pin, int tx_pin) {
    _rx_pin = rx_pin;
    _tx_pin = tx_pin;
}

// Constructor for ZIGBEE_Data class
ZIGBEE_Data::ZIGBEE_Data(uint8_t Floor_No, uint8_t Room_No, uint8_t Sensor_ID, uint8_t Sensor_No, uint16_t Short_Address) {
    _Floor_No      = Floor_No;
    _Room_No       = Room_No;
    _Sensor_ID     = Sensor_ID;
    _Sensor_No     = Sensor_No;
    _Short_Address = Short_Address;
}




const static char addonJson[] PROGMEM = R"raw(
[
  {
    "title": "MQTT Setting",
    "uri": "/mqtt_setting",
    "menu": true,
    "element": [
      {
        "name": "header",
        "type": "ACText",
        "value": "<h2>MQTT broker settings</h2>",
        "style": "text-align:center;color:#2f4f4f;padding:10px;"
      },
      {
        "name": "caption",
        "type": "ACText",
        "value": "<h4>Set up MQTT Parameters</h4>",
        "style": "font-family:serif;color:#4682b4;"
      },
  
      {
        "name": "MqttID/Ip",
        "type": "ACInput",
        "value": "",
        "label": "MQTT Server / IP",
        "pattern": "^(([a-zA-Z0-9]|[a-zA-Z0-9][a-zA-Z0-9\\-]*[a-zA-Z0-9])\\.)*([A-Za-z0-9]|[A-Za-z0-9][A-Za-z0-9\\-]*[A-Za-z0-9])$",
        "placeholder": "MQTT broker server"
        
      },
      {
        "name": "MqttPortNo",
        "type": "ACInput",
        "label": "Port No",
        "pattern": "^[0-9]{6}$",
        "placeholder": "1883"
      },
      {
        "name": "MqttUsername",
        "type": "ACInput",
        "label": "Username",
        "placeholder": "XXXXXX"
      },
      {
        "name": "MqttPassword",
        "type": "ACInput",
        "label": "Password",
        "placeholder": "XXXXXX"
      },
      {
        "name": "newline",
        "type": "ACElement",
        "value": "<hr>"
      },
      {
        "name": "save",
        "type": "ACSubmit",
        "value": "Save",
        "uri": "/mqtt_save"
      },
      {
        "name": "MainPage",
        "type": "ACSubmit",
        "value": "Main Page",
        "uri": "/_ac"
      }
    ]
  },
  {
    "title": "MQTT Setting",
    "uri": "/mqtt_save",
    "menu": false,
    "element": [
      {
        "name": "caption2",
        "type": "ACText",
        "value": "<h4>Parameters saved as:</h4>",
        "style": "text-align:center;color:#2f4f4f;padding:10px;"
      },
      {
        "name": "parameters",
        "type": "ACText"
      },
      {
        "name": "EspRestart",
        "type": "ACSubmit",
        "value": "ESP Restart",
        "uri": "/ESP_Restart"
       
      },
      {
        "name": "MainPage",
        "type": "ACSubmit",
        "value": "Main Page",
        "uri": "/_ac"
      }    
    ]
    
  },
   {
    "title": "ESP Restart",
    "uri": "/ESP_Restart",
    "menu": true,
    "element": []
  } 
]
)raw";

String onsave(AutoConnectAux& aux, PageArgument& args) {
  // Open a parameter file on the SPIFFS.
                
                // Get a '/sensor_setting' page
               auxPage  = Portal.aux("/mqtt_setting");

                  // Retrieve a server name from an AutoConnectText value.
                AutoConnectInput& Mqtt_ID = auxPage->getElement<AutoConnectInput>("MqttID/Ip");
                AutoConnectInput& MqttPort_No = auxPage->getElement<AutoConnectInput>("MqttPortNo");
                AutoConnectInput& Mqtt_Username = auxPage->getElement<AutoConnectInput>("MqttUsername");
                
                AutoConnectInput& Mqtt_Password = auxPage->getElement<AutoConnectInput>("MqttPassword");
                
                // Create a JSON document
                StaticJsonDocument<512> doc;
                doc["MQTT"]["MqttID"] = Mqtt_ID.value;
                doc["MQTT"]["MqttPortNo"] = MqttPort_No.value;
                doc["MQTT"]["MqttUsername"] = Mqtt_Username.value;
                doc["MQTT"]["MqttPassword"] = Mqtt_Password.value;
                
                
                // Serialize JSON to string
                String jsonString;
                serializeJsonPretty(doc, jsonString);
                Serial.println("MqttID Name");
                Serial.println(Mqtt_ID.value);

                SPIFFS.begin();
                if (!SPIFFS.begin(true)) {
                    Serial.println("An Error has occurred while mounting SPIFFS");
                    return "";
                }
                File paramFile = SPIFFS.open("/co-ordinator_config.json", FILE_WRITE);
                paramFile.println(jsonString);
                paramFile.close();
                Serial.println("Parameters saved to /co-ordinator_config.json.json");
                
                //Read parameters
                File param = SPIFFS.open("/co-ordinator_config.json", FILE_READ);
                if (!param) {
                    Serial.println("Failed to open file for reading");
                    return"";
                  }

                Serial.println("Reading parameters from /param:");
                while (param.available()) {
                String line = param.readStringUntil('\n');
                Serial.println(line);
              }

                param.close();
                
                SPIFFS.end();
                return jsonString;
                
}

String onEspRestart(AutoConnectAux& aux, PageArgument& args) {

                Serial.println("Restarted Esp Successfully, Prameters succefully updated into Esp Flash Memory");
                String Results;
                Results = "Restarted Esp Successfully, Prameters succefully updated into Esp Flash Memory";
                ESP.restart();
                return Results;
  
}

void Autoconnect::Autoconnect_loop(){
  Portal.handleClient();
}

// Begin WiFi connection for MQTT
void MQTT::begin() {
    Serial.begin(115200);

    
    config.ota = AC_OTA_BUILTIN;          // ENABLE TO OTA Mode 
    //Portal.config(config);
    config.otaExtraCaption = fw_ver;      //To display in the add an extra caption to the OTA update screen, sets the AutoConnectConfig::otaExtraCaption by your sketch. display version autoconnect in update screen
    Portal.config(config);   
    
    auxPage  = Portal.aux("/mqtt_setting");
    Save  = Portal .aux("/mqtt_save");
    
    Portal.load(addonJson);
    Portal.join({ auxPage, Save });       
    Portal.on("/mqtt_save", onsave);
    Portal.on("/ESP_Restart", onEspRestart);
    
    //Portal.join(MQTT_Setting);
    Portal.begin();
    if (Portal.begin()) {
                Serial.println("WiFi connected: " + WiFi.localIP().toString());  
  } 
    client.setServer(_mqtt_broker, _mqtt_port);
    Serial.println(_mqtt_broker);
    Serial.println(_mqtt_port);
    int i =0;
    while (!client.connected()) {
        
        Portal.handleClient();
        String client_id = "id:";
        client_id += String(WiFi.macAddress());
        Serial.printf("The client %s connects to the public mqtt broker\n", client_id.c_str());
        if (client.connect(client_id.c_str())) {
            Serial.println("Public mosquitto mqtt broker connected");
        } else {
            Serial.print("failed with state, rc =  ");
            Serial.println(client.state());
            Serial.println("Mosquitto mqtt broker IS NOT connected");
            delay(2000);
            i++;
            Serial.println(i);
            if(i>150){                         // every five minutes restart
              ESP.restart();
            }
        }
    }
    
}

void MQTT::MQTT_Client(){
  
}


// Reconnect MQTT client
void MQTT::reconnect_client() {
   client.setServer(_mqtt_broker, _mqtt_port);
    while (!client.connected()) {

        String client_id = "id:";
        client_id += String(WiFi.macAddress());
        //client.connect(client_id.c_str());
        Serial.printf("The client %s connects to the public mqtt broker\n", client_id.c_str());
        if (client.connect(client_id.c_str())) {
            Serial.println("Public mosquitto mqtt broker connected");
        } else {
            Serial.print("failed with state, rc =  ");
            Serial.println(client.state());
            Serial.print(_mqtt_broker);
            Serial.println(_mqtt_port);
            Serial.println("Mosquitto mqtt broker IS NOT connected");
            delay(5000);
            ESP.restart();
        }
    }
}

// Reconnect WiFi
void MQTT::reconnect_wifi() {
  /*  if (WiFi.status() != WL_CONNECTED) {
        WiFi.begin(_ssid, _password);
        Serial.println("WiFi connection lost. Reconnecting");
        WiFi.disconnect();
        WiFi.reconnect();
        delay(2000);
        
        while (WiFi.status() != WL_CONNECTED) {
            Serial.print(".");
            delay(1000);
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("Reconnected to WiFi");
            Serial.print("IP Address: ");
            Serial.println(WiFi.localIP());
        } else {
            Serial.println("Failed to reconnect to WiFi");
        }
    }*/

   /* if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Failed to reconnect to WiFi");
      Portal.begin();
      if (Portal.begin()) {
                Serial.println("WiFi connected: " + WiFi.localIP().toString());  
        }
      delay(120000);
    }*/
}

// Begin Zigbee communication
void ZIGBEE::begin() {
    Serial.begin(115200);
    Serial1.begin(115200, SERIAL_8N1, _rx_pin, _tx_pin);
    Serial.println();
}

void ZIGBEE_Data:: Serial_write_Zigbee(){
  
  if(Serial.available()){
      String Serial_hexCommand;
      char tempBuffer[3];                                                                                                  // Temporary buffer to accept two characters at a time
      uint8_t byteValue[255];
      uint8_t Buff[255];
      size_t bufferIndex = 0;
      
      Serial_hexCommand = Serial.readStringUntil('\n');                                                                                                    // Ensure the length is even
      if (Serial_hexCommand.length() % 2 != 0) {
        Serial.println("Error: Hex command length must be even.");
        return;
      }
      Serial.print("Hex command length: ");
      Serial.println(Serial_hexCommand.length());
                                                                                                                             // Send the hex command to Zigbee
      for (size_t i = 0; i < Serial_hexCommand.length(); i += 2) {
        tempBuffer[0] = Serial_hexCommand[i];
        tempBuffer[1] = Serial_hexCommand[i + 1];                                                                                                   // Convert the two characters to a byte in hex format
        byteValue[bufferIndex] = strtoul(tempBuffer, NULL, 16);
        bufferIndex++;                                                                                                      //Serial.print(byteValue.length());
      }                                                                                                                    // Send the byte to Zigbee E18
      memcpy(Buff, byteValue, bufferIndex);
      for(size_t i=0; i<bufferIndex;i++){
        Serial.print(Buff[i],HEX);
      }
      Serial.println();
      Serial.println(bufferIndex);
    
      Serial1.write(Buff, Serial_hexCommand.length() / 2);
      memset(byteValue, 0, sizeof(byteValue));
      memset(Buff, 0, bufferIndex);
      bufferIndex = 0;
      Serial.println(" ");   
}

}

void ZIGBEE_Data:: Serial_read_Zigbee(){
       unsigned long startTime = millis();
       String responseBuffer; // Buffer to store the response
       uint8_t buff[256];
       int index =0;
       while (Serial1.available()) { 
             uint8_t receivedChar = Serial1.read();  
             //Serial.print(receivedChar, HEX);                                                                       
             //Serial.print(" ");
             buff[index] = receivedChar;
             responseBuffer += receivedChar;
             responseBuffer +=  " ";
             index++;
           
           }
       if (!responseBuffer.isEmpty()) {
           //Serial.println();
           Serial1.flush();
           Process_Zigbee_Command(buff, index);
           index = 0;
  }                                                                                                    
  }

// Read zigbee json data
void ZIGBEE_Data::Read_Zigbee_Json_data(const uint8_t* byteCommand, size_t length) {
    DynamicJsonDocument ReadDoc(256);
    char serializedJson[256];
    char json_data[200];

    int len = length - 21;
    for (int j = 0; j < len; j++) {
        json_data[j] = byteCommand[20 + j];    
    }
    json_data[len] = '\0';                                                                                         // Null-terminate the string
    DeserializationError error = deserializeJson(ReadDoc, json_data);                                                  // Deserialize JSON data
    if (error) {
      //Serial.print("JSON parsing failed: ");
        return;
    }
    size_t serializedSize = serializeJson(ReadDoc, serializedJson, sizeof(serializedJson));                            // Serialize the JSON document into a string
    if (serializedSize == 0) {
        Serial.println("Failed to serialize JSON data.");
        return;
    }
    //Serial.println(serializedJson); 
    ReadDoc.clear();
}

// Read and publish json data together to mqtt
void ZIGBEE_Data::Read_Publish_Zigbee_Json_data(const uint8_t* byteCommand, size_t length, uint8_t floorNo, uint8_t roomNo, uint8_t sensorID, uint8_t sensorNo, uint16_t ShortAddress) {
    DynamicJsonDocument PublishDoc(256);
    char serializedJson[256];
    char dynamicTopic[50]; 
    int len = length - 21;
    char json_data[200];
    for (int j = 0; j < len; j++) {
        json_data[j] = byteCommand[20 + j]; 
           
    }
    json_data[len] = '\0';                                                                                                      // Null-terminate the string
    
    DeserializationError error = deserializeJson(PublishDoc, json_data);                                                              // Deserialize JSON data
    if (error) {
        //Serial.print("JSON parsing failed: ");
        //Serial.println(error.c_str());
        return;
    } 
    if(client.connected()) {                                                                                                    
        snprintf(dynamicTopic, sizeof(dynamicTopic), "ESP/ZIGBEE/%02X/%02X/%02X/%02X", floorNo, roomNo, sensorID, sensorNo);  // create dynamic topic based on env and espid       
        size_t serializedSize = serializeJson(PublishDoc, serializedJson, sizeof(serializedJson));                                      // Serialize the JSON document into a string
        if (serializedSize == 0) {
            Serial.println("Failed to serialize JSON data.");
            return;
        }
        Serial.println(serializedJson);
        Serial.print("dynamicTopic: ");
        Serial.println(dynamicTopic);
        Serial.print("Floor_No: ");
        Serial.println(floorNo, HEX);
        Serial.print("Room_No: ");
        Serial.println(roomNo, HEX);
        Serial.print("Sensor_ID: ");
        Serial.println(sensorID, HEX);
        Serial.print("Sensor_No: ");
        Serial.println(sensorNo, HEX);
        client.publish(dynamicTopic, serializedJson);                                                                            // Publish the serialized JSON data
        
        _Floor_No      = floorNo;
        _Room_No       = roomNo;
        _Sensor_ID     = sensorID;
        _Sensor_No     = sensorNo;
        _Short_Address = ShortAddress;
        
        
    } else {
        client.disconnect();
        Serial.println("Failed to connect to MQTT broker.");
    }
    PublishDoc.clear();
}



void ZIGBEE_Query:: SelfQuery(const uint8_t* byteCommand, size_t length){
            
            Serial.println("zigbee coordinator Query:");
            device_type = byteCommand[5];
            channel     = byteCommand[14];
            memcpy(mac_id, byteCommand+6, 8);
            memcpy(pan_id, byteCommand+15, 2);
            memcpy(short_address, byteCommand+17, 2);
            memcpy(expand_pan_id, byteCommand+19, 8);
            memcpy(net_key, byteCommand+27, 16);
            flag_zigbee_query = true;
}

void ZIGBEE_Query:: Sensor_Node_Query(const uint8_t* byteCommand, size_t length){
            
            Serial.println("zigbee sensor Node Query:");
            device_type = byteCommand[23];
            channel     = byteCommand[32];
            memcpy(mac_id, byteCommand+39, 8);
            memcpy(pan_id, byteCommand+33, 2);
            memcpy(short_address, byteCommand+35, 2);
            memcpy(expand_pan_id, byteCommand+37, 8);
            memcpy(net_key, byteCommand+45, 16);
            flag_zigbee_query = true;
}

void ZIGBEE_Query:: Node_Query(const uint8_t* byteCommand, size_t length){
            
            Serial.println("zigbee Node Query:");
            device_type = byteCommand[21];
            channel     = byteCommand[30];
            memcpy(mac_id, byteCommand+37, 8);
            memcpy(pan_id, byteCommand+31, 2);
            memcpy(short_address, byteCommand+33, 2);
            memcpy(expand_pan_id, byteCommand+35, 8);
            memcpy(net_key, byteCommand+43, 16);
            flag_zigbee_query = true; 
}


void ZIGBEE_Query:: Query(){
       if (flag_zigbee_query) {
            DynamicJsonDocument doc_query(238);
            doc_query.clear();
            doc_query["Status"] = "QUERY";
            doc_query["device_type"] = String(device_type, HEX);
            doc_query["channel"] = String(channel, HEX);

            char mac_idString[17]; 
            for (int i = 0; i < 8; ++i) {
                sprintf(mac_idString + i * 2, "%02X", mac_id[i]);
            }
            doc_query["mac_id"] = mac_idString;

            char pan_idString[5]; 
            for (int i = 0; i < 2; ++i) {
                sprintf(pan_idString + i * 2, "%02X", pan_id[i]);
            }
            doc_query["pan_id"] = pan_idString;

            char short_addressString[5]; 
            for (int i = 0; i < 2; ++i) {
                sprintf(short_addressString + i * 2, "%02X", short_address[i]);
            }
            doc_query["short_address"] = short_addressString;

            char expand_pan_idString[5]; 
            for (int i = 0; i < 2; ++i) {
                sprintf(expand_pan_idString + i * 2, "%02X", expand_pan_id[i]);
            }
            doc_query["expand_pan_id"] = expand_pan_idString;

                                             
            char net_keyString[33]; // Each byte represented by 2 characters, plus the null terminator
            for (int i = 0; i < 16; ++i) {
                sprintf(net_keyString + i * 2, "%02X", net_key[i]);
            }
            doc_query["net_key"] = net_keyString;
            
            

            char output[238];

            //doc.shrinkToFit();  // optional

            serializeJson(doc_query, output);
            //Serial.println();
            //serializeJson(doc_query, Serial);
            serializeJsonPretty(doc_query, Serial);
            Serial.println();
            flag_zigbee_query = false;
       }
}
