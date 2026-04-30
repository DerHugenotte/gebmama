#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <ArduinoJson.h>

// --- DEINE DATEN HIER EINTRAGEN ---



// NEU: Liste von Standorten für das Wetter
const int NUM_STAEDTE = 2;
String staedte[NUM_STAEDTE] = {"Schlat,DE", "Faurndau,DE"};
int aktuelleStadtIndex = 0; // Startet bei der ersten Stadt in der Liste
// ----------------------------------

TFT_eSPI tft = TFT_eSPI();

// --- AUFGABEN-KONFIGURATION ---
struct Task {
  String title;
  String message;
  uint16_t color;
};

const int NUM_TASKS = 4; 

Task tasks[NUM_TASKS] = {
  {"Kochen", "Wer macht heute das vegane Abendessen? 🥦", TFT_DARKGREEN},
  {"Muell", "Bitte den Müll rausbringen! 🗑️", TFT_MAROON},
  {"Waesche", "Die Waschmaschine ist fertig! 👕", TFT_NAVY},
  {"Saugen", "Einmal durchsaugen bitte! 🧹", TFT_PURPLE}
};

// --- VARIABLEN FÜR DIE LOGIK ---
enum AppState { STATE_HOME, STATE_HAUSHALT_MENU, STATE_HAUSHALT_CONFIRM, STATE_WETTER };
AppState currentState = STATE_HOME;

int currentPage = 0;
const int TASKS_PER_PAGE = 2;
int totalPages = (NUM_TASKS + TASKS_PER_PAGE - 1) / TASKS_PER_PAGE;
int pendingTaskIndex = -1;

unsigned long lastTouchTime = 0;

// Variablen zum Speichern der Wetterdaten
float w_temp = 0;
float w_minTemp = 0;
float w_maxTemp = 0;
int w_humidity = 0;
float w_wind = 0;
String w_description = "";
bool weatherNeedsUpdate = true; 

// --- LAYOUT KOORDINATEN ---
#define BTN_W 200
#define BTN_H 70
#define BTN_X 20
#define BTN1_Y 50
#define BTN2_Y 140

#define NAV_Y 240
#define NAV_W 80
#define NAV_H 50
#define NAV_PREV_X 20
#define NAV_NEXT_X 140

#define CONFIRM_W 90
#define CONFIRM_H 60
#define CONFIRM_Y 160
#define CONFIRM_YES_X 20
#define CONFIRM_NO_X 130


void setup() {
  Serial.begin(115200);

  tft.init();
  tft.setRotation(0); 
  tft.fillScreen(TFT_BLACK);

  uint16_t calData[5] = { 275, 3620, 264, 3532, 1 };
  tft.setTouch(calData);

  pinMode(27, OUTPUT);
  digitalWrite(27, HIGH);

  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.print("Verbinde WLAN...");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  drawHomeScreen();
}

