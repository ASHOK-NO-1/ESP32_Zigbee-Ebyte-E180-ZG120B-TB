#ifndef ESP_FILE_SERVER_H
#define ESP_FILE_SERVER_H

#include <Arduino.h>
#include "SPIFFS.h"
#include <SPI.h>
#include "FS.h"
#include <ArduinoJson.h>
#include <WebServer.h>

extern WebServer server;

class ESPFileServer {
  public:
    ESPFileServer();
    void begin();
    void handleClient();
    
  //private:
    void SendHTML_Header();
    void SendHTML_Content();
    void SendHTML_Stop();
    void HomePage();
    void File_Download();
    void DownloadFile(String filename);
    void File_Upload();
    void handleFileUpload();
    void File_Stream();
    void SPIFFS_file_stream(String filename);
    void File_Delete();
    void SPIFFS_file_delete(String filename);
    void SPIFFS_dir();
    void printDirectory(const char * dirname, uint8_t levels);
    void SelectInput(String heading1, String command, String arg_calling_name);
    void ReportSPIFFSNotPresent();
    void ReportFileNotPresent(String target);
    void ReportCouldNotCreateFile(String target);
    String file_size(int bytes);
    void append_page_header();
    void append_page_footer();
    

    
};

class JsonFileManager {
public:
    JsonFileManager(const char* filename);

    bool saveJsonToFile(const DynamicJsonDocument& doc);
    bool loadJsonFromFile( DynamicJsonDocument& doc);
    bool overwriteJsonToFile(const DynamicJsonDocument& doc);

private:
    const char* filename;
};


#endif
