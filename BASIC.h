// https://doc-tft-espi.readthedocs.io/tft_espi/methods/drawrect/
// https://randomnerdtutorials.com/esp32-pinout-reference-gpios/
#include "WiFi.h"
#include "ElegantOTA.h"
#include "AsyncTCP.h"
#include "ESPAsyncWebServer.h"
#include "TFT_eSPI.h"
TFT_eSPI tft = TFT_eSPI();
#include "HX711_ADC.h"
#include "SD.h"
#include "SPI.h"
#include "FS.h"
#include "LittleFS.h" // or #include "SPIFFS.h" depending on your setup

// WiFi credentials for station mode
const char* ssid_sta = "aaaaaaaaaa";
const char* password_sta = "aaaaaaaaaaaa";

// Fallback AP credentials
const char* ssid_ap     = "ForceGauge";
const char* password_ap = "1234";
IPAddress IP(30, 8, 6, 5); // Set your desired static IP address
IPAddress gateway(30, 8, 6, 5); // Set your gateway IP address
IPAddress subnet(255, 255, 255, 0); // Set your subnet mask
//const char* ssid = "Starnet";
//const char* password = "LeTMeiNPLeaSe";
//IPAddress gateway(192,168,8,1); // Set your gateway IP address
//IPAddress subnet(255, 255, 255, 0); // Set your subnet mask
AsyncWebServer server(80);

bool wifiConnected = false;

void setupWiFi() {
  String ssid = readSSID();
  String pass = readPassword();

  if (ssid.length() > 0 && pass.length() > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());
    Serial.println("Connecting to WiFi in station mode...");
    unsigned long startAttemptTime = millis();
    const unsigned long timeout = 10000; // 10 seconds timeout

    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < timeout) {
      delay(500);
      Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nConnected to WiFi!");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      wifiConnected = true;
      return;
    }
  }

  // fallback AP if not connected
  Serial.println("Failed to connect, starting fallback AP...");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid_ap, password_ap);
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
}

// Jpeg image array attached to this sketch
#include "Logo.h"
#include "Black.h"

// Include the jpeg decoder library
#include "TJpg_Decoder.h"

uint16_t* tft_buffer;
bool      buffer_loaded = false;
uint16_t  spr_width = 0;
uint16_t  bg_color =0;


// =======================================================================================
// This function will be called during decoding of the jpeg file
// =======================================================================================
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap)
{
  // Stop further decoding as image is running off bottom of screen
  if ( y >= tft.height() ) return 0;

  // This function will clip the image block rendering automatically at the TFT boundaries
  tft.pushImage(x, y, w, h, bitmap);

  // Return 1 to decode next block
  return 1;
}

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
//HX711 constructor:
HX711_ADC LoadCell(HX711_dout, HX711_sck);
unsigned long t = 0;
unsigned long Max = 0;
float i = 1000;
int cnt = 0;
int Ccnt = 0;
char buff[10];


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


void TareCalib() 
{
  long tare_offset = 0;
  LoadCell.tare(); // calculate the new tare / zero offset value (blocking)
  tare_offset = LoadCell.getTareOffset(); // get the new tare / zero offset value
  LoadCell.setTareOffset(tare_offset); // set value as library parameter (next restart it will be read from EEprom)
}


// =======================================================================================
// Setup
// =======================================================================================
void setup()   
{
  Serial.println();
  Serial.println("Starting...");
  
  Serial.begin(115200);
  while (!Serial) 
  {
    delay(10);
  }
  if (!LittleFS.begin()) {
    Serial.println("Failed to mount LittleFS");
  }
/////////////////////////////////////////////
///////////////// SD SETUP //////////////////
/////////////////////////////////////////////
  SPI.begin(SD_SCLK, SD_DO, SD_DI, SD_CS);
  if (!SD.begin(SD_CS)) 
  {
    Serial.println("Card Mount Failed");
  }
  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE) 
  {
    Serial.println("No SD card attached");
    return;
  }

  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC) 
  {
    Serial.println("MMC");
  } 
  else if (cardType == CARD_SD) 
  {
    Serial.println("SDSC");
  } 
  else if (cardType == CARD_SDHC) 
  {
    Serial.println("SDHC");
  } 
  else 
  {
    Serial.println("UNKNOWN");
  }
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);
  deleteFile(SD, "/forcebck.csv");
  renameFile(SD, "/force.csv", "/forcebck.csv"); 
  writeFile(SD, "/force.csv", "start\n");



