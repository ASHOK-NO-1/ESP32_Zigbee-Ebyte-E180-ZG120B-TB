// this code helps to send data to coordinator , based static short address, frame serial number(enviroment id), 
//you also receive command coordinator to do some operation like led control, query this node data like short address, macid, net key etc.,
// if you search query command (55 03 00 00 00) in this (zigbee node) serial  mointer it will still send query to zigbee coordinator



#define FIRMWARE_VERSION  "1.1.12-dev"


#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <HardwareSerial.h>
#include <freertos/task.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include "SPIFFS.h"

#include <esp_sleep.h>
#include <driver/uart.h>
#include <WiFi.h>         // Replace with WiFi.h for ESP32
#include <WebServer.h>    // Use the WebServer library
#include <AutoConnect.h>


#define RX_PIN 18                                                                                        // Replace with the actual GPIO pin connected to E18 Zigbee RX
#define TX_PIN 17 

#define I2C_SDA 15
#define I2C_SCL 4  

#define BUTTON_PIN    0              
#define GREEN_LED     6
#define RED_LED       5 
#define BLUE_LED      7

#define uS_TO_S_FACTOR 1000000ULL                            /* Conversion factor for micro seconds to seconds */
#define BUTTON_PIN_BITMASK 0x40001                         // GPIO 0 and gpio 18
/*
 * If you want to use GPIO 2 and GPIO 15 as a wake up source, you should do the following:

Calculate 2^2 + 2^15. You should get 32772
Convert that number to hex. You should get: 8004
Replace that number in the BUTTON_PIN_BITMASK as follows:*/

int RED_LED_STATE = HIGH;                                           // the current state of button
int GREEN_LED_STATE = LOW;
int BLUE_LED_STATE   = LOW;
int button_state;                                                   // the current state of button
int last_button_state; 

bool Autoconnect_wakeup = false;
bool Autoconnect = false;
bool portall = false;



RTC_DATA_ATTR int LED_State = LOW;                    
//RTC_DATA_ATTR int bootCount  = 0;

TwoWire I2CBME = TwoWire(0);
Adafruit_BME280 bme;
WebServer server;
AutoConnect Portal(server);
AutoConnectConfig config;            // this fuction contain lot of features, like controlling menu items, ota, design custom pages etc..,
AutoConnectUpdate update;
AutoConnectAux Sensor_Setting;
AutoConnectAux* auxPage;
AutoConnectAux* Save;
const char* fw_ver = FIRMWARE_VERSION;



float temp;
float hum;
char tempBuffer[3];                                                                                      // Temporary buffer to accept two characters at a time
uint8_t byteValue[255];
uint8_t Buff[255];
bool SpiffsData = true; // Static flag to track if JSON data has been read

uint8_t checksum;


size_t bufferIndex = 0;
  
DynamicJsonDocument doc(238);
DynamicJsonDocument doc_query(238);
DynamicJsonDocument feedback_doc(238);


String  sensor_model;
String  sensor_number;
String  location;
int floorNo;
uint8_t roomNo;                             
uint8_t sensorId;
uint8_t serialNo;

                                                                                  // Replace with the actual GPIO pin connected to E18 Zigbee TX
//uint8_t transmode[] = { 0x55, 0x07, 0x00, 0x11, 0x00, 0x03, 0x00, 0x01, 0x13};
uint8_t query[] = { 0x55, 0x03, 0x00, 0x00, 0x00};
//uint8_t hexmode[] = {0x2B, 0X2B, 0X2B};

// Function declarations

void WifiMode(void *pvParameters);
void sendHexCommandToZigbee();
void readResponseFromZigbee();
void ZIGBEE_Json_data_write(float intensity);
void ZIGBEE_COMMANDS(void *pvParameters);
void printValues(void *pvParameters);


void StartAutoconnect(bool Autoconnect, bool portall );
void StopAutoconnect(bool Autoconnect, bool portall );
void readParameters();


