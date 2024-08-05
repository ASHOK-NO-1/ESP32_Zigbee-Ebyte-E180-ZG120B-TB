// this code helps to send data to coordinator , based static short address, frame serial number(enviroment id), 
//you also receive command coordinator to do some operation like led control, query this node data like short address, macid, net key etc.,
// if you search query command (55 03 00 00 00) in this (zigbee node) serial  mointer it will still send query to zigbee coordinator

#define FIRMWARE_VERSION  "1.1.12-dev"
#include <Adafruit_Sensor.h>
#include <HardwareSerial.h>
#include <freertos/task.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include "SPIFFS.h"
#include <WiFi.h>         
#include <WebServer.h>    
#include <AutoConnect.h>

#define RX1_PIN 19  // Zigbee RX
#define TX1_PIN 18  // Zigbee TX
#define RX2_PIN 32  // MH-Z19B RX
#define TX2_PIN 33  // MH-Z19B TX

#define BUTTON_PIN 13
#define RED_LED 2

HardwareSerial SerialMhz19b(2);  // MH-Z19B serial
WebServer server;
AutoConnect Portal(server);
AutoConnectConfig config;
AutoConnectUpdate update;
AutoConnectAux Sensor_Settings;
AutoConnectAux* auxPage;
AutoConnectAux* Save;
const char* fw_ver = FIRMWARE_VERSION;

int RED_LED_STATE = LOW;
int button_state;
int last_button_state;

char tempBuffer[3];
uint8_t byteValue[255];
uint8_t Buff[255];
bool jsonDataRead = false;

uint8_t checksum;
const int ledPin = 6;
size_t bufferIndex = 0;

DynamicJsonDocument doc(238);
DynamicJsonDocument doc_query(238);

String sensor_model;
String sensor_number;
String location;
int floorNo;
uint8_t roomNo;
uint8_t sensorId;
uint8_t serialNo;

int CO2;

uint8_t query[] = { 0x55, 0x03, 0x00, 0x00, 0x00 };
//uint8_t transmode[] = { 0x55, 0x07, 0x00, 0x11, 0x00, 0x03, 0x00, 0x01, 0x13};
//uint8_t hexmode[] = {0x2B, 0X2B, 0X2B};

void Read_EspFlash_JsonFile(){
    if (!SPIFFS.begin(true)) {
        Serial.println("An error occurred while mounting SPIFFS");
        // Exit the code
        while(true) {
            delay(1000); // Delay to keep the error message visible
        }
    }
    
    // Load JSON data from file
    File configFile = SPIFFS.open("/sensor_config_file.json", "r"); // r stands for read mode
    if (!configFile) {
        Serial.println("Failed to open config file");
        // Exit the code
        while(true) {
            delay(1000); // Delay to keep the error message visible
        }
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
        Serial.println(error.c_str()); // Print error message
        // Exit the code
        while(true) {
            delay(1000); // Delay to keep the error message visible
        }
    }
    // Accessing values
    sensor_model = doc["SENSOR"]["Sensor_Type"].as<String>();
    sensor_number = doc["SENSOR"]["Sensor_Number"].as<String>();
    location = doc["SENSOR"]["Location"].as<String>();
    floorNo = static_cast<uint8_t>(doc["Map"]["Floor_No"].as<String>().toInt());
    roomNo = static_cast<uint8_t>(doc["Map"]["Room_No"].as<String>().toInt());
    sensorId = static_cast<uint8_t>(doc["Map"]["Sensor_Id"].as<String>().toInt());
    serialNo = static_cast<uint8_t>(doc["Map"]["Serial_No"].as<String>().toInt());

    // Printing values
    Serial.print("Sensor Type: ");
    Serial.println(sensor_model);
    Serial.print("Sensor Number: ");
    Serial.println(sensor_number);
    Serial.print("location: ");
    Serial.println(location);
    Serial.print("Floor Number: ");
    Serial.println(floorNo);
    Serial.print("Room Number: ");
    Serial.println(roomNo);
    Serial.print("Sensor ID: ");
    Serial.println(sensorId);
    Serial.print("Sensor Serial Number: ");
    Serial.println(serialNo);
  
}


