#ifndef ZIGBEE_Data_H               // if zigbee_data_h is not defined, 
#define ZIGBEE_Data_H               // define zigbee_data_h, define only once

#include "arduino.h"                // arduino.h helps access arduino core function access
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

#include <AutoConnect.h>


extern WiFiClient espClient;
extern PubSubClient client;

class MQTT{
  public :
      MQTT() {} 
      MQTT(const char* mqtt_broker, int mqtt_port); 
      void MQTT_Client();
      void begin();
      void reconnect_client();
      void reconnect_wifi();
       bool reconnect();  // Add this line
     
  private:
       const char* _mqtt_broker;
       int _mqtt_port;
     
};

class ZIGBEE{
  public :
       ZIGBEE(int rx_pin, int tx_pin); 
       void begin();
      
  private:
       int _rx_pin;
       int _tx_pin;
     
};

class ZIGBEE_Data{
  public:
       ZIGBEE_Data() {}
       ZIGBEE_Data(uint8_t Floor_No, uint8_t Room_No, uint8_t Sensor_ID, uint8_t Sensor_No, uint16_t Short_Address);
       void Serial_write_Zigbee();
       void Serial_read_Zigbee();
       void Read_Zigbee_Json_data(const uint8_t* byteCommand, size_t length);
       void Read_Publish_Zigbee_Json_data(const uint8_t* byteCommand, size_t length, uint8_t Floor_No, uint8_t Room_No, uint8_t Sensor_ID, uint8_t Sensor_No, uint16_t ShortAddress);        // not in use but additional feature, if you want to read zigbee json data , this function read receive zigbee data in json format in serial mointer
       uint8_t _Floor_No;
       uint8_t  _Room_No;
       uint8_t _Sensor_ID;
       uint8_t _Sensor_No;
       uint16_t _Short_Address;
       
           
};

class ZIGBEE_Query{
  public :
      void Query();
      void SelfQuery(const uint8_t* byteCommand, size_t length); 
      void Sensor_Node_Query(const uint8_t* byteCommand, size_t length);
      void Node_Query(const uint8_t* byteCommand, size_t length);
      bool flag_zigbee_query = false;
           
  private:
     uint8_t device_type;
     uint8_t mac_id[8];
     uint8_t channel;
     uint8_t pan_id[2];
     uint8_t short_address[2];
     uint8_t expand_pan_id[8];
     uint8_t net_key[16];
             
};


class Autoconnect{
  public :
      void Autoconnect_loop();
      
      
             
};
//extern AutoConnectConfig config;
//extern AutoConnectAux* auxPage;


#endif