void deepSleep() {
    esp_sleep_enable_timer_wakeup(60 * uS_TO_S_FACTOR);
    esp_deep_sleep_start();
}
void hibernate() {
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH,   ESP_PD_OPTION_OFF);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_OFF);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_OFF);
    esp_sleep_pd_config(ESP_PD_DOMAIN_XTAL,         ESP_PD_OPTION_OFF);

    //esp_sleep_pd_config(ESP_PD_DOMAIN_RC_FAST,      ESP_PD_OPTION_OFF);
    //esp_sleep_pd_config(ESP_PD_DOMAIN_VDDSDIO,      ESP_PD_OPTION_OFF);
    //esp_sleep_pd_config(ESP_PD_DOMAIN_MODEM,      ESP_PD_OPTION_OFF);
    //esp_sleep_pd_config(ESP_PD_DOMAIN_MAX,      ESP_PD_OPTION_OFF);
    
    deepSleep();
}

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
        digitalWrite(RED_LED, LOW);
        // Exit the code
        while(true) {
            SpiffsData = false;
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
            SpiffsData = false;
            //delay(10);
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
  /*  Serial.print("Sensor Type: ");
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
    Serial.println(serialNo);*/
  
}

void setup() {
   Serial.begin(115200);
   Serial1.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);
   
   
   // Initialize pins
   pinMode(BUTTON_PIN, INPUT_PULLUP);  
   pinMode(RED_LED, OUTPUT);
   pinMode(GREEN_LED, OUTPUT);                                                                                                          
   pinMode(BLUE_LED, OUTPUT); 

   digitalWrite(RED_LED, LOW);
   digitalWrite(GREEN_LED, LOW);
   digitalWrite(BLUE_LED, LOW);

   
   delay(100);
  
   Serial.println(F("BME280 test"));
   I2CBME.begin(I2C_SDA, I2C_SCL, 100000);
   
   bool status;
   status = bme.begin(0x77, &I2CBME);  
   if (!status) {
     Serial.println("Could not find a valid BME280 sensor, check wiring!");
     while (1);
   }
   Serial.println("-- BME280 sensor found --");
   Serial.println();
   


   //delay(1000);
              
   // Create FreeRTOS tasks  
   xTaskCreatePinnedToCore(ZIGBEE_COMMANDS, "Task1", 30000, NULL, 9, NULL, 0);
   xTaskCreatePinnedToCore(printValues, "Task2", 10000, NULL, 7, NULL, 0);
   xTaskCreatePinnedToCore(WifiMode, "Task3", 10000, NULL, 7, NULL, 0);

   Read_EspFlash_JsonFile();
   
   // Configure wake-up sources
   esp_sleep_enable_ext1_wakeup(BUTTON_PIN_BITMASK,ESP_EXT1_WAKEUP_ALL_LOW);     // GPIO 0 AND GPIO 18 , if these two gpio pins goes to low, it will wake up ESP32 in sleep mode
                                    
   // Determine the wake-up reason
   esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

   // Handle different wake-up causes
   switch (wakeup_reason) {
        case ESP_SLEEP_WAKEUP_TIMER:
        
            Serial.println("TIMER: Woke up from deep sleep.");
                        
            feedback_doc["Feedback"] = LED_State;  // LED INTILAIZATION FOR NODE-UI
            char outputs[238];
            serializeJson(feedback_doc, outputs);
            Serial.println(outputs);
            // Cast the output array to uint8_t* when calling ZIGBEE_write
            ZIGBEE_write(reinterpret_cast<uint8_t*>(outputs), strlen(outputs));

            digitalWrite(GREEN_LED, LED_State);
            TIMERDeepSleepWakeUp();
            Serial1.flush();
            break;

        case ESP_SLEEP_WAKEUP_EXT1:
        
            if (digitalRead(GPIO_NUM_0) == LOW) {
                  Serial.println("BUTTON: Woke up from deep sleep.");
                  digitalWrite(GREEN_LED, LED_State);
                  Autoconnect_wakeup =true;
                  ButtonWakeUp();
            }else {
                  Serial.println("UART(RX): Woke up from deep sleep.");
                  //delay(100);                                           // Short delay to ensure stability
                  Serial1.flush();
                  readResponseFromZigbee();
                  delay(5000);
                  hibernate();
                  //esp_deep_sleep_start();
                  
            }
            
            break;

        case ESP_SLEEP_WAKEUP_UART:
            Serial.println("UART(UART_NUM_1): Woke up from deep sleep.");
            digitalWrite(BLUE_LED, HIGH);
            Serial1.flush();
          
            break;

        default:
            Serial.println("wakeup_reason ");
            Serial.println(wakeup_reason);
            break;
    }
   
   
   if(!Autoconnect){
      Serial.println("Set up: going to deep sleep.");
      hibernate();
      //esp_deep_sleep_start();
      //esp_light_sleep_start();
   }
   
}