void setup() {
   Serial.begin(115200);
   pinMode(BUTTON_PIN, INPUT);
   pinMode(RED_LED, OUTPUT);
   digitalWrite(RED_LED, LOW);
   digitalWrite(BUTTON_PIN, LOW);
   
   Serial1.begin(115200, SERIAL_8N1, RX1_PIN, TX1_PIN);
   SerialMhz19b.begin(9600, SERIAL_8N1, RX2_PIN, TX2_PIN);
   
   delay(2000);
   Serial.println("MH-Z19B CO2 Sensor Initialized");

   xTaskCreatePinnedToCore(ZIGBEE_COMMANDS, "Task1", 30000, NULL, 9, NULL, 0);
   xTaskCreatePinnedToCore(printValues, "Task2", 6000, NULL, 7, NULL, 0);
   xTaskCreatePinnedToCore(WifiMode, "Task3", 10000, NULL, 7, NULL, 0);
}

void ZIGBEE_COMMANDS(void *pvParameters) {
  
  (void) pvParameters; 
  for(;;){
       if (Serial.available()) {
         sendHexCommandToZigbee();
         Serial.println("command succefully written");
       } 
       else{
           vTaskDelay(10);                                                                                                       
       }
       readResponseFromZigbee(); 
  }
} 

void printValues(void *pvParameters) {
    (void) pvParameters;
    for(;;) {
        byte response[9];
        byte cmd[9] = { 0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79 };
        
        while (SerialMhz19b.available()) {
            SerialMhz19b.read();
        }
        
        SerialMhz19b.write(cmd, 9);
        
        if (SerialMhz19b.readBytes(response, 9) == 9) {
            if (response[0] == 0xFF && response[1] == 0x86) {
                CO2 = response[2] * 256 + response[3];
                Serial.print("CO2 Concentration: ");
                Serial.print(CO2);
                Serial.println(" ppm");
                // Call ZIGBEE_Json_data_write with the CO2 value
                ZIGBEE_Json_data_write(CO2);
            } else {
                Serial.println("Invalid response");
            }
        } else {
            Serial.println("No response from sensor");
        }
        
        delay(120000);
    }
}


void ZIGBEE_Json_data_write(int Co2 ) {

            if (!jsonDataRead) {
              Read_EspFlash_JsonFile();
              jsonDataRead = true;
            }
            doc.clear(); 
            doc["Sensor_Type"] = sensor_model;
            doc["Sensor_Number"] = sensor_number;
            doc["Location"] = location;                      // Room 1
            JsonObject data = doc.createNestedObject("data");
            data["CO2"] = CO2;
                      
            char output[238];
            serializeJson(doc, output);
            Serial.println(output);
            //Serial.println();
            //serializeJson(doc, Serial);
           // Serial.println();
           // Serial.print("JSON Length: ");
            //Serial.println(strlen(output));
            //Serial.println();
            // Cast the output array to uint8_t* when calling ZIGBEE_write
            ZIGBEE_write(reinterpret_cast<uint8_t*>(output), strlen(output));
 }




void sendHexCommandToZigbee() {
                                                                                                         // Read the hex command from the Serial Monitor
  String hexCommand = Serial.readStringUntil('\n');

                                                                                                          // Ensure the length is even
  if (hexCommand.length() % 2 != 0) {
    Serial.println("Error: Hex command length must be even.");
    return;
  }
  Serial.print("Hex command length: ");
  Serial.println(hexCommand.length());
                                                                                                            // Send the hex command to Zigbee
  for (size_t i = 0; i < hexCommand.length(); i += 2) {
    tempBuffer[0] = hexCommand[i];
    tempBuffer[1] = hexCommand[i + 1];
                                                                                                           // Convert the two characters to a byte in hex format
    byteValue[bufferIndex] = strtoul(tempBuffer, NULL, 16);
    bufferIndex++;
                                                                                                          //Serial.print(byteValue.length());
   }                                                                                                     // Send the byte to Zigbee E18
    memcpy(Buff, byteValue, bufferIndex);
    for(size_t i=0; i<bufferIndex;i++){
      Serial.print(Buff[i],HEX);
    }
    Serial.println();
    Serial.println(bufferIndex);
    
    Serial1.write(Buff, hexCommand.length() / 2);
    memset(byteValue, 0, sizeof(byteValue));
    memset(Buff, 0, bufferIndex);
    bufferIndex = 0;
   
    
}

void readResponseFromZigbee() {
                                                                                                           // Check for incoming data from Zigbee
 String responseBuffer; 
 uint8_t buff[200];
 int index =0;
 
 while (Serial1.available())                                                                                                     // Check for incoming data from Zigbee
      {
       uint8_t receivedChar = Serial1.read();  
       //Serial.print(receivedChar, HEX);                                                                        // Print the received character in hexadecimal format
       //Serial.print(" ");
       buff[index] = receivedChar;
       responseBuffer += receivedChar;
       responseBuffer +=  " ";
       index++;     
     }
     
  if (!responseBuffer.isEmpty()) {
     //Serial.println();
     processZigbeeCommand(buff, index);
     index = 0;
  }                                                                                 // Add space for better readability
  }