void loop() {
  uint16_t x, y;

  if (tft.getTouch(&x, &y)) {
    x = 240 - x; 
    
    if (millis() - lastTouchTime > 100) {

      // ==========================================
      // ZUSTAND 0: HOME SCREEN
      // ==========================================
      if (currentState == STATE_HOME) {
        if (isHit(x, y, BTN_X, BTN1_Y, BTN_W, BTN_H)) {
          currentState = STATE_HAUSHALT_MENU;
          currentPage = 0;
          drawHaushaltMenu();
        }
        else if (isHit(x, y, BTN_X, BTN2_Y, BTN_W, BTN_H)) {
          currentState = STATE_WETTER;
          weatherNeedsUpdate = true; 
          drawWetterScreen(); 
        }
      }

      // ==========================================
      // ZUSTAND 1: HAUSHALTS-APP
      // ==========================================
      else if (currentState == STATE_HAUSHALT_MENU) {
        int task1Index = currentPage * TASKS_PER_PAGE;
        int task2Index = task1Index + 1;

        if (task1Index < NUM_TASKS && isHit(x, y, BTN_X, BTN1_Y, BTN_W, BTN_H)) {
          pendingTaskIndex = task1Index;
          currentState = STATE_HAUSHALT_CONFIRM;
          drawConfirm();
        }
        else if (task2Index < NUM_TASKS && isHit(x, y, BTN_X, BTN2_Y, BTN_W, BTN_H)) {
          pendingTaskIndex = task2Index;
          currentState = STATE_HAUSHALT_CONFIRM;
          drawConfirm();
        }
        else if (isHit(x, y, NAV_PREV_X, NAV_Y, NAV_W, NAV_H)) {
          if (currentPage == 0) {
            currentState = STATE_HOME;
            drawHomeScreen();
          } else {
            currentPage--;
            drawHaushaltMenu();
          }
        }
        else if (currentPage < totalPages - 1 && isHit(x, y, NAV_NEXT_X, NAV_Y, NAV_W, NAV_H)) {
          currentPage++;
          drawHaushaltMenu();
        }
      }
      
      // ==========================================
      // ZUSTAND 2: HAUSHALTS-APP BESTÄTIGUNG
      // ==========================================
      else if (currentState == STATE_HAUSHALT_CONFIRM) {
        if (isHit(x, y, CONFIRM_YES_X, CONFIRM_Y, CONFIRM_W, CONFIRM_H)) {
          drawStatus("Sende...");
          sendPushoverMessage(tasks[pendingTaskIndex].message);
          drawStatus("Gesendet!");
          delay(1500); 
          currentState = STATE_HAUSHALT_MENU;
          drawHaushaltMenu();
        }
        else if (isHit(x, y, CONFIRM_NO_X, CONFIRM_Y, CONFIRM_W, CONFIRM_H)) {
          currentState = STATE_HAUSHALT_MENU;
          drawHaushaltMenu();
        }
      }

      // ==========================================
      // ZUSTAND 3: WETTER-APP
      // ==========================================
      else if (currentState == STATE_WETTER) {
        // HOME Button gedrückt (Links)
        if (isHit(x, y, NAV_PREV_X, NAV_Y, NAV_W, NAV_H)) {
          currentState = STATE_HOME;
          drawHomeScreen();
        }
        // NEU: SWAP Button gedrückt (Rechts)
        else if (isHit(x, y, NAV_NEXT_X, NAV_Y, NAV_W, NAV_H)) {
          aktuelleStadtIndex++; // Zum nächsten Ort wechseln
          
          // Wenn wir am Ende der Liste sind, fangen wir wieder von vorne an
          if (aktuelleStadtIndex >= NUM_STAEDTE) {
            aktuelleStadtIndex = 0;
          }
          
          weatherNeedsUpdate = true; // Daten für neuen Ort laden
          drawWetterScreen(); // Direkt UI neu zeichnen ("Lade Daten..." anzeigen)
        }
      }

      while(tft.getTouch(&x, &y)) { 
        delay(20); 
      }
      lastTouchTime = millis();
    }
  }

  // --- WETTER LADEN LOGIK ---
  if (currentState == STATE_WETTER && weatherNeedsUpdate) {
    fetchWeatherData();
    weatherNeedsUpdate = false;
    drawWetterScreen(); 
  }
}

bool isHit(uint16_t tx, uint16_t ty, uint16_t bx, uint16_t by, uint16_t bw, uint16_t bh) {
  return (tx > bx && tx < (bx + bw) && ty > by && ty < (by + bh));
}

// ==========================================
// GRAFIK-FUNKTIONEN
// ==========================================

void drawHomeScreen() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(50, 15);
  tft.print("MAIN MENU");

  tft.fillRoundRect(BTN_X, BTN1_Y, BTN_W, BTN_H, 8, TFT_BLUE);
  tft.setCursor(BTN_X + 45, BTN1_Y + 25);
  tft.print("Haushalt");

  tft.fillRoundRect(BTN_X, BTN2_Y, BTN_W, BTN_H, 8, TFT_ORANGE);
  tft.setCursor(BTN_X + 55, BTN2_Y + 25);
  tft.print("Wetter");
}

void drawHaushaltMenu() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(20, 15);
  tft.print("Aufgabe waehlen:");

  int task1Index = currentPage * TASKS_PER_PAGE;
  int task2Index = task1Index + 1;

  if (task1Index < NUM_TASKS) {
    tft.fillRoundRect(BTN_X, BTN1_Y, BTN_W, BTN_H, 8, tasks[task1Index].color);
    tft.setCursor(BTN_X + 20, BTN1_Y + 25);
    tft.print(tasks[task1Index].title);
  }
  if (task2Index < NUM_TASKS) {
    tft.fillRoundRect(BTN_X, BTN2_Y, BTN_W, BTN_H, 8, tasks[task2Index].color);
    tft.setCursor(BTN_X + 20, BTN2_Y + 25);
    tft.print(tasks[task2Index].title);
  }

  if (currentPage == 0) {
    tft.fillRoundRect(NAV_PREV_X, NAV_Y, NAV_W, NAV_H, 5, TFT_BLUE);
    tft.setCursor(NAV_PREV_X + 15, NAV_Y + 15);
    tft.print("HOME");
  } else {
    tft.fillRoundRect(NAV_PREV_X, NAV_Y, NAV_W, NAV_H, 5, TFT_DARKGREY);
    tft.setCursor(NAV_PREV_X + 25, NAV_Y + 15);
    tft.print("<-");
  }

  if (currentPage < totalPages - 1) {
    tft.fillRoundRect(NAV_NEXT_X, NAV_Y, NAV_W, NAV_H, 5, TFT_DARKGREY);
    tft.setCursor(NAV_NEXT_X + 25, NAV_Y + 15);
    tft.print("->");
  }
}

