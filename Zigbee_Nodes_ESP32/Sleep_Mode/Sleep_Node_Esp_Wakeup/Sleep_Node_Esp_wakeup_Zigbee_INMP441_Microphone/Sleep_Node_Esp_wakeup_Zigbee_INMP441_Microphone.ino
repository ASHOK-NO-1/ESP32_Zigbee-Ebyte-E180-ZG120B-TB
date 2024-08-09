// this code helps to send data to coordinator , based static short address, frame serial number(enviroment id), 
//you also receive command coordinator to do some operation like led control, query this node data like short address, macid, net key etc.,
// if you search query command (55 03 00 00 00) in this (zigbee node) serial  mointer it will still send query to zigbee coordinator
//PROBLEMS


#define FIRMWARE_VERSION  "1.1.12-dev"
#include <driver/i2s.h>
#include <arduinoFFT.h>

#include <Wire.h>


#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <HardwareSerial.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include "SPIFFS.h"

#include <WiFi.h>         // Replace with WiFi.h for ESP32
#include <WebServer.h>    // Use the WebServer library
#include <AutoConnect.h>


#define RX_PIN 18                                                                                        // Replace with the actual GPIO pin connected to E18 Zigbee RX
#define TX_PIN 17 

// I2S pin definitions for INMP441 microphone
#define I2S_WS 2
#define I2S_SD 1
#define I2S_SCK 42


// I2S port definition
#define I2S_PORT I2S_NUM_0

// Input buffer length
#define BUFFER_LEN 1024
int16_t sampleBuffer[BUFFER_LEN]; // Buffer for 1024 samples (1024 samples * 16-bit = 2048 bytes)

// FFT configuration
#define SAMPLES 1024              // Must be a power of 2
#define SAMPLING_FREQUENCY 44100  // Hz, must be the same as set in I2S configuration

// FFT arrays
double vReal[SAMPLES];
double vImag[SAMPLES];

// Create FFT object
ArduinoFFT<double> FFT = ArduinoFFT<double>(vReal, vImag, SAMPLES, SAMPLING_FREQUENCY);

//Peak frequency buffer for Statistics
#define NUM_FREQUENCY_POINTS 60
float frequencyBuffer[NUM_FREQUENCY_POINTS];
int frequencyIndex = 0;
bool bufferFull = false;

#define BUTTON_PIN    0              
#define GREEN_LED     6
#define RED_LED       5 
#define BLUE_LED      7

int RED_LED_STATE = HIGH;                                                                   // the current state of button
int GREEN_LED_STATE = LOW;
int BLUE_LED_STATE   = LOW;
int button_state;                                                                   // the current state of button
int last_button_state; 


WebServer server;
AutoConnect Portal(server);
AutoConnectConfig config;            // this fuction contain lot of features, like controlling menu items, ota, design custom pages etc..,
AutoConnectUpdate update;
AutoConnectAux Sensor_Setting;
AutoConnectAux* auxPage;
//AutoConnectAux aux1("/config_settings", "Sensor Settings");
//AutoConnectAux aux2("/config_save", "Sensor Settings",false) ;    // setting false means it doesnot show in auto connect menu
//AutoConnectAux aux3("/Esp_restart", "Esp Restart");
const char* fw_ver = FIRMWARE_VERSION;





char tempBuffer[3];                                                                                      // Temporary buffer to accept two characters at a time
uint8_t byteValue[255];
uint8_t Buff[255];
bool jsonDataRead = false; // Static flag to track if JSON data has been read

uint8_t checksum;

//const int ledPin = 6;
size_t bufferIndex = 0;
  
DynamicJsonDocument doc(238);
DynamicJsonDocument doc_query(238);


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

bool SendCommand = false;

// I2S configuration
void configureI2S() {
    const i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLING_FREQUENCY,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = 0,
        .dma_buf_count = 8,
        .dma_buf_len = BUFFER_LEN,
        .use_apll = false
    };

    i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
}

