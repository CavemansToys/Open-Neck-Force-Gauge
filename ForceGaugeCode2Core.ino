#include "WiFi.h"
#include "ElegantOTA.h"
#include "AsyncTCP.h"
#include "ESPAsyncWebServer.h"
#include "TFT_eSPI.h"
#include "HX711_ADC.h"
#include "FS.h"
#include "LittleFS.h" // Added
#include "SD.h"
#include "SPI.h"

/////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// TFT SETUP ////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
// Initialize TFT
TFT_eSPI tft = TFT_eSPI();

// Jpeg image array attached to this sketch
#include "Logo.h"
#include "Black.h"

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
  // Stop further decoding as image is running off bottom of screen
  if (y >= tft.height()) return 0;

  // This function will clip the image block rendering automatically at the TFT boundaries
  tft.pushImage(x, y, w, h, bitmap);

  // Return 1 to decode next block
  return 1;
}

////////////////////////////////////// END ////////////////////////////////////////
/////////////////////////////////// TFT SETUP /////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////

//////////////////DIGITAL PINOUT//////////////////
// 4 
// 5  SD_CS (SS)
// 13 
// 14 SD_CLK  
// 16 
// 17 
// 18 
// 19 SD_DO (MISO)
// 21 Load Cell DOUT
// 22 Load Cell SCLK
// 23 SD_DI (MOSI)
// 25 TFT_RST
// 26 TFT_SDA
// 27 TFT_SCLK
// 32 TFT_DC
// 33 TFT_CS
// 34 
// 36

int SD_CS = 5;
int TFT_CLK = 14;
int SD_SCLK = 18;
int SD_DO = 19;
int SD_DI = 23;
int HX711_dout = 21; //mcu > HX711 dout pin
int HX711_sck = 22; //mcu > HX711 sck pin


/////////////////////////////////////////////////////////////////////////////////
//////////////////////////// LOAD CELL SETUP ////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
//HX711 constructor:
HX711_ADC LoadCell(HX711_dout, HX711_sck);
unsigned long t = 0;
unsigned long Max = 0;
float i = 1000;
int cnt = 0;
int Ccnt = 0;
char buff[10];

////////////////////////////////// END //////////////////////////////////////////
//////////////////////////// LOAD CELL SETUP ////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// WIFI SETUP //////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

// WiFi credentials for fallback AP
const char* ssid_ap     = "ForceGauge";
const char* password_ap = "1234";
IPAddress IP(30, 8, 6, 5);
IPAddress gateway(30, 8, 6, 5);
IPAddress subnet(255, 255, 255, 0);

AsyncWebServer server(80);

// Functions to read/write WIFI credentials
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

// Setup WiFi with stored credentials
void setupWiFi() {
  String ssid = readSSID();
  String pass = readPassword();

  if (ssid.length() > 0 && pass.length() > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());
    Serial.println("Connecting to WiFi in station mode...");
    unsigned long startAttemptTime = millis();
    const unsigned long timeout = 10000; // 10 sec

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
      Serial.println("\nFailed to connect, starting fallback AP...");
    }
  }

  // fallback AP setup
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid_ap, password_ap);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());
}

/////////////////////////////////// END /////////////////////////////////////////
/////////////////////////////// WIFI SETUP //////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////
///////////////////////////// SD CARD SETUP /////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

void deleteFile(fs::FS &fs, const char *path) {
  Serial.printf("Deleting file: %s\n", path);
  if (fs.remove(path)) {
    Serial.println("File deleted");
  } else {
    Serial.println("Delete failed");
  }
}

void renameFile(fs::FS &fs, const char *path1, const char *path2) {
  Serial.printf("Renaming file %s to %s\n", path1, path2);
  if (fs.rename(path1, path2)) {
    Serial.println("File renamed");
  } else {
    Serial.println("Rename failed");
  }
}

void writeFile(fs::FS &fs, const char *path, const char *message) {
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
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

void appendFile(fs::FS &fs, const char *path, const char *message) {
//  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  if (file.print(message)) {
//    Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
}

////////////////////////////////// END //////////////////////////////////////////
///////////////////////////// SD CARD SETUP /////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////// FORCE GAUGE SETUP ///////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

void TareCalib() 
{
  long tare_offset = 0;
  LoadCell.tare(); // calculate the new tare / zero offset value (blocking)
  tare_offset = LoadCell.getTareOffset(); // get the new tare / zero offset value
  LoadCell.setTareOffset(tare_offset); // set value as library parameter (next restart it will be read from EEprom)
}

////////////////////////////////// END //////////////////////////////////////////
/////////////////////////// FORCE GAUGE SETUP ///////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

// Task to handle Load Cell and Display updates on Core 0
TaskHandle_t LoadCellTaskHandle = NULL;

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
      Serial.println(local_i);

      if (local_i < 500) {
        local_i = 0;
        local_Max = 0;
        if (local_cnt == 1) {
          local_cnt = 0;
          local_Ccnt++;
          appendFile(SD, "/force.csv", dtostrf(local_i, 4, 0, local_buff));
          appendFile(SD, "/force.csv", ";\n");
        }
      }

      if (local_i > 1000) {
        local_cnt = 1;
        appendFile(SD, "/force.csv", dtostrf(local_i, 4, 0, local_buff));
        appendFile(SD, "/force.csv", ";");

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

    vTaskDelay(5 / portTICK_PERIOD_MS);  // Yield task
  }
}

void setup() {
  Serial.begin(115200);
  LittleFS.begin();
  
  // Initialize TFT
  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextDatum(MC_DATUM);

  // Initialize SD card
  if (!SD.begin(SD_CS)) {
    Serial.println("SD Card mount failed.");
    return;
  }
  Serial.println("SD Card mounted.");

  // Initialize Load Cell
  LoadCell.begin();
  LoadCell.start(2000, true);
  TareCalib();

  // Setup WiFi
  setupWiFi();

  // Start OTA and Web Server
  ElegantOTA.begin(&server);
  server.begin();

  // Start Load Cell Task on Core 0
  xTaskCreatePinnedToCore(
    LoadCellTask,          // Function to implement the task
    "LoadCellTask",        // Name of the task
    8192,                  // Stack size
    NULL,                  // Task input parameter
    1,                     // Priority
    &LoadCellTaskHandle,   // Task handle
    0                      // Core 0
  );
}

void loop() {
  ElegantOTA.loop();  // Handle OTA updates on Core 1
}
