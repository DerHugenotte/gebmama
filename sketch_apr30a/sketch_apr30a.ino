#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <ArduinoJson.h>
#include <Preferences.h> 

// --- DEINE DATEN HIER EINTRAGEN ---


const int NUM_STAEDTE = 2;
String staedte[NUM_STAEDTE] = {"Schlat,DE", "Faurndau,DE"};
int aktuelleStadtIndex = 0; 
// ----------------------------------

TFT_eSPI tft = TFT_eSPI();
Preferences preferences; 

// --- AUFGABEN-KONFIGURATION ---
struct Task {
  String title;
  String message;
};

// Die Farben wurden hier entfernt, das System nutzt nun das Dark Theme!
const int NUM_TASKS = 4; 

Task tasks[NUM_TASKS] = {
  {"Waesche Keller", "Die Wäsche ist trocken und will nach oben! 🧺"},
  {"Waesche Bad", "Die Wäsche im Bad ist voll, ab nach unten! 🧺"},
  {"Flaschen", "Bitte die Flaschen runterbringen! 🗑️"},
  {"Geschirr", "Ab in die Küche: Geschirr runterbringen! 🍽️"}
};

bool taskActive[NUM_TASKS] = {false, false, false, false};

// --- VARIABLEN FÜR DIE LOGIK ---
enum AppState { STATE_HOME, STATE_HAUSHALT_MENU, STATE_HAUSHALT_CONFIRM, STATE_WETTER, STATE_TODO, STATE_TODO_CONFIRM };
AppState currentState = STATE_HOME;

int currentPage = 0;
const int TASKS_PER_PAGE = 2;
int totalPages = (NUM_TASKS + TASKS_PER_PAGE - 1) / TASKS_PER_PAGE;
int pendingTaskIndex = -1;

int currentTodoPage = 0;
const int TODOS_PER_PAGE = 3; 

unsigned long lastTouchTime = 0;

// Wetter Variablen
float w_temp = 0;
float w_minTemp = 0;
float w_maxTemp = 0;
int w_humidity = 0;
float w_wind = 0;
String w_description = "";
bool weatherNeedsUpdate = true; 

