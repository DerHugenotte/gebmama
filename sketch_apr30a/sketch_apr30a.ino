#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <TFT_eSPI.h>

// --- DEINE DATEN HIER EINTRAGEN ---

// ----------------------------------

TFT_eSPI tft = TFT_eSPI();

// --- AUFGABEN-KONFIGURATION ---
struct Task {
  String title;
  String message;
  uint16_t color;
};

// Wie viele Aufgaben gibt es insgesamt?
const int NUM_TASKS = 4; 

// Unsere Aufgaben-Liste
Task tasks[NUM_TASKS] = {
  {"Kochen", "Wer macht heute das vegane Abendessen? 🥦", TFT_DARKGREEN},
  {"Muell", "Bitte den Müll rausbringen! 🗑️", TFT_MAROON},
  {"Waesche", "Die Waschmaschine ist fertig! 👕", TFT_NAVY},
  {"Saugen", "Einmal durchsaugen bitte! 🧹", TFT_PURPLE}
};

// --- VARIABLEN FÜR DIE LOGIK ---
enum AppState { STATE_MENU, STATE_CONFIRM };
AppState currentState = STATE_MENU;

int currentPage = 0;
const int TASKS_PER_PAGE = 2;
int totalPages = (NUM_TASKS + TASKS_PER_PAGE - 1) / TASKS_PER_PAGE;
int pendingTaskIndex = -1;

unsigned long lastTouchTime = 0;

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
  tft.setRotation(0); // Hochformat
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

  drawMenu();
}

void loop() {
  uint16_t x, y;

  if (tft.getTouch(&x, &y)) {
    
    // --- BUGFIX 1: X-Achse spiegeln ---
    // Korrigiert das Problem, dass Rechts und Links vertauscht waren
    x = 240 - x; 
    
    if (millis() - lastTouchTime > 100) {

      // ==========================================
      // ZUSTAND 1: HAUPTMENÜ
      // ==========================================
      if (currentState == STATE_MENU) {
        
        int task1Index = currentPage * TASKS_PER_PAGE;
        int task2Index = task1Index + 1;

        if (task1Index < NUM_TASKS && isHit(x, y, BTN_X, BTN1_Y, BTN_W, BTN_H)) {
          pendingTaskIndex = task1Index;
          currentState = STATE_CONFIRM;
          drawConfirm();
        }
        
        else if (task2Index < NUM_TASKS && isHit(x, y, BTN_X, BTN2_Y, BTN_W, BTN_H)) {
          pendingTaskIndex = task2Index;
          currentState = STATE_CONFIRM;
          drawConfirm();
        }

        else if (currentPage > 0 && isHit(x, y, NAV_PREV_X, NAV_Y, NAV_W, NAV_H)) {
          currentPage--;
          drawMenu();
        }

        else if (currentPage < totalPages - 1 && isHit(x, y, NAV_NEXT_X, NAV_Y, NAV_W, NAV_H)) {
          currentPage++;
          drawMenu();
        }
      }
      
      // ==========================================
      // ZUSTAND 2: BESTÄTIGUNG
      // ==========================================
      else if (currentState == STATE_CONFIRM) {
        
        if (isHit(x, y, CONFIRM_YES_X, CONFIRM_Y, CONFIRM_W, CONFIRM_H)) {
          drawStatus("Sende...");
          sendPushoverMessage(tasks[pendingTaskIndex].message);
          drawStatus("Gesendet!");
          delay(1500); 
          currentState = STATE_MENU;
          drawMenu();
        }

        else if (isHit(x, y, CONFIRM_NO_X, CONFIRM_Y, CONFIRM_W, CONFIRM_H)) {
          currentState = STATE_MENU;
          drawMenu();
        }
      }

      // --- BUGFIX 2: Warten auf Loslassen ---
      // Verhindert versehentliche Doppelklicks und Durchrutschen durch Menüs
      while(tft.getTouch(&x, &y)) { 
        delay(20); 
      }
      
      lastTouchTime = millis();
    }
  }
}

bool isHit(uint16_t tx, uint16_t ty, uint16_t bx, uint16_t by, uint16_t bw, uint16_t bh) {
  return (tx > bx && tx < (bx + bw) && ty > by && ty < (by + bh));
}

void drawMenu() {
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

  if (currentPage > 0) {
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

void drawStatus(String text) {
  tft.fillRect(0, 260, 240, 60, TFT_BLACK); 
  tft.setTextColor(TFT_CYAN);
  tft.setTextSize(2);
  tft.setCursor(60, 280);
  tft.print(text);
}

void sendPushoverMessage(String message) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin("https://api.pushover.net/1/messages.json");
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    String postData = "token=" + String(pushoverApiToken) + 
                      "&user=" + String(pushoverUserKey) + 
                      "&message=" + message; h

    int httpResponseCode = http.POST(postData);

    if (httpResponseCode > 0) {
      Serial.print("Pushover gesendet! Code: ");
      Serial.println(httpResponseCode);
    } else {
      Serial.print("Fehler beim Senden: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  }
}