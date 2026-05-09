#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <ArduinoJson.h>
#include <Preferences.h> 
#include <WebServer.h>

// --- DEINE DATEN HIER EINTRAGEN ---


const int NUM_STAEDTE = 2;
String staedte[NUM_STAEDTE] = {"Schlat,DE", "Faurndau,DE"};
int aktuelleStadtIndex = 0; 
// ----------------------------------

TFT_eSPI tft = TFT_eSPI();
Preferences preferences; 
WebServer server(80); 

// --- KATEGORIEN KONFIGURATION ---
const int CAT_KUECHE = 0;
const int CAT_WAESCHE = 1;
const int CAT_HAUSTIERE = 2;
const int CAT_BAD = 3;
const int CAT_ALLGEMEIN = 4;
const int NUM_CATS = 5;

String catNames[NUM_CATS] = {"Kueche", "Waesche", "Haustiere", "Bad", "Allgemein"};

struct Task {
  int categoryId;
  String title;
  String message;
};

const int NUM_TASKS = 14; 

Task tasks[NUM_TASKS] = {
  {CAT_KUECHE, "Spuelmaschine", "Bitte die Spülmaschine ausräumen! 🍽️"},
  {CAT_KUECHE, "Getraenke hoch", "Wir brauchen mehr Getränke oben! 🥤"},
  {CAT_WAESCHE, "Waesche hoch", "Die Wäsche ist trocken und will nach oben! 🧺"},
  {CAT_WAESCHE, "Waesche in Keller", "Der Wäschekorb vom Bad muss in den Keller! 🧺"},
  {CAT_HAUSTIERE, "Klo putzen", "Das Katzenklo muss geputzt werden! 🐈"},
  {CAT_HAUSTIERE, "Klo nachfuellen", "Bitte das Katzenklo nachfüllen! 🐈"},
  {CAT_HAUSTIERE, "Katzenfutter", "Die Katzen haben Hunger, Futter auffüllen! 🐈‍⬛"},
  {CAT_HAUSTIERE, "Trinkbrunnen", "Den Trinkbrunnen für die Katzen auffüllen! 💧"},
  {CAT_BAD, "Seife auffuellen", "Bitte die Seife im Bad auffüllen! 🧼"},
  {CAT_BAD, "Klopapier", "Klopapier auffüllen, Notfall! 🧻"},
  {CAT_ALLGEMEIN, "Leergut runter", "Bitte das Leergut runterbringen! ♻️"},
  {CAT_ALLGEMEIN, "Gelbe Saecke", "Bitte die gelben Säcke an die Straße stellen! 🟡"},
  {CAT_ALLGEMEIN, "Einkaufskorb", "Den Einkaufskorb hochtragen! 🛒"},
  {CAT_ALLGEMEIN, "Auto ausladen", "Getränke aus dem Auto ausladen! 🚗"}
};

bool taskActive[NUM_TASKS];

// --- VARIABLEN FÜR DIE LOGIK ---
enum AppState { STATE_HOME, STATE_HAUSHALT_CAT, STATE_HAUSHALT_TASKS, STATE_HAUSHALT_CONFIRM, STATE_WETTER, STATE_WETTER_DETAILS, STATE_TODO, STATE_TODO_CONFIRM };
AppState currentState = STATE_HOME;

int currentCatPage = 0;
int currentTaskPage = 0;
int selectedCategory = -1;
int pendingTaskIndex = -1;

const int ITEMS_PER_PAGE = 2; 

int currentTodoPage = 0;
const int TODOS_PER_PAGE = 3; 

unsigned long lastTouchTime = 0;
bool screenNeedsUpdate = false; 

const unsigned long SCREEN_TIMEOUT = 30000; 
bool isScreenOn = true;

// --- WETTER VARIABLEN ---
float w_temp = 0;
float w_minTemp = 0;
float w_maxTemp = 0;
int w_humidity = 0;
float w_wind = 0;
String w_description = "";
bool weatherNeedsUpdate = true; 

// Vorhersage Variablen (Stunden - JETZT 3 EINTRÄGE)
float f_temp[3] = {0, 0, 0};
String f_time[3] = {"", "", ""};
String f_desc[3] = {"", "", ""};

// Vorhersage Variablen (Tage: Morgen, Übermorgen, in 3 Tagen)
float f_day_temp[3] = {0, 0, 0};
String f_day_desc[3] = {"", "", ""};

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

