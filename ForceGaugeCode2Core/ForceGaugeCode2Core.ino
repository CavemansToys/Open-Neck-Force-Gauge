#include "WiFi.h"
#include "ElegantOTA.h"
#include "AsyncTCP.h"
#include "ESPAsyncWebServer.h"
#include "TFT_eSPI.h"
#include "HX711_ADC.h"
#include "FS.h"
#include "LittleFS.h"
#include "HTML.h"  // Include the HTML content

/////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// TFT SETUP ////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
// Initialize TFT
TFT_eSPI tft = TFT_eSPI();

// Include the jpeg decoder library
#include "TJpg_Decoder.h"

uint16_t* tft_buffer;
bool      buffer_loaded = false;
uint16_t  spr_width = 0;
uint16_t  bg_color = 0;

// =======================================================================================
// This function will be called during decoding of the jpeg file
// =======================================================================================
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap)
{
  if (y >= tft.height()) return 0;
  tft.pushImage(x, y, w, h, bitmap);
  return 1;
}

//////////////////DIGITAL PINOUT//////////////////
int HX711_dout = 21; //mcu > HX711 dout pin
int HX711_sck = 22; //mcu > HX711 sck pin

/////////////////////////////////////////////////////////////////////////////////
//////////////////////////// LOAD CELL SETUP ////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
HX711_ADC LoadCell(HX711_dout, HX711_sck);
unsigned long t = 0;
unsigned long Max = 0;
volatile float i = 0;  // Changed to volatile and initialized to 0
int cnt = 0;
int Ccnt = 0;
char buff[10];

void TareCalib() 
{
  Serial.println("Starting tare calibration...");
  long tare_offset = 0;
  LoadCell.tare();
  tare_offset = LoadCell.getTareOffset();
  LoadCell.setTareOffset(tare_offset);
  Serial.printf("Tare calibration complete. Offset: %ld\n", tare_offset);
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// WIFI SETUP //////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
const char* ssid_ap     = "ForceGuage";
const char* password_ap = "ForceGuage";

AsyncWebServer server(80);

String readSSID() {
  if (LittleFS.exists("/ssid.txt")) {
    File f = LittleFS.open("/ssid.txt", "r");
    String ssid = f.readStringUntil('\n');
    f.close();
    ssid.trim();
    return ssid;
  }
  return "";
}

String readPassword() {
  if (LittleFS.exists("/password.txt")) {
    File f = LittleFS.open("/password.txt", "r");
    String pass = f.readStringUntil('\n');
    f.close();
    pass.trim();
    return pass;
  }
  return "";
}

void writeSSID(const String& ssid) {
  File f = LittleFS.open("/ssid.txt", "w");
  if (f) {
    f.print(ssid);
    f.close();
  }
}

void writePassword(const String& pass) {
  File f = LittleFS.open("/password.txt", "w");
  if (f) {
    f.print(pass);
    f.close();
  }
}

void setupWiFi() {
  Serial.println("\nStarting WiFi setup...");
  
  // First, try to connect to stored WiFi
  String ssid = readSSID();
  String pass = readPassword();
  
  if (ssid.length() > 0 && pass.length() > 0) {
    Serial.println("Found stored WiFi credentials");
    Serial.print("SSID: ");
    Serial.println(ssid);
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());
    
    Serial.println("Attempting to connect to WiFi...");
    unsigned long startAttemptTime = millis();
    const unsigned long timeout = 10000;

    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < timeout) {
      delay(500);
      Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nConnected to WiFi!");
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
      return;
    } else {
      Serial.println("\nFailed to connect to WiFi");
    }
  } else {
    Serial.println("No stored WiFi credentials found");
  }

  // If we get here, we need to start AP mode
  Serial.println("\nStarting AP mode...");
  
  // Force disconnect and clear any existing WiFi state
  WiFi.disconnect(true);
  delay(2000);  // Increased delay
  
  // Clear any existing WiFi mode
  WiFi.mode(WIFI_OFF);
  delay(2000);  // Increased delay
  
  // Verify WiFi hardware is working
  if (WiFi.getMode() != WIFI_OFF) {
    Serial.println("Error: Unable to turn off WiFi. Hardware may be malfunctioning.");
    return;
  }
  
  // Set to AP mode only
  WiFi.mode(WIFI_AP);
  delay(2000);  // Increased delay
  
  // Verify AP mode was set correctly
  if (WiFi.getMode() != WIFI_AP) {
    Serial.println("Error: Unable to set WiFi to AP mode.");
    return;
  }
  
  Serial.println("Starting AP with credentials:");
  Serial.print("SSID: ");
  Serial.println(ssid_ap);
  Serial.print("Password: ");
  Serial.println(password_ap);
  
  // Start AP with retry mechanism
  bool ap_started = false;
  int retry_count = 0;
  const int max_retries = 3;
  
  while (!ap_started && retry_count < max_retries) {
    ap_started = WiFi.softAP(ssid_ap, password_ap);
    if (!ap_started) {
      Serial.printf("AP start attempt %d failed, retrying...\n", retry_count + 1);
      delay(2000);
      retry_count++;
    }
  }
  
  if (ap_started) {
    Serial.println("AP Started successfully!");
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());
    Serial.print("AP MAC address: ");
    Serial.println(WiFi.softAPmacAddress());
    Serial.print("Number of connected stations: ");
    Serial.println(WiFi.softAPgetStationNum());
    
    // Verify AP is actually running
    if (WiFi.softAPgetStationNum() >= 0) {
      Serial.println("AP is running and ready for connections");
    } else {
      Serial.println("Warning: AP started but may not be fully operational");
    }
  } else {
    Serial.println("Failed to start AP after multiple attempts!");
    Serial.println("Please check the following:");
    Serial.println("1. WiFi hardware is working");
    Serial.println("2. No other AP with same SSID");
    Serial.println("3. Password meets requirements (8-63 chars)");
    Serial.println("4. ESP32 is not in a low power state");
  }
}