// --- LAYOUT KOORDINATEN ---
#define BTN_W 200
#define BTN_H 60
#define BTN_X 20
#define BTN1_Y 40
#define BTN2_Y 115
#define BTN3_Y 190

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

  preferences.begin("todos", false); 
  for (int i = 0; i < NUM_TASKS; i++) {
    String key = "t" + String(i); 
    taskActive[i] = preferences.getBool(key.c_str(), false); 
  }

  tft.setTextColor(TFT_SKYBLUE);
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
        else if (isHit(x, y, BTN_X, BTN3_Y, BTN_W, BTN_H)) {
          currentState = STATE_TODO;
          currentTodoPage = 0; 
          drawTodoScreen(); 
        }
      }

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
      
      else if (currentState == STATE_HAUSHALT_CONFIRM) {
        if (isHit(x, y, CONFIRM_YES_X, CONFIRM_Y, CONFIRM_W, CONFIRM_H)) {
          drawStatus("Sende...");
          sendPushoverMessage(tasks[pendingTaskIndex].message);
          
          taskActive[pendingTaskIndex] = true;
          String key = "t" + String(pendingTaskIndex);
          preferences.putBool(key.c_str(), true);

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

      else if (currentState == STATE_WETTER) {
        if (isHit(x, y, NAV_PREV_X, NAV_Y, NAV_W, NAV_H)) {
          currentState = STATE_HOME;
          drawHomeScreen();
        }
        else if (isHit(x, y, NAV_NEXT_X, NAV_Y, NAV_W, NAV_H)) {
          aktuelleStadtIndex++; 
          if (aktuelleStadtIndex >= NUM_STAEDTE) {
            aktuelleStadtIndex = 0;
          }
          weatherNeedsUpdate = true; 
          drawWetterScreen(); 
        }
      }

      else if (currentState == STATE_TODO) {
        int totalActiveTasks = 0;
        for (int i = 0; i < NUM_TASKS; i++) {
          if (taskActive[i]) totalActiveTasks++;
        }
        int totalTodoPages = (totalActiveTasks + TODOS_PER_PAGE - 1) / TODOS_PER_PAGE;
        if(totalTodoPages == 0) totalTodoPages = 1;

        if (isHit(x, y, NAV_PREV_X, NAV_Y, NAV_W, NAV_H)) {
          if (currentTodoPage == 0) {
            currentState = STATE_HOME;
            drawHomeScreen();
          } else {
            currentTodoPage--;
            drawTodoScreen();
          }
        } 
        else if (currentTodoPage < totalTodoPages - 1 && isHit(x, y, NAV_NEXT_X, NAV_Y, NAV_W, NAV_H)) {
          currentTodoPage++;
          drawTodoScreen();
        } 
        else {
          int activeTaskCount = 0;
          int startIndex = currentTodoPage * TODOS_PER_PAGE;
          int endIndex = startIndex + TODOS_PER_PAGE;
          int checkY = 50;

          for (int i = 0; i < NUM_TASKS; i++) {
            if (taskActive[i]) {
              if (activeTaskCount >= startIndex && activeTaskCount < endIndex) {
                if (isHit(x, y, 20, checkY, 200, 35)) {
                  pendingTaskIndex = i;
                  currentState = STATE_TODO_CONFIRM;
                  drawTodoConfirm();
                  break; 
                }
                checkY += 45; 
              }
              activeTaskCount++;
            }
          }
        }
      }
      
      else if (currentState == STATE_TODO_CONFIRM) {
        if (isHit(x, y, CONFIRM_YES_X, CONFIRM_Y, CONFIRM_W, CONFIRM_H)) {
          taskActive[pendingTaskIndex] = false;
          String key = "t" + String(pendingTaskIndex);
          preferences.putBool(key.c_str(), false);
          currentState = STATE_TODO;
          drawTodoScreen();
        }
        else if (isHit(x, y, CONFIRM_NO_X, CONFIRM_Y, CONFIRM_W, CONFIRM_H)) {
          currentState = STATE_TODO;
          drawTodoScreen();
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
// GRAFIK-FUNKTIONEN (DARK MODE THEME)
// ==========================================

// Hilfsfunktion für einheitliche Buttons
void drawThemeButton(int x, int y, int w, int h, String text, int textOffsetX) {
  tft.fillRoundRect(x, y, w, h, 8, TFT_NAVY);         // Dunkler Hintergrund
  tft.drawRoundRect(x, y, w, h, 8, TFT_SKYBLUE);      // Blauer Akzent-Rand
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(x + textOffsetX, y + (h/2) - 7);      // Text vertikal zentrieren
  tft.print(text);
}

void drawHomeScreen() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_SKYBLUE);
  tft.setTextSize(2);
  tft.setCursor(50, 15);
  tft.print("MAIN MENU");

  drawThemeButton(BTN_X, BTN1_Y, BTN_W, BTN_H, "Haushalt", 50);
  drawThemeButton(BTN_X, BTN2_Y, BTN_W, BTN_H, "Wetter", 60);
  drawThemeButton(BTN_X, BTN3_Y, BTN_W, BTN_H, "To-Do's", 55);
}

void drawHaushaltMenu() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_SKYBLUE);
  tft.setTextSize(2);
  tft.setCursor(20, 15);
  tft.print("Aufgabe waehlen:");

  int task1Index = currentPage * TASKS_PER_PAGE;
  int task2Index = task1Index + 1;

  if (task1Index < NUM_TASKS) {
    drawThemeButton(BTN_X, BTN1_Y, BTN_W, BTN_H, tasks[task1Index].title, 20);
  }
  if (task2Index < NUM_TASKS) {
    drawThemeButton(BTN_X, BTN2_Y, BTN_W, BTN_H, tasks[task2Index].title, 20);
  }

  if (currentPage == 0) {
    drawThemeButton(NAV_PREV_X, NAV_Y, NAV_W, NAV_H, "HOME", 15);
  } else {
    drawThemeButton(NAV_PREV_X, NAV_Y, NAV_W, NAV_H, "<-", 25);
  }

  if (currentPage < totalPages - 1) {
    drawThemeButton(NAV_NEXT_X, NAV_Y, NAV_W, NAV_H, "->", 25);
  }
}

void drawConfirm() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_SKYBLUE);
  tft.setTextSize(2);
  tft.setCursor(20, 30);
  tft.print("Wirklich senden?");
  
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(20, 70);
  tft.print("> " + tasks[pendingTaskIndex].title);

  // JA Button (Navy mit grünem Rand)
  tft.fillRoundRect(CONFIRM_YES_X, CONFIRM_Y, CONFIRM_W, CONFIRM_H, 8, TFT_NAVY);
  tft.drawRoundRect(CONFIRM_YES_X, CONFIRM_Y, CONFIRM_W, CONFIRM_H, 8, TFT_GREEN);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(CONFIRM_YES_X + 30, CONFIRM_Y + 20);
  tft.print("JA");

  // NEIN Button (Navy mit rotem Rand)
  tft.fillRoundRect(CONFIRM_NO_X, CONFIRM_Y, CONFIRM_W, CONFIRM_H, 8, TFT_NAVY);
  tft.drawRoundRect(CONFIRM_NO_X, CONFIRM_Y, CONFIRM_W, CONFIRM_H, 8, TFT_RED);
  tft.setCursor(CONFIRM_NO_X + 20, CONFIRM_Y + 20);
  tft.print("NEIN");
}