// Koordinaten für 3 Buttons unten im Wetter-Menü
#define WETTER_BTN_W 65
#define W_BTN1_X 10
#define W_BTN2_X 85
#define W_BTN3_X 165

#define CONFIRM_W 90
#define CONFIRM_H 60
#define CONFIRM_Y 160
#define CONFIRM_YES_X 20
#define CONFIRM_NO_X 130


void handleTaskDone() {
  if (server.hasArg("id")) {
    int id = server.arg("id").toInt();
    if (id >= 0 && id < NUM_TASKS) {
      taskActive[id] = false;
      String key = "t" + String(id);
      preferences.putBool(key.c_str(), false);
      
      String html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'></head>";
      html += "<body style='background-color:#121212; color:#87CEEB; font-family:sans-serif; text-align:center; padding-top:20%;'>";
      html += "<h1>✅ Erledigt!</h1>";
      html += "<p>Die Aufgabe <b>" + tasks[id].title + "</b> wurde abgehakt.</p>";
      html += "<p style='color:#777; font-size:12px; margin-top:50px;'>Du kannst diese Seite jetzt schliessen.</p>";
      html += "</body></html>";
      
      server.send(200, "text/html", html);
      screenNeedsUpdate = true; 
    } else {
      server.send(400, "text/plain", "Fehler: Aufgabe nicht gefunden.");
    }
  } else {
    server.send(400, "text/plain", "Fehler: Keine ID uebergeben.");
  }
}


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

  server.on("/done", handleTaskDone);
  server.begin();
  
  Serial.print("\nVerbunden! IP-Adresse: ");
  Serial.println(WiFi.localIP());

  drawHomeScreen();
  lastTouchTime = millis(); 
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    static unsigned long lastReconnectAttempt = 0;
    if (millis() - lastReconnectAttempt > 10000) { 
      Serial.println("WLAN verloren... verbinde neu...");
      WiFi.begin(ssid, password);
      lastReconnectAttempt = millis();
    }
  }

  server.handleClient(); 

  if (isScreenOn && (millis() - lastTouchTime > SCREEN_TIMEOUT)) {
    isScreenOn = false;
    digitalWrite(27, LOW); 
  }

  if (screenNeedsUpdate) {
    screenNeedsUpdate = false;
    if (currentState == STATE_TODO) {
      drawTodoScreen();
    }
  }

  uint16_t x, y;

  if (tft.getTouch(&x, &y)) {

    if (!isScreenOn) {
      isScreenOn = true;
      digitalWrite(27, HIGH); 
      lastTouchTime = millis();
      while(tft.getTouch(&x, &y)) { delay(20); } 
      return; 
    }

    x = 240 - x; 
    
    if (millis() - lastTouchTime > 100) {

      if (currentState == STATE_HOME) {
        if (isHit(x, y, BTN_X, BTN1_Y, BTN_W, BTN_H)) {
          currentState = STATE_HAUSHALT_CAT;
          currentCatPage = 0;
          drawCategoryMenu();
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

      else if (currentState == STATE_HAUSHALT_CAT) {
        int cat1Index = currentCatPage * ITEMS_PER_PAGE;
        int cat2Index = cat1Index + 1;
        int totalCatPages = (NUM_CATS + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE;

        if (cat1Index < NUM_CATS && isHit(x, y, BTN_X, BTN1_Y, BTN_W, BTN_H)) {
          selectedCategory = cat1Index;
          currentState = STATE_HAUSHALT_TASKS;
          currentTaskPage = 0;
          drawTaskMenu();
        }
        else if (cat2Index < NUM_CATS && isHit(x, y, BTN_X, BTN2_Y, BTN_W, BTN_H)) {
          selectedCategory = cat2Index;
          currentState = STATE_HAUSHALT_TASKS;
          currentTaskPage = 0;
          drawTaskMenu();
        }
        else if (isHit(x, y, NAV_PREV_X, NAV_Y, NAV_W, NAV_H)) {
          if (currentCatPage == 0) {
            currentState = STATE_HOME;
            drawHomeScreen();
          } else {
            currentCatPage--;
            drawCategoryMenu();
          }
        }
        else if (currentCatPage < totalCatPages - 1 && isHit(x, y, NAV_NEXT_X, NAV_Y, NAV_W, NAV_H)) {
          currentCatPage++;
          drawCategoryMenu();
        }
      }

      else if (currentState == STATE_HAUSHALT_TASKS) {
        int filteredTasks[NUM_TASKS];
        int count = 0;
        for(int i = 0; i < NUM_TASKS; i++){
          if(tasks[i].categoryId == selectedCategory) {
            filteredTasks[count++] = i;
          }
        }

        int t1Index = currentTaskPage * ITEMS_PER_PAGE;
        int t2Index = t1Index + 1;
        int totalTaskPages = (count + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE;

        if (t1Index < count && isHit(x, y, BTN_X, BTN1_Y, BTN_W, BTN_H)) {
          pendingTaskIndex = filteredTasks[t1Index];
          currentState = STATE_HAUSHALT_CONFIRM;
          drawConfirm();
        }
        else if (t2Index < count && isHit(x, y, BTN_X, BTN2_Y, BTN_W, BTN_H)) {
          pendingTaskIndex = filteredTasks[t2Index];
          currentState = STATE_HAUSHALT_CONFIRM;
          drawConfirm();
        }
        else if (isHit(x, y, NAV_PREV_X, NAV_Y, NAV_W, NAV_H)) {
          if (currentTaskPage == 0) {
            currentState = STATE_HAUSHALT_CAT;
            drawCategoryMenu();
          } else {
            currentTaskPage--;
            drawTaskMenu();
          }
        }
        else if (currentTaskPage < totalTaskPages - 1 && isHit(x, y, NAV_NEXT_X, NAV_Y, NAV_W, NAV_H)) {
          currentTaskPage++;
          drawTaskMenu();
        }
      }
      
      else if (currentState == STATE_HAUSHALT_CONFIRM) {
        if (isHit(x, y, CONFIRM_YES_X, CONFIRM_Y, CONFIRM_W, CONFIRM_H)) {
          drawStatus("Sende...");
          taskActive[pendingTaskIndex] = true;
          String key = "t" + String(pendingTaskIndex);
          preferences.putBool(key.c_str(), true);
          sendPushoverMessage(pendingTaskIndex);
          drawStatus("Gesendet!");
          delay(1500); 
          currentState = STATE_HAUSHALT_TASKS; 
          drawTaskMenu();
        }
        else if (isHit(x, y, CONFIRM_NO_X, CONFIRM_Y, CONFIRM_W, CONFIRM_H)) {
          currentState = STATE_HAUSHALT_TASKS;
          drawTaskMenu();
        }
      }

      else if (currentState == STATE_WETTER) {
        if (isHit(x, y, W_BTN1_X, NAV_Y, WETTER_BTN_W, NAV_H)) { // HOME
          currentState = STATE_HOME;
          drawHomeScreen();
        }
        else if (isHit(x, y, W_BTN2_X, NAV_Y, WETTER_BTN_W, NAV_H)) { // MEHR INFOS
          currentState = STATE_WETTER_DETAILS;
          drawWetterDetailsScreen();
        }
        else if (isHit(x, y, W_BTN3_X, NAV_Y, WETTER_BTN_W, NAV_H)) { // SWAP STADT
          aktuelleStadtIndex++; 
          if (aktuelleStadtIndex >= NUM_STAEDTE) { aktuelleStadtIndex = 0; }
          weatherNeedsUpdate = true; 
          drawWetterScreen(); 
        }
      }

      else if (currentState == STATE_WETTER_DETAILS) {
        if (isHit(x, y, NAV_PREV_X, NAV_Y, NAV_W, NAV_H)) { // ZURÜCK
          currentState = STATE_WETTER;
          drawWetterScreen();
        }
        else if (isHit(x, y, NAV_NEXT_X, NAV_Y, NAV_W, NAV_H)) { // HOME
          currentState = STATE_HOME;
          drawHomeScreen();
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

  if (currentState == STATE_WETTER && weatherNeedsUpdate) {
    fetchAllWeatherData(); 
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

void drawThemeButton(int x, int y, int w, int h, String text, int textSize = 2) {
  tft.fillRoundRect(x, y, w, h, 8, TFT_NAVY);       
  tft.drawRoundRect(x, y, w, h, 8, TFT_SKYBLUE);    
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(textSize);

  int tw = tft.textWidth(text);
  int th = (textSize == 1) ? 8 : 16; 
  int tx = x + (w - tw) / 2;
  int ty = y + (h - th) / 2;
  
  tft.setCursor(tx, ty);
  tft.print(text);
}

void drawHomeScreen() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_SKYBLUE);
  tft.setTextSize(2);
  tft.setCursor(50, 15);
  tft.print("MAIN MENU");

  drawThemeButton(BTN_X, BTN1_Y, BTN_W, BTN_H, "Haushalt");
  drawThemeButton(BTN_X, BTN2_Y, BTN_W, BTN_H, "Wetter");
  drawThemeButton(BTN_X, BTN3_Y, BTN_W, BTN_H, "To-Do's");
}

void drawCategoryMenu() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_SKYBLUE);
  tft.setTextSize(2);
  tft.setCursor(10, 15);
  tft.print("Kategorie wahlen:");

  int cat1 = currentCatPage * ITEMS_PER_PAGE;
  int cat2 = cat1 + 1;
  int totalCatPages = (NUM_CATS + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE;

  if (cat1 < NUM_CATS) {
    drawThemeButton(BTN_X, BTN1_Y, BTN_W, BTN_H, catNames[cat1], 2);
  }
  if (cat2 < NUM_CATS) {
    drawThemeButton(BTN_X, BTN2_Y, BTN_W, BTN_H, catNames[cat2], 2);
  }

  if (currentCatPage == 0) {
    drawThemeButton(NAV_PREV_X, NAV_Y, NAV_W, NAV_H, "HOME");
  } else {
    drawThemeButton(NAV_PREV_X, NAV_Y, NAV_W, NAV_H, "<-");
  }

  if (currentCatPage < totalCatPages - 1) {
    drawThemeButton(NAV_NEXT_X, NAV_Y, NAV_W, NAV_H, "->");
  }
}

void drawTaskMenu() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_SKYBLUE);
  tft.setTextSize(2);
  tft.setCursor(10, 15);
  tft.print(catNames[selectedCategory] + ":");

  int filteredTasks[NUM_TASKS];
  int count = 0;
  for(int i = 0; i < NUM_TASKS; i++){
    if(tasks[i].categoryId == selectedCategory) {
      filteredTasks[count++] = i;
    }
  }

  int t1 = currentTaskPage * ITEMS_PER_PAGE;
  int t2 = t1 + 1;
  int totalTaskPages = (count + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE;

  if (t1 < count) {
    drawThemeButton(BTN_X, BTN1_Y, BTN_W, BTN_H, tasks[filteredTasks[t1]].title, 2);
  }
  if (t2 < count) {
    drawThemeButton(BTN_X, BTN2_Y, BTN_W, BTN_H, tasks[filteredTasks[t2]].title, 2);
  }

  drawThemeButton(NAV_PREV_X, NAV_Y, NAV_W, NAV_H, "<-");

  if (currentTaskPage < totalTaskPages - 1) {
    drawThemeButton(NAV_NEXT_X, NAV_Y, NAV_W, NAV_H, "->");
  }
}

void drawConfirm() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_SKYBLUE);
  tft.setTextSize(2);
  tft.setCursor(20, 30);
  tft.print("Wirklich senden?");
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2); 
  tft.setCursor(20, 70);
  tft.print("> " + tasks[pendingTaskIndex].title);

  tft.fillRoundRect(CONFIRM_YES_X, CONFIRM_Y, CONFIRM_W, CONFIRM_H, 8, TFT_NAVY);
  tft.drawRoundRect(CONFIRM_YES_X, CONFIRM_Y, CONFIRM_W, CONFIRM_H, 8, TFT_GREEN);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(CONFIRM_YES_X + 30, CONFIRM_Y + 20);
  tft.print("JA");

  tft.fillRoundRect(CONFIRM_NO_X, CONFIRM_Y, CONFIRM_W, CONFIRM_H, 8, TFT_NAVY);
  tft.drawRoundRect(CONFIRM_NO_X, CONFIRM_Y, CONFIRM_W, CONFIRM_H, 8, TFT_RED);
  tft.setCursor(CONFIRM_NO_X + 20, CONFIRM_Y + 20);
  tft.print("NEIN");
}

String getWindCategory(float speed) {
  if (speed < 3.0) return "Leicht";
  if (speed < 8.0) return "Mittel";
  if (speed < 14.0) return "Stark";
  return "Sehr stark";
}

void drawWetterScreen() {
  tft.fillScreen(TFT_BLACK);

  tft.setTextColor(TFT_SKYBLUE);
  tft.setTextSize(2);
  tft.setCursor(15, 10);
  String header = "Wetter: " + staedte[aktuelleStadtIndex];
  header.replace(",DE", ""); 
  tft.print(header);
  
  tft.drawLine(10, 30, 230, 30, TFT_SKYBLUE);

  if (weatherNeedsUpdate) {
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(40, 100);
    tft.print("Lade Daten...");
  } else {
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(4);
    tft.setCursor(20, 40);
    tft.print(w_temp, 1); 
    tft.print("C");

    tft.setTextColor(TFT_SKYBLUE);
    tft.setTextSize(2);
    tft.setCursor(20, 75);
    tft.print(w_description);

    tft.setTextColor(TFT_LIGHTGREY);
    tft.setTextSize(1);
    tft.setCursor(20, 105);
    tft.print("Wind: " + getWindCategory(w_wind) + " (" + String(w_wind, 1) + " m/s)");

    tft.drawLine(10, 125, 230, 125, TFT_DARKGREY);
    tft.setTextColor(TFT_SKYBLUE);
    tft.setTextSize(2);
    tft.setCursor(20, 135);
    tft.print("Ausblick:");
    
    // --- NEU: 3 Stunden-Einträge mit Details in TextSize(1) ---
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(1);
    int drawY = 160;
    
    for(int i = 0; i < 3; i++) {
      if(f_time[i] != "") {
        tft.setCursor(20, drawY);
        // Baut den String: "15:00: 12.5C - Bewoelkt"
        tft.print(f_time[i] + ": " + String(f_temp[i], 1) + "C - " + f_desc[i]);
        drawY += 20; 
      }
    }
  }

  drawThemeButton(W_BTN1_X, NAV_Y, WETTER_BTN_W, NAV_H, "HOME", 1);
  drawThemeButton(W_BTN2_X, NAV_Y, WETTER_BTN_W, NAV_H, "MEHR", 1);
  drawThemeButton(W_BTN3_X, NAV_Y, WETTER_BTN_W, NAV_H, "SWAP", 1);
}

void drawWetterDetailsScreen() {
  tft.fillScreen(TFT_BLACK);

  tft.setTextColor(TFT_SKYBLUE);
  tft.setTextSize(2);
  tft.setCursor(15, 10);
  tft.print("Wochenausblick");
  tft.drawLine(10, 30, 230, 30, TFT_SKYBLUE);

  String days[3] = {"Morgen:", "Uebermorgen:", "In 3 Tagen:"};
  int drawY = 45;

  tft.setTextSize(1);
  for(int i=0; i<3; i++) {
    tft.setTextColor(TFT_SKYBLUE);
    tft.setCursor(15, drawY);
    tft.print(days[i]);
    
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(15, drawY + 15);
    tft.print(String(f_day_temp[i], 1) + " C  -  " + f_day_desc[i]);
    
    tft.drawLine(15, drawY + 35, 225, drawY + 35, TFT_DARKGREY);
    drawY += 45;
  }

  drawThemeButton(NAV_PREV_X, NAV_Y, NAV_W, NAV_H, "<-");
  drawThemeButton(NAV_NEXT_X, NAV_Y, NAV_W, NAV_H, "HOME");
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
          tft.fillRoundRect(20, drawY, 200, 35, 5, TFT_NAVY);
          tft.drawRoundRect(20, drawY, 200, 35, 5, TFT_SKYBLUE);
          
          tft.drawRect(30, drawY + 7, 20, 20, TFT_SKYBLUE);
          
          tft.setTextColor(TFT_WHITE);
          tft.setTextSize(2); 
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
    drawThemeButton(NAV_PREV_X, NAV_Y, NAV_W, NAV_H, "HOME");
  } else {
    drawThemeButton(NAV_PREV_X, NAV_Y, NAV_W, NAV_H, "<-");
  }

  if (currentTodoPage < pages - 1) {
    drawThemeButton(NAV_NEXT_X, NAV_Y, NAV_W, NAV_H, "->");
  }
}

void drawTodoConfirm() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_SKYBLUE);
  tft.setTextSize(2);
  tft.setCursor(20, 30);
  tft.print("Schon erledigt?");
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2); 
  tft.setCursor(20, 70);
  tft.print("> " + tasks[pendingTaskIndex].title);

  tft.fillRoundRect(CONFIRM_YES_X, CONFIRM_Y, CONFIRM_W, CONFIRM_H, 8, TFT_NAVY);
  tft.drawRoundRect(CONFIRM_YES_X, CONFIRM_Y, CONFIRM_W, CONFIRM_H, 8, TFT_GREEN);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
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
  
  int tw = tft.textWidth(text);
  tft.setCursor((240 - tw) / 2, 280); 
  tft.print(text);
}