void ButtonWakeUp() {
     
    WifiMode(NULL);                                                                // declare all function, this function is free RTOS, it run like a loop, so, it doesnot exit function
    delay(100);
}

void TIMERDeepSleepWakeUp() {
    
    DeepSleepSensorRead();
    delay(100); 
   
    if(!Autoconnect){
      Serial.println("Timer : Going back to deep sleep");
      hibernate();
    }
    
}

void DeepSleepSensorRead(){

     if(SpiffsData){   
        temp = bme.readTemperature();
        hum  = bme.readHumidity();
        Serial.print("Temperature: ");
        Serial.println(temp);
        Serial.print("Humidity: ");
        Serial.println(hum);
        ZIGBEE_Json_data_write(temp, hum);
        //Serial.println(); 
        delay(10000);
     }     
}

void ZIGBEE_COMMANDS(void *pvParameters) {
  
  (void) pvParameters; 
  for(;;){
       if (Serial.available()) {
         sendHexCommandToZigbee();
       } 
       else{
        
        vTaskDelay(10);                                                                                                       
       }
       readResponseFromZigbee(); 
  }
} 

void printValues(void *pvParameters) {
  (void) pvParameters; 
  for(;;){
      
        delay(13000);
        temp = bme.readTemperature();
        hum  = bme.readHumidity();
        Serial.print("Temperature: ");
        Serial.println(temp);
        Serial.print("Humidity: ");
        Serial.println(hum);
        ZIGBEE_Json_data_write(temp, hum);
        //Serial.println(); 
     
                             
}
}


void ZIGBEE_Json_data_write(float temp, float hum) {
  
       if(SpiffsData){
            doc.clear(); 
            doc["Sensor_Type"] = sensor_model;
            doc["Sensor_Number"] = sensor_number;
            doc["Location"] = location;
                                 
            // Format the float values with two decimal places
            String tempStr = String(temp, 2);
            String humStr = String(hum, 2);
            JsonObject data = doc.createNestedObject("data");
            data["temperature"] = tempStr; 
            data["humidity"] = humStr;             
            char output[238];
            serializeJson(doc, output);
            Serial.println(output);
            
            /*Serial.println();
            //serializeJson(doc, Serial);
           // Serial.println();
            Serial.print("JSON Length: ");
            Serial.println(strlen(output));
            Serial.println();*/
            // Cast the output array to uint8_t* when calling ZIGBEE_write
            ZIGBEE_write(reinterpret_cast<uint8_t*>(output), strlen(output));
       }
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
         /*  for(int i=0; i<length; i++){
            Serial.print(byteCommand[i], HEX);
            Serial.print(" ");
           }*/
           Serial.print("ShortAddress: ");
           Serial.println(ShortAddress, HEX);
           processCommand(byteCommand, length);    // control leds and commands
    }
     else if(Total_length == 44){
           //Serial.println();
           Serial.print("QueryShortAddress: ");
           Serial.println(Query_ShortAddress, HEX);
           ZIGBEE_write(byteCommand, length);      // read query status and send as output to zigbee send command
      
     } 
  }
}