// I2S pin setup
void setupI2SPins() {
    const i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_SCK,
        .ws_io_num = I2S_WS,
        .data_out_num = -1,
        .data_in_num = I2S_SD
    };

    i2s_set_pin(I2S_PORT, &pin_config);
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
        "placeholder": "Microphone_INMP441"
      },
      {
        "name": "SensorNumber",
        "type": "ACInput",
        "label": "Sensor Number",
        "placeholder": "Microphone_INMP441_1"
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
        "placeholder": "Range(0~255) Eg: 5"
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
  
  Serial.println("Setup I2S For INMP441 Microphone...");
  delay(1000);
  configureI2S();
  setupI2SPins();
  i2s_start(I2S_PORT);
  delay(500);
  
  Serial1.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN); 
  pinMode(BUTTON_PIN, INPUT);  
   
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED,OUTPUT);                                                                                                          // set ESP32 pin to output mode
  pinMode(BLUE_LED, OUTPUT); 
    
  digitalWrite(RED_LED,LOW);
  digitalWrite(GREEN_LED,LOW);
  digitalWrite(BLUE_LED,LOW);
  digitalWrite(BUTTON_PIN,LOW);
  delay(1000); 
  
  Serial.println();
  xTaskCreatePinnedToCore(ZIGBEE_COMMANDS, "Task1", 20000, NULL, 9, NULL, 0);
  xTaskCreatePinnedToCore(printValues, "Task2", 30000, NULL, 7, NULL, 0);
  xTaskCreatePinnedToCore(Statistics, "Task3", 30000, NULL, 7, NULL, 0);
  xTaskCreatePinnedToCore(WifiMode, "Task4", 10000, NULL, 7, NULL, 0);
   
  
   
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
  float maxPeakFrequency = 0.0;
  float maxPeakAmplitude = 0.0;

  for (;;) {
    size_t bytesIn = 0;
    esp_err_t result = i2s_read(I2S_PORT, sampleBuffer, BUFFER_LEN * sizeof(int16_t), &bytesIn, portMAX_DELAY);

    if (result == ESP_OK) {
      int samplesRead = bytesIn / sizeof(int16_t);

      if (samplesRead > 0) {
        // Copy I2S samples into FFT input arrays
        for (int i = 0; i < SAMPLES; i++) {
          vReal[i] = sampleBuffer[i];
          vImag[i] = 0.0;                                                     // Imaginary part is zero, i think for 44100 sample, first half have positive values , used for FFT and second half values is negative mirror of first half values
        }

        // Perform FFT
        FFT.windowing(FFTWindow::Hamming, FFTDirection::Forward); // Apply windowing
        FFT.compute(FFTDirection::Forward); // Compute FFT
        FFT.complexToMagnitude(); // Compute magnitudes

        // Find the peak frequency for the current frame
        float peakFrequency = 0.0;
        float peakAmplitude = 0.0;

        for (int i = 1; i < (SAMPLES / 2); i++) {
          if (vReal[i] > peakAmplitude) {
            peakAmplitude = vReal[i];
            peakFrequency = (i * ((double)SAMPLING_FREQUENCY / SAMPLES));
          }
        }

        // Update the maximum peak frequency and amplitude if needed
        if (peakAmplitude > maxPeakAmplitude) {
          maxPeakAmplitude = peakAmplitude;
          maxPeakFrequency = peakFrequency;
        }

        /* Serial.print("Frequency: ");
        Serial.print(peakFrequency);
        Serial.print(" Hz, Amplitude: ");
        Serial.println(peakAmplitude);*/
      }
    }

    // Store the maximum peak frequency once per second
    static unsigned long lastMillis = 0;
    if (millis() - lastMillis >= 1000) {
      frequencyBuffer[frequencyIndex] = maxPeakFrequency;
      frequencyIndex++;
      if (frequencyIndex >= NUM_FREQUENCY_POINTS) {
        frequencyIndex = 0;
        bufferFull = true;
      }
      //Serial.print("maxPeakFrequency: ");
      //Serial.println(maxPeakFrequency);
      // Reset the maximum peak for the next second
      maxPeakFrequency = 0.0;
      maxPeakAmplitude = 0.0;

      lastMillis = millis();
    }
    
    vTaskDelay(1); // To avoid watchdog trigger and esure task yields, that means avoid countinuos run this code and give some delay to run other function also
  }
}