void drawConfirm() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(20, 30);
  tft.print("Wirklich senden?");
  
  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(20, 70);
  tft.print("> " + tasks[pendingTaskIndex].title);

  tft.fillRoundRect(CONFIRM_YES_X, CONFIRM_Y, CONFIRM_W, CONFIRM_H, 8, TFT_DARKGREEN);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(CONFIRM_YES_X + 30, CONFIRM_Y + 20);
  tft.print("JA");

  tft.fillRoundRect(CONFIRM_NO_X, CONFIRM_Y, CONFIRM_W, CONFIRM_H, 8, TFT_MAROON);
  tft.setCursor(CONFIRM_NO_X + 20, CONFIRM_Y + 20);
  tft.print("NEIN");
}

void drawWetterScreen() {
  tft.fillScreen(TFT_BLACK);

  // Kopfzeile
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(20, 15);
  // NEU: Holt den Namen der aktuellen Stadt aus der Liste
  String header = "Wetter: " + staedte[aktuelleStadtIndex];
  header.replace(",DE", ""); 
  tft.print(header);
  
  tft.drawLine(20, 35, 220, 35, TFT_DARKGREY);

  if (weatherNeedsUpdate) {
    tft.setTextColor(TFT_YELLOW);
    tft.setCursor(40, 100);
    tft.print("Lade Daten...");
  } else {
    tft.setTextColor(TFT_ORANGE);
    tft.setTextSize(5);
    tft.setCursor(30, 50);
    tft.print(w_temp, 1); 
    tft.print("C");

    tft.setTextColor(TFT_CYAN);
    tft.setTextSize(2);
    tft.setCursor(20, 100);
    tft.print(w_description);

    tft.setTextColor(TFT_LIGHTGREY);
    tft.setTextSize(2);
    tft.setCursor(20, 140);
    tft.print("Tag: " + String(w_minTemp, 0) + "C bis " + String(w_maxTemp, 0) + "C");

    tft.setCursor(20, 170);
    tft.print("Feuchte: " + String(w_humidity) + "%");

    tft.setCursor(20, 200);
    tft.print("Wind: " + String(w_wind, 1) + " m/s");
  }

  // HOME Button (Links)
  tft.fillRoundRect(NAV_PREV_X, NAV_Y, NAV_W, NAV_H, 5, TFT_BLUE);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(NAV_PREV_X + 15, NAV_Y + 15);
  tft.print("HOME");

  // NEU: SWAP Button (Rechts)
  tft.fillRoundRect(NAV_NEXT_X, NAV_Y, NAV_W, NAV_H, 5, TFT_PURPLE);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(NAV_NEXT_X + 15, NAV_Y + 15);
  tft.print("SWAP");
}

void drawStatus(String text) {
  tft.fillRect(0, 260, 240, 60, TFT_BLACK); 
  tft.setTextColor(TFT_CYAN);
  tft.setTextSize(2);
  tft.setCursor(60, 280);
  tft.print(text);
}

// ==========================================
// NETZWERK-FUNKTIONEN
// ==========================================

void sendPushoverMessage(String message) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin("https://api.pushover.net/1/messages.json");
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    String postData = "token=" + String(pushoverApiToken) + 
                      "&user=" + String(pushoverUserKey) + 
                      "&message=" + message;

    int httpResponseCode = http.POST(postData);
    http.end();
  }
}

void fetchWeatherData() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    // NEU: Die URL nutzt jetzt dynamisch den Ort aus unserer Liste
    String url = "http://api.openweathermap.org/data/2.5/weather?q=" + staedte[aktuelleStadtIndex] + "&appid=" + String(owmApiKey) + "&units=metric&lang=de";
    
    http.begin(url);
    int httpCode = http.GET();
    
    if (httpCode > 0) {
      String payload = http.getString();
      
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, payload);

      if (!error) {
        w_temp = doc["main"]["temp"];
        w_minTemp = doc["main"]["temp_min"];
        w_maxTemp = doc["main"]["temp_max"];
        w_humidity = doc["main"]["humidity"];
        w_wind = doc["wind"]["speed"];
        
        const char* desc = doc["weather"][0]["description"];
        w_description = String(desc);
        
        if(w_description.length() > 0) {
           w_description[0] = toupper(w_description[0]);
        }
      } else {
        Serial.print("JSON Fehler: ");
        Serial.println(error.c_str());
        w_description = "Fehler beim Laden";
      }
    } else {
      w_description = "Kein Internet";
    }
    http.end();
  }
}