void processCommand(const uint8_t *data, size_t length) {
  
    feedback_doc.clear();  // Clear the JSON document at the start of each call
    char data_ = data[length-2];         //55 10 82 F 0 0 0 1 7 0 8 FC 0 20 D1 0 30 BE (this is command receive from coordinator F ( receive /send) and 30 is data, 
    int command = data_;
    switch (command) {
      case '1':
        Serial.print("Command: ");
        Serial.println("LED turned ON");
        Serial.println();
        LED_State = HIGH;
        digitalWrite(GREEN_LED, LED_State);
        feedback_doc["Feedback"] = LED_State;
        
        break;
      case '0':
        Serial.print("Command: ");
        Serial.println("LED turned OFF");
        Serial.println();
        LED_State = LOW;
        digitalWrite(GREEN_LED, LED_State);
        feedback_doc["Feedback"] = LED_State;
        
        break;
      case '2':
        Serial.print("Command: ");
        Serial.println("QUERY_NODE");
        Serial.println();
        Serial1.write(query, sizeof(query));
        Serial1.flush();
       // return;  // No feedback to be sent for QUERY_NODE
        break;
        
      default:
        Serial.println("Unknown command received");
        delay(10);
       // return;  // No feedback to be sent for QUERY_NODE
    }
    char outputs[238];
    serializeJson(feedback_doc, outputs);
    Serial.println(outputs);
    // Cast the output array to uint8_t* when calling ZIGBEE_write
    ZIGBEE_write(reinterpret_cast<uint8_t*>(outputs), strlen(outputs));
    delay(1000);
    
  }

 