/////////////////////////////////////////////////////////////////////////////////
///////////////////////////// FILE OPERATIONS ///////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
void deleteFile(const char *path) {
  Serial.printf("Deleting file: %s\n", path);
  if (LittleFS.remove(path)) {
    Serial.println("File deleted");
  } else {
    Serial.println("Delete failed");
  }
}

void renameFile(const char *path1, const char *path2) {
  Serial.printf("Renaming file %s to %s\n", path1, path2);
  if (LittleFS.rename(path1, path2)) {
    Serial.println("File renamed");
  } else {
    Serial.println("Rename failed");
  }
}

void writeFile(const char *path, const char *message) {
  Serial.printf("Writing file: %s\n", path);
  File file = LittleFS.open(path, "w");
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  if (file.print(message)) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}

void appendFile(const char *path, const char *message) {
  File file = LittleFS.open(path, "a");
  if (!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  if (file.print(message)) {
  } else {
    Serial.println("Append failed");
  }
  file.close();
}

// Task to handle Load Cell and Display updates on Core 0
TaskHandle_t LoadCellTaskHandle = NULL;

// Add these global variables at the top with other globals
bool isLogging = false;
unsigned long lastLogTime = 0;
const unsigned long logInterval = 100; // Log every 100ms

// Add this function with other file operations
void startNewLogFile() {
  // Create a new log file with timestamp
  char filename[32];
  unsigned long timestamp = millis();
  sprintf(filename, "/force_%lu.csv", timestamp);
  
  // Write header
  File file = LittleFS.open(filename, "w");
  if (file) {
    file.println("Timestamp,Force");
    file.close();
    Serial.printf("Created new log file: %s\n", filename);
  } else {
    Serial.println("Failed to create log file");
  }
}

// Modify the LoadCellTask function to include logging
void LoadCellTask(void *parameter) {
  static boolean newDataReady = 0;
  unsigned long t_local = 0;
  const int serialPrintInterval = 5;
  float local_i;
  unsigned long local_Max = 0;
  int local_cnt = 0;
  int local_Ccnt = 0;
  char local_buff[10];

  for (;;) {
    if (LoadCell.update()) newDataReady = true;

    if (newDataReady && millis() > t_local + serialPrintInterval) {
      local_i = LoadCell.getData();
      i = local_i;  // Update the global variable
      Serial.println(local_i);

      // Handle logging if active
      if (isLogging && millis() - lastLogTime >= logInterval) {
        char logEntry[64];
        sprintf(logEntry, "%lu,%.2f\n", millis(), ((round(local_i / 100)) / 10));
        appendFile("/force.csv", logEntry);
        lastLogTime = millis();
      }

      if (local_i < 500) {
        local_i = 0;
        i = 0;  // Update global variable
        local_Max = 0;
        if (local_cnt == 1) {
          local_cnt = 0;
          local_Ccnt++;
          appendFile("/force.csv", dtostrf(local_i, 4, 0, local_buff));
          appendFile("/force.csv", ";\n");
        }
      }

      if (local_i > 1000) {
        local_cnt = 1;
        appendFile("/force.csv", dtostrf(local_i, 4, 0, local_buff));
        appendFile("/force.csv", ";");

        if (local_Max < local_i) {
          local_Max = local_i;
          tft.setTextColor(TFT_WHITE);
          tft.setTextDatum(MC_DATUM);
          tft.fillRect(35, 165, 70, 25, TFT_BLACK);
          tft.setCursor(0, 4, 4);
          tft.drawFloat(((round(local_Max / 100)) / 10), 1, 70, 180);
        }
      }

      tft.setTextColor(TFT_WHITE);
      tft.setTextDatum(MC_DATUM);
      tft.fillRect(60, 30, 110, 50, TFT_BLACK);
      tft.setCursor(0, 4, 6);
      tft.drawFloat(((round(local_i / 100)) / 10), 1, 120, 60);
      t_local = millis();
      newDataReady = 0;

      tft.setTextColor(TFT_WHITE);
      tft.setTextDatum(MC_DATUM);
      tft.setCursor(0, 4, 4);
      tft.fillRect(123, 165, 90, 25, TFT_BLACK);
      tft.drawFloat(local_Ccnt, 1, 170, 180);
    }

    vTaskDelay(5 / portTICK_PERIOD_MS);
  }
}

void setup() {
  // Initialize Serial first and wait for it to be ready
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }
  Serial.println("\n\n=== Force Gauge Starting ===");
  Serial.println("Serial communication initialized");
  
  // Initialize filesystem
  if (!LittleFS.begin()) {
    Serial.println("LittleFS mount failed!");
    return;
  }
  Serial.println("LittleFS mounted successfully");
  
  // Initialize TFT
  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  Serial.println("TFT initialized");

  // Initialize Load Cell
  LoadCell.begin();
  LoadCell.start(2000, true);
  TareCalib();
  Serial.println("Load Cell initialized");

  // Setup WiFi
  Serial.println("\nStarting WiFi setup...");
  setupWiFi();

  // Setup Web Server routes
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("Serving main page");
    request->send_P(200, "text/html", index_html);
  });

  server.on("/force", HTTP_GET, [](AsyncWebServerRequest *request){
    char forceStr[10];
    sprintf(forceStr, "%.1f", ((round(i / 100)) / 10));
    request->send(200, "text/plain", forceStr);
  });

  server.on("/tare", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("Tare request received from web interface");
    TareCalib();
    request->send(200, "text/plain", "Tare complete");
  });

  // Add routes for file operations
  server.on("/download", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("Download request received");
    if (LittleFS.exists("/force.csv")) {
      Serial.println("Sending force.csv file");
      request->send(LittleFS, "/force.csv", "text/csv", true);
    } else {
      Serial.println("force.csv not found");
      request->send(404, "text/plain", "No data file found");
    }
  });

  server.on("/view-data", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("View data request received");
    if (LittleFS.exists("/force.csv")) {
      Serial.println("Sending force.csv file for viewing");
      request->send(LittleFS, "/force.csv", "text/plain", false);
    } else {
      Serial.println("force.csv not found");
      request->send(404, "text/plain", "No data file found");
    }
  });

  // Add backup data routes
  server.on("/view-datab", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("View backup data request received");
    if (LittleFS.exists("/forceb.csv")) {
      Serial.println("Sending forceb.csv file for viewing");
      request->send(LittleFS, "/forceb.csv", "text/plain", false);
    } else {
      Serial.println("forceb.csv not found");
      request->send(404, "text/plain", "No backup data file found");
    }
  });

  server.on("/downloadb", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("Download backup request received");
    if (LittleFS.exists("/forceb.csv")) {
      Serial.println("Sending forceb.csv file");
      request->send(LittleFS, "/forceb.csv", "text/csv", true);
    } else {
      Serial.println("forceb.csv not found");
      request->send(404, "text/plain", "No backup data file found");
    }
  });

  // Add update route
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("Update page request received");
    request->send(200, "text/html", "<html><body><h1>Firmware Update</h1><p>Please use the Arduino IDE or ESPHome to update the firmware.</p></body></html>");
  });

  // Add logging control route
  server.on("/toggle-logging", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("Logging toggle request received");
    isLogging = !isLogging;
    
    if (isLogging) {
      startNewLogFile();
      request->send(200, "text/plain", "started");
      Serial.println("Logging started");
    } else {
      request->send(200, "text/plain", "stopped");
      Serial.println("Logging stopped");
    }
  });

  // Add a catch-all handler for 404s
  server.onNotFound([](AsyncWebServerRequest *request){
    Serial.printf("404 Not Found: %s\n", request->url().c_str());
    request->send(404, "text/plain", "Not found");
  });

  // Start Web Server
  server.begin();
  Serial.println("Web server started");

  // Start Load Cell Task on Core 0
  xTaskCreatePinnedToCore(
    LoadCellTask,
    "LoadCellTask",
    8192,
    NULL,
    1,
    &LoadCellTaskHandle,
    0
  );
  Serial.println("Load Cell task started on Core 0");
  
  Serial.println("=== Setup Complete ===\n");
}

void loop() {
  // Main loop tasks
}