/////////////////////////////////////////////
//////////////// LOAD CELL //////////////////
/////////////////////////////////////////////

  LoadCell.begin();
  float calibrationValue = 80.13; // uncomment this if you want to set the calibration value in the sketch
  LoadCell.setTareOffset(calibrationValue);
  boolean ReTare = false; //set this to false as the value has been resored from eeprom

  unsigned long stabilizingtime = 2000; // preciscion right after power-up can be improved by adding a few seconds of stabilizing time
  LoadCell.start(stabilizingtime, ReTare);
  if (LoadCell.getTareTimeoutFlag()) 
  {
    Serial.println("Timeout, check MCU>HX711 wiring and pin designations");
    while (1);
  }
  else 
  {
    LoadCell.setCalFactor(calibrationValue); // set calibration value (float)
    TareCalib();    
    Serial.println("Startup is complete");
  }

/////////////////////////////////////////////
//////////////// WEB SERVER /////////////////
/////////////////////////////////////////////
  ElegantOTA.begin(&server);
// Connect to Wi-Fi network with SSID and password
  WiFi.softAP(ssid_ap);
  WiFi.softAPConfig(IP, gateway, subnet); 
//  Serial.println(WiFi.softAPIP());
  Serial.print("AP IP address: ");
  Serial.println(IP);

    // Handle the root URL
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SD, "/index.html", "text/html");
  });

  // Handle the download button
  server.on("/download", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SD, "/force.csv", String(), true);
  });

  // Handle the View Data button
  server.on("/view-data", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SD, "/force.csv", "text/plain", false);
  });

  // Handle the download backup button
  server.on("/downloadb", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SD, "/forcebck.csv", String(), true);
  });
  
  // Handle the View Data button
  server.on("/view-datab", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SD, "/forcebck.csv", "text/plain", false);
  });

  /* SETUP YOR WEB OWN ENTRY POINTS */
  server.on("/Updatefirm", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(302, "Location", "/update");
  });

  server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request){
    String html = "<form method='POST' action='/save-credentials'>";
    html += "SSID: <input type='text' name='ssid' value='" + readSSID() + "'><br>";
    html += "Password: <input type='text' name='pass' value='" + readPassword() + "'><br>";
    html += "<input type='submit' value='Save'>";
    html += "</form>";
    request->send(200, "text/html", html);
  }); 

  server.on("/save-credentials", HTTP_POST, [](AsyncWebServerRequest *request){
    String ssid = request->getParam("ssid")->value();
    String pass = request->getParam("pass")->value();
    writeSSID(ssid);
    writePassword(pass);
    request->send(200, "text/html", "Credentials saved! Restart device to connect.");
  // Optionally, restart device:
  // ESP.restart();
});

  /* INITIALIZE ElegantOTA LIBRARY */
ElegantOTA.begin(&server);
      
  server.begin(); 

/////////////////////////////////////////////
//////////////// TFT SETUP //////////////////
/////////////////////////////////////////////
  // The byte order can be swapped (set true for TFT_eSPI)
  TJpgDec.setSwapBytes(true);

  // The jpeg decoder must be given the exact name of the rendering function above
  TJpgDec.setCallback(tft_output);
  
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);

  // Draw the Logo
  TJpgDec.drawJpg(0, 0, logo,sizeof(logo));
  delay(1000);
  tft.fillScreen(TFT_BLACK);