void ZIGBEE_write(uint8_t* output, size_t length) {
    uint8_t xor8checksum;
    uint8_t total_length;
    uint8_t hex[255]; // Increase array size to accommodate extra bytes (255 + 6)
    uint8_t frame_length;
    uint8_t full_length;
    // Initialize wake up dormnant node adding 6bytes (0x00) for 115200 baurd rate zigbee
    hex[0] = 0x00;
    hex[1] = 0x00;
    hex[2] = 0x00;
    hex[3] = 0x00;
    hex[4] = 0x00;
    hex[5] = 0x00;

    // Initialize existing header and command bytes
    hex[6] = 0x55;  // header value
    hex[8] = 0x02;  // command code 
    hex[9] = 0x0F;  // command type (send - 0F)
    hex[10] = 0x00; // transmit mode (assuming it's 0)
    hex[11] = 0x00; // short address (2 bytes)
    hex[12] = 0x00; // short address (2 bytes)
    hex[13] = 0x01; // target port
    hex[14] = 0x02; // serial frame number
    hex[15] = 0x00; // command direction (client to server)
    hex[16] = 0x08; // cluster (2 bytes)
    hex[17] = 0xFC; // cluster (2 bytes)
    hex[18] = 0x00; // manufacturer id (2 bytes)
    hex[19] = 0x20; // manufacturer id (2 bytes)
    hex[20] = 0x00; // response mode
    hex[21] = 0x00; // trans mode
    hex[22] = floorNo;  // Device Location - Floor no (assuming single byte)
    hex[23] = roomNo;   // Device Location - Room no (assuming single byte)
    hex[24] = sensorId; // Sensor ID (assuming single byte)
    hex[25] = serialNo; // Sensor serial number (assuming single byte)

    // Copy output data into hex array
    memcpy(&hex[26], output, length);
    hex[length + 26] = '}'; // Add the closing curly brace for the JSON object
    hex[length + 27] = '\0'; // Null-terminate the string

    // Calculate and add XOR checksum
    xor8checksum = calculateXORChecksum(hex, 8, length + 21 + 6); // Adjust index for checksum calculation
    hex[length + 24+ 2+2] = xor8checksum;

    // Calculate total and frame length
    total_length = length + 19+2+2;
    frame_length = total_length - 2;
    hex[7] = frame_length; // Update frame length in hex array

    full_length = length + 25 +2 +2;
    Serial.println("Sending command to Zigbee Coordinator");
    Serial.println();
    for (int i = 0; i < full_length; i++) {
      Serial.print(hex[i], HEX);
      Serial.print(" ");
    }
    
   
    Serial1.write(hex, full_length);
    Serial.println("Send command to Zigbee Coordinator Successfully");
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
        "placeholder": "BME280"
      },
      {
        "name": "SensorNumber",
        "type": "ACInput",
        "label": "Sensor Number",
        "placeholder": "BME280_1"
      },
      {
        "name": "DeviceLocation",
        "type": "ACInput",
        "label": "Location",
        "placeholder": "F1_R1"
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
        "placeholder": "Range(0~255) Eg: 1"
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


void WifiMode(void *pvParameters) {
  
  (void) pvParameters; 
  for(;;){
       //Serial.println("Button activate");
       last_button_state = button_state;
       button_state = digitalRead(BUTTON_PIN);
       if (last_button_state == HIGH && button_state == LOW) {
          RED_LED_STATE = !RED_LED_STATE;
          BLUE_LED_STATE = !BLUE_LED_STATE;
          if ( BLUE_LED_STATE == HIGH){
            Serial.println("Blue Light activate");
            digitalWrite(GREEN_LED, LOW);
            digitalWrite(BLUE_LED, BLUE_LED_STATE);
            digitalWrite(RED_LED, RED_LED_STATE);
            portall = true;
            Autoconnect = true;
            //StartAutoconnect(Autoconnect,portall);
            if (Autoconnect) {
                xTaskCreate(StartAutoconnect, "StartAutoconnect", 8192, NULL, 5, NULL);
              }
            }
            
          else{
            Serial.println("Red Light activate");
            digitalWrite(BLUE_LED, BLUE_LED_STATE);
            digitalWrite(RED_LED, RED_LED_STATE);
            portall = false;
            Autoconnect =false;
            StopAutoconnect(Autoconnect,portall );
            
          } 
       }
         vTaskDelay(1000);
    }
    
} 
//void StartAutoconnect(bool Autoconnect, bool portall) {
void StartAutoconnect(void *pvParameters) {
  while (Autoconnect ) {
  //if(Autoconnect || SpiffsData) {
    Serial.println("Autoconnect true");
    if (portall) {
      Serial.println("WIFI SSID: esp32ap");
      config.apip = IPAddress(192, 168, 10, 101);  // Sets SoftAP IP address
      config.gateway = IPAddress(192, 168, 10, 1); // Sets WLAN router IP address
      config.netmask = IPAddress(255, 255, 255, 0); // Sets WLAN scope
      config.immediateStart = true;  // Launch portal by SoftAP immediately
      config.autoRise = true;
      config.ota = AC_OTA_BUILTIN;  // Enable OTA mode
      config.otaExtraCaption = fw_ver; // Display version in update screen
    
      Portal.config(config);

      auxPage = Portal.aux("/sensor_setting");
      Save = Portal.aux("/sensor_save");

      Portal.load(addonJson);
      Portal.join({ auxPage, Save });

      Portal.on("/sensor_save", onsave);
      Portal.on("/ESP_Restart", onEspRestart);

      if (Portal.begin()) {
        bool reboot = true;
        update.rebootOnUpdate(reboot);
        update.attach(Portal);
      }
    } else {
      delay(1000);
    }

    // Break out of the loop if Autoconnect is turned off
    if (!Autoconnect) break;
  }
}

void StopAutoconnect(bool Autoconnect, bool portall ){
  if(!Autoconnect){
          if(!portall){
             ESP.restart();  // Corrected method to restart the ESP
          }
          Serial.println("Autoconnect falses " );  
     }
}

void loop() {
     
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