void processZigbeeCommand(uint8_t* byteCommand, size_t length) {
  uint8_t header = byteCommand[0]; 
  uint8_t len = byteCommand[1];
  uint8_t send_confirmation = byteCommand[3];                    //55 10 82 F 0 0 0 1 7 0 8 FC 0 20 D1 0 30 BE (this is command receive from coordinator F ( receive /send) and 30 is data
  int Total_length = len + 2;
  uint8_t checksum_range =Total_length -2;
  uint8_t real_checksum =byteCommand[Total_length-1];
  uint8_t xorChecksum = calculateXORChecksum(byteCommand, 2, checksum_range);
  
  if(header == 0X55 && real_checksum == checksum ){
   /* 
    Serial.println();
    Serial.println("Total_length" + String(Total_length));
    Serial.println("Cal_Checksum" + String(checksum,HEX));
    Serial.println("Real time_Checksum" + String(real_checksum, HEX));*/
    uint16_t ShortAddress = (static_cast<uint16_t>(byteCommand[5]) << 8) | static_cast<uint16_t>(byteCommand[6]);
    uint16_t Query_ShortAddress = (static_cast<uint16_t>(byteCommand[17]) << 8) | static_cast<uint16_t>(byteCommand[18]); 
    
    if(Total_length ==18  && ShortAddress == 0X0000 && send_confirmation == 0x0F){
      //Serial.println();
      Serial.print("ShortAddress: ");
      Serial.println(ShortAddress, HEX);
      processCommand(byteCommand, length);    // control leds and commands
    }
     else if(Total_length == 44 && Query_ShortAddress == 0X9192 ){ // node short address 74 51 hex value
      //Serial.println();
      Serial.println(Query_ShortAddress, HEX);
      ZIGBEE_write(byteCommand, length);      // read query status and send as output to zigbee send command
      
     } 
  }
}

void processCommand(const uint8_t *data, size_t length) {
 
    char data_ = data[length-2];         //55 10 82 F 0 0 0 1 7 0 8 FC 0 20 D1 0 30 BE (this is command receive from coordinator F ( receive /send) and 30 is data, 
    int command = data_;
    switch (command) {
      case '1':
        Serial.print("Command: ");
        Serial.println("LED turned ON");
        Serial.println();
        digitalWrite(RED_LED, HIGH);
        break;
      case '0':
        Serial.print("Command: ");
        Serial.println("LED turned OFF");
        Serial.println();
        digitalWrite(RED_LED, LOW);
        break;
      case '2':
        Serial.print("Command: ");
        Serial.println("QUERY_NODE");
        Serial.println();
        Serial1.write(query, sizeof(query));
        Serial1.flush(); 
        break;
        
      default:
        Serial.println("Unknown command received");
        delay(10);
    }
  }

    
void ZIGBEE_write(uint8_t* output, size_t length) {
  uint8_t xor8checksum;
  uint8_t total_length;
  uint8_t hex[255];  
  uint8_t frame_length;
  hex[0] = 0x55;  // header value
  hex[2] = 0x02;  // command code 
  hex[3] = 0x0F;  // command type( send -0F)
  hex[4] = 0x00;  // transmit mode(i think)
  hex[5] = 0x00;  // short address(2bytes)
  hex[6] = 0x00;  // short address(2bytes)
  hex[7] = 0x01;  // target port
  hex[8] = 0x03;  // serial frame number
  hex[9] = 0x00;  // command direction ( client to server)
  hex[10] = 0x08; // cluster (2bytes)
  hex[11] = 0xFC;  // cluster (2bytes)
  hex[12] = 0x00;  // manufactured id (2bytes)
  hex[13] = 0x20;  // manufactured id (2bytes)
  hex[14] = 0x00;  // response mode
  hex[15] = 0x00;  // trans mode
  hex[16] = floorNo;  // Device Location - Floor no is 1st floor
  hex[17] = roomNo;  // Device Location - Room no is 1
  hex[18] = sensorId;  // Sensor ID
  hex[19] = serialNo;  // Sensor serial number


  memcpy(&hex[20], output, length);
  hex[length + 20] = '}'; // Add the closing curly brace for the JSON object
  hex[length + 21] = '\0'; // Null-terminate the string

  xor8checksum = calculateXORChecksum(hex, 2, length + 17+2+2); //check sum range index 2(count from 0) to 

  hex[length + 18+2+2] = xor8checksum;

  total_length = length + 19+2+2;
  frame_length = total_length -2;
  hex[1] = frame_length;
 
  for (int i = 0; i < total_length; i++) {
    //Serial.print(hex[i], HEX);
    //Serial.print(" ");
  }
   
   //Serial.println();
   Serial1.write(hex, total_length);
   Serial1.flush(); 
   

}
uint8_t calculateXORChecksum(const uint8_t* array, int startIndex, int endIndex) {
  checksum = 0;
  
  for (int i = startIndex; i <= endIndex; i++) {
    checksum ^= array[i];
  }

  return checksum;
}