// Draw a circle with a green border
  tft.drawCircle(120, 120, 120, TFT_GREEN);
  tft.drawCircle(120, 120, 119, TFT_GREEN);
  tft.drawCircle(120, 120, 118, TFT_GREEN);
  tft.drawCircle(120, 120, 117, TFT_GREEN);
  tft.drawCircle(120, 120, 116, TFT_GREEN);
  tft.drawLine(5, 120, 235, 120, TFT_BLUE);
  tft.drawLine(6, 119, 234, 119, TFT_BLUE);
  tft.drawLine(6, 121, 234, 121, TFT_BLUE);  
  tft.drawLine(120, 120, 120, 235, TFT_BLUE);
  tft.drawLine(119, 120, 119, 234, TFT_BLUE);
  tft.drawLine(121, 120, 121, 234, TFT_BLUE);

// Plot the text labels 
  tft.setTextColor(TFT_WHITE, bg_color);
  tft.setTextDatum(MC_DATUM);
  tft.setCursor(0, 4, 4);
  tft.drawString("Kilograms", 120, 105);
  tft.setCursor(0, 4, 2);
  tft.drawString("Last Max", 70, 135);
  tft.drawString("Count", 180, 135);
  
////////////////////////////////////////////////
///////////////// GET DATA /////////////////////
////////////////////////////////////////////////
//  i = LoadCell.getData();
  tft.setTextColor(TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.setCursor(0, 4, 6);
  tft.drawFloat(0, 1, 120, 60); //((round(i/100))/10)
  tft.setTextColor(TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.setCursor(0, 4, 4); 
  tft.drawFloat(Max,1, 70, 180); 
  tft.setTextColor(TFT_WHITE);
  tft.setTextDatum(MC_DATUM);  
  tft.setCursor(0, 4, 4);
  tft.drawString("Ccnt", 1, 170, 180);  
  delay(20);
  
}//VOID Setup END

// =======================================================================================
// Loop
// =======================================================================================
void loop() 
{
  ElegantOTA.loop();
  static boolean newDataReady = 0;
  const int serialPrintInterval = 5; //increase value to slow down serial print activity

  // check for new data/start next conversion:
  if (LoadCell.update()) newDataReady = true;

  // get smoothed value from the dataset:
  if (newDataReady) 
  {
    if (millis() > t + serialPrintInterval) 
    {
      i = LoadCell.getData();
      Serial.println(i); ///////////////////////////////////////////////////
      if (i < 500)
      {
        i = 0;
        Max = 0;
        if (cnt == 1)
        {
          cnt = 0;
          Ccnt++;
          appendFile(SD, "/force.csv", dtostrf(i, 4, 0, buff));
          appendFile(SD, "/force.csv", ";\n");         
        }

      }
      if (i>1000)
      {
        cnt = 1;
        Serial.print(((round(i/100))/10));
        Serial.print(";");        
        appendFile(SD, "/force.csv", dtostrf(i, 4, 0, buff));
        appendFile(SD, "/force.csv", ";");        
        if (Max<i)
        {
          Max = i;
          tft.setTextColor(TFT_WHITE);
          tft.setTextDatum(MC_DATUM);
          tft.fillRect(35, 165, 70, 25, TFT_BLACK); ///BLANK Max
          tft.setCursor(0, 4, 4); 
          tft.drawFloat(((round(Max/100))/10),1, 70, 180); 
        }
      }               
      tft.setTextColor(TFT_WHITE);
      tft.setTextDatum(MC_DATUM);
      tft.fillRect(60, 30, 110, 50, TFT_BLACK); ///BLANK MAIN KG COUNTER
      tft.setCursor(0, 4, 6);
      tft.drawFloat(((round(i/100))/10),1, 120, 60);
      newDataReady = 0;  
      t = millis();
    }  
  tft.setTextColor(TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.setCursor(0, 4, 4);
  tft.fillRect(123, 165, 90, 25, TFT_BLACK); ///BLANK Ccnt TIME 
  tft.drawFloat(Ccnt, 1, 170, 180);
  }
}


// =======================================================================================