// ==========================================
// NETZWERK-FUNKTIONEN
// ==========================================

void sendPushoverMessage(int tIndex) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin("https://api.pushover.net/1/messages.json");
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    String ip = WiFi.localIP().toString();
    String doneUrl = "http://" + ip + "/done?id=" + String(tIndex);
    
    String postData = "token=" + String(pushoverApiToken) + 
                      "&user=" + String(pushoverUserKey) + 
                      "&message=" + tasks[tIndex].message +
                      "&url=" + doneUrl +
                      "&url_title=Als%20erledigt%20markieren"; 

    int httpResponseCode = http.POST(postData);
    http.end();
  }
}

String replaceUmlaute(String text) {
  text.replace("ä", "ae");
  text.replace("ö", "oe");
  text.replace("ü", "ue");
  text.replace("Ä", "Ae");
  text.replace("Ö", "Oe");
  text.replace("Ü", "Ue");
  text.replace("ß", "ss");
  return text;
}

void fetchAllWeatherData() {
  if (WiFi.status() != WL_CONNECTED) return;
  
  HTTPClient http;
  
  // 1. AKTUELLLES WETTER ABRUFEN
  String url_current = "http://api.openweathermap.org/data/2.5/weather?q=" + staedte[aktuelleStadtIndex] + "&appid=" + String(owmApiKey) + "&units=metric&lang=de";
  http.begin(url_current);
  if (http.GET() > 0) {
    JsonDocument doc;
    if (!deserializeJson(doc, http.getString())) {
      w_temp = doc["main"]["temp"];
      w_minTemp = doc["main"]["temp_min"];
      w_maxTemp = doc["main"]["temp_max"];
      w_humidity = doc["main"]["humidity"];
      w_wind = doc["wind"]["speed"];
      const char* desc = doc["weather"][0]["description"];
      w_description = replaceUmlaute(String(desc)); 
      if(w_description.length() > 0) w_description[0] = toupper(w_description[0]);
    }
  }
  http.end();

  // 2. VORHERSAGE ABRUFEN 
  String url_forecast = "http://api.openweathermap.org/data/2.5/forecast?q=" + staedte[aktuelleStadtIndex] + "&appid=" + String(owmApiKey) + "&units=metric&lang=de&cnt=25";
  http.begin(url_forecast);
  if (http.GET() > 0) {
    JsonDocument doc2;
    if (!deserializeJson(doc2, http.getString())) {
      
      // --- NEU: 3 Einträge für die nächsten Stunden (+3h, +6h, +9h) abrufen ---
      for(int i = 0; i < 3; i++) {
        f_temp[i] = doc2["list"][i+1]["main"]["temp"];
        
        const char* dt = doc2["list"][i+1]["dt_txt"];
        if(dt) f_time[i] = String(dt).substring(11, 16);
        
        const char* hDesc = doc2["list"][i+1]["weather"][0]["description"];
        f_desc[i] = replaceUmlaute(String(hDesc));
        if(f_desc[i].length() > 0) f_desc[i][0] = toupper(f_desc[i][0]);
      }

      int tagIndex = 0;
      for(int i = 7; i <= 23; i += 8) {
        if(tagIndex < 3) {
          f_day_temp[tagIndex] = doc2["list"][i]["main"]["temp"];
          const char* dDesc = doc2["list"][i]["weather"][0]["description"];
          f_day_desc[tagIndex] = replaceUmlaute(String(dDesc)); 
          if(f_day_desc[tagIndex].length() > 0) f_day_desc[tagIndex][0] = toupper(f_day_desc[tagIndex][0]);
          tagIndex++;
        }
      }
    }
  }
  http.end();
}