const static char addonJson[] PROGMEM = R"raw(
[
  {
    "title": "Sensor_Settings",
    "uri": "/sensor_setting",
    "menu": true,
    "element": [
      {
        "name": "header",
        "type": "ACText",
        "value": "<h2>ZigBee Sensor Settings</h2>",
        "style": "text-align:center;color:#2f4f4f;padding:10px;"
      },
      {
        "name": "caption",
        "type": "ACText",
        "value": "<h4>Setting up Sensor location, Sensor ID, Sensor Serial Number</h4>",
        "style": "font-family:serif;color:#4682b4;"
      },
      {
        "name": "SensorType",
        "type": "ACInput",
        "label": "Sensor Type",
        "pattern": "^(([a-zA-Z0-9]|[a-zA-Z0-9][a-zA-Z0-9\\-]*[a-zA-Z0-9])\\.)*([A-Za-z0-9]|[A-Za-z0-9][A-Za-z0-9\\-]*[A-Za-z0-9])$",
        "placeholder": "MH-Z19B"
      },
      {
        "name": "SensorNumber",
        "type": "ACInput",
        "label": "Sensor Number",
        "placeholder": "MH-Z19B_1"
      },
      {
        "name": "DeviceLocation",
        "type": "ACInput",
        "label": "Location",
        "placeholder": "F2_R2"
      },
      {
        "name": "FloorNumber",
        "type": "ACInput",
        "label": "Floor Number",
        "pattern": "^[0-9]{6}$",
        "placeholder": "Range(0~255)"
      },
      {
        "name": "RoomNumber",
        "type": "ACInput",
        "label": "Room Number",
        "pattern": "^[0-9]{6}$",
        "placeholder": "Range(0~255)"
      },
      {
        "name": "SensorID",
        "type": "ACInput",
        "label": "Sensor ID",
        "pattern": "^[0-9]{6}$",
        "placeholder": "Range(0~255) Eg: 4"
      },
      {
        "name": "SensorSerialNumber",
        "type": "ACInput",
        "label": "Sensor Serial Number",
        "pattern": "^[0-9]{6}$",
        "placeholder": "Range(0~255)"
      },
      {
        "name": "newline",
        "type": "ACElement",
        "value": "<hr>"
      },
      {
        "name": "save",
        "type": "ACSubmit",
        "value": "SAVE",
        "uri": "/sensor_save"
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
    "title": "Sensor_Settings",
    "uri": "/sensor_save",
    "menu": false,
    "element": [
      {
        "name": "caption2",
        "type": "ACText",
        "value": "<h4>Save parameters</h4>",
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
        "uri": "/ESP_Restart",
        "style": "font-family:serif;color:#4682b4;"
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
                auxPage  = Portal.aux("/sensor_setting");

                  // Retrieve a server name from an AutoConnectText value.
                AutoConnectInput& Sensor_Type = auxPage->getElement<AutoConnectInput>("SensorType");
                AutoConnectInput& Sensor_Number = auxPage->getElement<AutoConnectInput>("SensorNumber");
                AutoConnectInput& Location = auxPage->getElement<AutoConnectInput>("DeviceLocation");
                
                AutoConnectInput& Floor_No = auxPage->getElement<AutoConnectInput>("FloorNumber");
                AutoConnectInput& Room_No = auxPage->getElement<AutoConnectInput>("RoomNumber");
                AutoConnectInput& Sensor_Id = auxPage->getElement<AutoConnectInput>("SensorID");
                AutoConnectInput& Serial_No = auxPage->getElement<AutoConnectInput>("SensorSerialNumber");
                // Create a JSON document
                StaticJsonDocument<512> doc;
                doc["SENSOR"]["Sensor_Type"] = Sensor_Type.value;
                doc["SENSOR"]["Sensor_Number"] = Sensor_Number.value;
                doc["SENSOR"]["Location"] = Location.value;

                doc["Map"]["Floor_No"] = Floor_No.value;
                doc["Map"]["Room_No"] = Room_No.value;
                doc["Map"]["Sensor_Id"] = Sensor_Id.value;
                doc["Map"]["Serial_No"] = Serial_No.value;

                // Serialize JSON to string
                String jsonString;
                serializeJsonPretty(doc, jsonString);
                Serial.println("sensor Model");
                Serial.println(Sensor_Type.value);

                SPIFFS.begin();
                if (!SPIFFS.begin(true)) {
                    Serial.println("An Error has occurred while mounting SPIFFS");
                    return "";
                }
                File paramFile = SPIFFS.open("/sensor_config_file.json", FILE_WRITE);
                paramFile.println(jsonString);
                paramFile.close();
                Serial.println("Parameters saved to /sensor_config_file.json");
                readParameters();
                SPIFFS.end();
                return jsonString;
                
}

String onEspRestart(AutoConnectAux& aux, PageArgument& args) {

                Serial.println("Restarted Esp Successfully, Prameters succefully updated into Esp Flash Memory");
                String Results;
                Results = "Restarted Esp Successfully, Parameters succefully updated into Esp Flash Memory";
                ESP.restart();
                return Results;
  
}

bool Autoconnect = false;
bool portall = false;

void WifiMode(void *pvParameters) {
  
  (void) pvParameters; 
  for(;;){
       last_button_state = button_state;
       button_state = digitalRead(BUTTON_PIN);
       if (last_button_state == HIGH && button_state == LOW) {
          RED_LED_STATE = !RED_LED_STATE;
          
          if ( RED_LED_STATE == HIGH){
            Serial.println("Blue Light activate");
            //digitalWrite(BLUE_LED, BLUE_LED_STATE);
            digitalWrite(RED_LED, RED_LED_STATE);
            portall = true;
            Autoconnect = true;
            
            
          }else{
            //Serial.println("Red Light activate");
           // digitalWrite(BLUE_LED, BLUE_LED_STATE);
            digitalWrite(RED_LED, RED_LED_STATE);
            portall = false;
            Autoconnect =false;
            stopp(Autoconnect,portall );
            
          } 
         
    }
    vTaskDelay(1000);
  }
}
 
void stopp(bool Autoconnect, bool portall ){
  if(!Autoconnect){
          if(!portall){
             ESP.restart();  // Corrected method to restart the ESP
          }
          Serial.println("Autoconnect falses " );  
     }
}

void loop() {
  
  if(Autoconnect){
          Serial.println("Autoconnect true " );
          if(portall){
            Serial.println("Blu1 Light activate");
            config.apip = IPAddress(192,168,10,101);      // Sets SoftAP IP address
            config.gateway = IPAddress(192,168,10,1);     // Sets WLAN router IP address
            config.netmask = IPAddress(255,255,255,0);    // Sets WLAN scope
            //Portal.config(config);
            config.immediateStart = true;         //AutoConnectConfig::immediateStart is an option to launch the portal by the SoftAP immediately without attempting 1st-WiFi.begin
            config.autoRise = true; 
            config.ota = AC_OTA_BUILTIN;          // ENABLE TO OTA Mode 
            config.otaExtraCaption = fw_ver;      //To display in the add an extra caption to the OTA update screen, sets the AutoConnectConfig::otaExtraCaption by your sketch. display version autoconnect in update screen
 
            Portal.config(config);
            Serial.println("Blu2 Light activate");
            
            auxPage  = Portal.aux("/sensor_setting");
            Save  = Portal .aux("/sensor_save");
            

            Portal.load(addonJson);
            Portal.join({ auxPage, Save });
            
            Portal.on("/sensor_save", onsave);
            Portal.on("/ESP_Restart", onEspRestart);
            //Portal.join(Sensor_Settings);
            
            Portal.begin();
            if(Portal.begin()){
                Serial.println("Blu3 Light activate");
                bool reboot = true;
                update.rebootOnUpdate(reboot);
                update.attach(Portal);
                portall = false;
                Serial.println("Blu4 Light activate");
            }
            
        }else{
         delay(1000);
        }
        //Portal.handleClient();
     }else{
      delay(1000);
     }
}

void readParameters() {
  File param = SPIFFS.open("/sensor_config_file.json", FILE_READ);
  if (!param) {
    Serial.println("Failed to open file for reading");
    return;
  }

  Serial.println("Reading parameters from /param:");
  while (param.available()) {
    String line = param.readStringUntil('\n');
    Serial.println(line);
  }

  param.close();
}