void Statistics(void *pvParameters) {
  (void) pvParameters;
  for (;;) {
    if (bufferFull) {
      float sum = 0.0;
      float max = frequencyBuffer[0];
      float min = frequencyBuffer[0];

      for (int i = 0; i < NUM_FREQUENCY_POINTS; i++) {
        sum += frequencyBuffer[i];
        if (frequencyBuffer[i] > max) {
          max = frequencyBuffer[i];
        }
        if (frequencyBuffer[i] < min) {
          min = frequencyBuffer[i];
        }
      }

      float average = sum / NUM_FREQUENCY_POINTS;
      float variance = 0.0;

      for (int i = 0; i < NUM_FREQUENCY_POINTS; i++) {
        variance += pow(frequencyBuffer[i] - average, 2);
      }

      variance /= NUM_FREQUENCY_POINTS;
      float standardDeviation = sqrt(variance);

 
      ZIGBEE_Json_data_write(average, max, min, standardDeviation );
      bufferFull = false; // Reset bufferFull after processing
      
      Serial.println("Statistics:");
      Serial.print("Average: ");
      Serial.println(average);
      Serial.print("Max: ");
      Serial.println(max);
      Serial.print("Min: ");
      Serial.println(min);
      Serial.print("Standard Deviation: ");
      Serial.println(standardDeviation);
      Serial.println();
      vTaskDelay(2000);     // given dealy, to check feed back, if send command is sent succefully or command is broken 55 3 FF FF 0,  if send command is broken, it will resend again
      
      if(SendCommand){
        ZIGBEE_Json_data_write(average, max, min, standardDeviation );
        SendCommand =false;
        Serial.print("Resend Successfull ");
      }
    }else{
    vTaskDelay(1);  // Yield the task if the buffer is not full, To avoid watchdog trigger and esure task yields, that means avoid countinuos run this code and give some delay to run other function also
   }
  }
}

void ZIGBEE_Json_data_write(float Average, float Max, float Min, float StandardDeviation) {
  
            if (!jsonDataRead) {
              Read_EspFlash_JsonFile();
              jsonDataRead = true;
          }
            doc.clear(); 
            doc["Sensor_Type"] = sensor_model;
            doc["Sensor_Number"] = sensor_number;
            doc["Location"] = location;

             // Format the float values with two decimal places
            String AverageStr = String(Average, 2);
            String MaxStr = String(Max, 2);
            String MinStr = String(Min, 2);
            String StandardDeviationStr = String(StandardDeviation, 2);

            
            JsonObject data = doc.createNestedObject("data");
            data["Average"] = AverageStr; 
            data["Max"] = MaxStr;
            data["Min"] = MinStr; 
            data["StandardDeviation"] = StandardDeviationStr;             
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
     else if(Total_length == 44){
           //Serial.println();
           Serial.print("QueryShortAddress: ");
           Serial.println(Query_ShortAddress, HEX);
           ZIGBEE_write(byteCommand, length);      // read query status and send as output to zigbee send command
      
     }else if(Total_length == 5 && real_checksum == 0){  // resend command to rewrite sensor data to zigbee co-ordinator, if you receive command like 55 3 FF FF 0 
           //Serial.println();
           Serial.print("Resend command: ");
           SendCommand = true;
           for(int i=0; i<length; i++){
            Serial.print(byteCommand[i], HEX);
            Serial.print(" ");
           }
      
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
        digitalWrite(GREEN_LED, HIGH);
        break;
      case '0':
        Serial.print("Command: ");
        Serial.println("LED turned OFF");
        Serial.println();
        digitalWrite(GREEN_LED, LOW);
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
  /*  for (int i = 0; i < full_length; i++) {
      Serial.print(hex[i], HEX);
      Serial.print(" ");
    }*/
    
   
    Serial1.write(hex, full_length);
    Serial1.flush(); 
}

uint8_t calculateXORChecksum(const uint8_t* array, int startIndex, int endIndex) {
  checksum = 0;
  
  for (int i = startIndex; i <= endIndex; i++) {
    checksum ^= array[i];
  }

  return checksum;
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
          BLUE_LED_STATE = !BLUE_LED_STATE;
          if ( BLUE_LED_STATE == HIGH){
            Serial.println("Blue Light activate");
            digitalWrite(BLUE_LED, BLUE_LED_STATE);
            digitalWrite(RED_LED, RED_LED_STATE);
            portall = true;
            Autoconnect = true;
           
            
          }else{
            Serial.println("Red Light activate");
            digitalWrite(BLUE_LED, BLUE_LED_STATE);
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
            Portal.load(addonJson);
            
            Portal.on("/sensor_save", onsave);
            Portal.on("/ESP_Restart", onEspRestart);
            //Portal.join(Sensor_Setting);
            
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