void drawWetterScreen() {
  tft.fillScreen(TFT_BLACK);

  tft.setTextColor(TFT_SKYBLUE);
  tft.setTextSize(2);
  tft.setCursor(20, 15);
  String header = "Wetter: " + staedte[aktuelleStadtIndex];
  header.replace(",DE", ""); 
  tft.print(header);
  
  tft.drawLine(20, 35, 220, 35, TFT_SKYBLUE);

  if (weatherNeedsUpdate) {
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(40, 100);
    tft.print("Lade Daten...");
  } else {
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(5);
    tft.setCursor(30, 50);
    tft.print(w_temp, 1); 
    tft.print("C");

    tft.setTextColor(TFT_SKYBLUE);
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

  drawThemeButton(NAV_PREV_X, NAV_Y, NAV_W, NAV_H, "HOME", 15);
  drawThemeButton(NAV_NEXT_X, NAV_Y, NAV_W, NAV_H, "SWAP", 15);
}

void drawTodoScreen() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_SKYBLUE);
  tft.setTextSize(2);
  tft.setCursor(20, 15);
  tft.print("Aktuelle To-Do's:");

  int totalActiveTasks = 0;
  for (int i = 0; i < NUM_TASKS; i++) {
    if (taskActive[i]) totalActiveTasks++;
  }

  if (totalActiveTasks == 0) {
    tft.setTextColor(TFT_LIGHTGREY);
    tft.setCursor(20, 120);
    tft.print("Alles erledigt! :)");
  } else {
    int totalTodoPages = (totalActiveTasks + TODOS_PER_PAGE - 1) / TODOS_PER_PAGE;
    if (currentTodoPage >= totalTodoPages) {
      currentTodoPage = totalTodoPages - 1;
    }
    if (currentTodoPage < 0) currentTodoPage = 0;

    int activeTaskCount = 0;
    int startIndex = currentTodoPage * TODOS_PER_PAGE;
    int endIndex = startIndex + TODOS_PER_PAGE;
    int drawY = 50;

    for (int i = 0; i < NUM_TASKS; i++) {
      if (taskActive[i]) {
        if (activeTaskCount >= startIndex && activeTaskCount < endIndex) {
          // Listen-Element zeichnen (Navy-Hintergrund, hellblauer Rahmen)
          tft.fillRoundRect(20, drawY, 200, 35, 5, TFT_NAVY);
          tft.drawRoundRect(20, drawY, 200, 35, 5, TFT_SKYBLUE);
          
          // Checkbox zeichnen (Hellblau)
          tft.drawRect(30, drawY + 7, 20, 20, TFT_SKYBLUE);
          
          tft.setTextColor(TFT_WHITE);
          tft.setCursor(60, drawY + 10);
          tft.print(tasks[i].title);
          
          drawY += 45; 
        }
        activeTaskCount++;
      }
    }
  }

  int pages = (totalActiveTasks + TODOS_PER_PAGE - 1) / TODOS_PER_PAGE;
  if(pages == 0) pages = 1;

  if (currentTodoPage == 0) {
    drawThemeButton(NAV_PREV_X, NAV_Y, NAV_W, NAV_H, "HOME", 15);
  } else {
    drawThemeButton(NAV_PREV_X, NAV_Y, NAV_W, NAV_H, "<-", 25);
  }

  if (currentTodoPage < pages - 1) {
    drawThemeButton(NAV_NEXT_X, NAV_Y, NAV_W, NAV_H, "->", 25);
  }
}

void drawTodoConfirm() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_SKYBLUE);
  tft.setTextSize(2);
  tft.setCursor(20, 30);
  tft.print("Schon erledigt?");
  
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(20, 70);
  tft.print("> " + tasks[pendingTaskIndex].title);

  tft.fillRoundRect(CONFIRM_YES_X, CONFIRM_Y, CONFIRM_W, CONFIRM_H, 8, TFT_NAVY);
  tft.drawRoundRect(CONFIRM_YES_X, CONFIRM_Y, CONFIRM_W, CONFIRM_H, 8, TFT_GREEN);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(CONFIRM_YES_X + 30, CONFIRM_Y + 20);
  tft.print("JA");

  tft.fillRoundRect(CONFIRM_NO_X, CONFIRM_Y, CONFIRM_W, CONFIRM_H, 8, TFT_NAVY);
  tft.drawRoundRect(CONFIRM_NO_X, CONFIRM_Y, CONFIRM_W, CONFIRM_H, 8, TFT_RED);
  tft.setCursor(CONFIRM_NO_X + 20, CONFIRM_Y + 20);
  tft.print("NEIN");
}

void drawStatus(String text) {
  tft.fillRect(0, 260, 240, 60, TFT_BLACK); 
  tft.setTextColor(TFT_SKYBLUE);
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