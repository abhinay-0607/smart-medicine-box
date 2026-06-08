#include <Wire.h>
#include <RTClib.h>
#include <WiFi.h>
#include <HTTPClient.h>

// USER CONFIGURATION
const char* WIFI_SSID    = "YourSSID";
const char* WIFI_PASS    = "YourPassword";
const char* FCM_KEY      = "YOUR_FCM_SERVER_KEY";
const char* FCM_TOKEN_U1 = "FCM_TOKEN_USER1";
const char* FCM_TOKEN_U2 = "FCM_TOKEN_USER2";
const char* PHONE_U1     = "+91XXXXXXXXXX";
const char* PHONE_U2     = "+91YYYYYYYYYY";

// Slots 0,1,2 = User 1, Slots 3,4,5 = User 2
struct Schedule { int slot; int hr; int min; };
Schedule schedules[] = {
  {0,  8,  0},
  {1, 14,  0},
  {2, 21,  0},
  {3,  8, 30},
  {4, 14, 30},
  {5, 21, 30}
};

// PIN DEFINITIONS
int LED_PIN[6] = {2,  4,  5,  18, 19, 21};
int BUZ_PIN[6] = {13, 14, 15, 25, 26, 27};
int IR_PIN[6]  = {32, 33, 34, 35, 36, 39};
int BTN_PIN[6] = {23,  1,  3,  8,  7,  6};

// TIMING 
int STAGE1_MS   = 30000; // 30 seconds
int STAGE2_MS   = 90000; // 90 seconds
int BUZZ_FAST   = 125;   
int BUZZ_SLOW   = 500;   
int DEBOUNCE_MS = 50;

// STATE MACHINE
enum SlotState { IDLE, STAGE1, STAGE2, ACKED, MISSED };
SlotState slotState[6];
long      alertStart[6];
bool      doseTaken[6];
bool      lastBtnState[6];
long      lastDebounce[6];
bool      scheduleFired[6];
int       lastFiredMin = -1;

RTC_DS3231 rtc;

// SETUP
void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, 16, 17);

  for (int i = 0; i < 6; i++) {
    pinMode(LED_PIN[i], OUTPUT);
    pinMode(BUZ_PIN[i], OUTPUT);
    pinMode(IR_PIN[i],  INPUT);
    pinMode(BTN_PIN[i], INPUT_PULLUP);

    digitalWrite(LED_PIN[i], LOW);
    digitalWrite(BUZ_PIN[i], LOW);

    slotState[i]     = IDLE;
    alertStart[i]    = 0;
    doseTaken[i]     = false;
    lastBtnState[i]  = HIGH;
    lastDebounce[i]  = 0;
    scheduleFired[i] = false;
  }

  Wire.begin();
  if (!rtc.begin()) {
    Serial.println("RTC not found! Check wiring.");
    while (true);
  }
  if (rtc.lostPower()) {
    Serial.println("RTC lost power, resetting to compile time.");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  Serial.println("Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  long stopAt = millis() + 10000;
  while (WiFi.status() != WL_CONNECTED && millis() < stopAt) {
    delay(300);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED)
    Serial.println("\nWiFi connected!");
  else
    Serial.println("\nWiFi failed — 4G fallback active.");

  Serial2.println("AT");
  delay(1000);
  Serial2.println("AT+CMGF=1");
  delay(500);

  Serial.println("Medicine box ready!");
}

// LOOP 
void loop() {
  DateTime now = rtc.now();
  long ms = millis();

  // reset fired flags every new minute
  if (now.minute() != lastFiredMin) {
    lastFiredMin = now.minute();
    for (int i = 0; i < 6; i++) scheduleFired[i] = false;
  }

  // check if any slot needs to trigger
  for (int i = 0; i < 6; i++) {
    Schedule s = schedules[i];
    bool itsTime = (now.hour() == s.hr && now.minute() == s.min);

    if (itsTime && slotState[i] == IDLE && !scheduleFired[i]) {
      scheduleFired[i] = true;
      slotState[i]     = STAGE1;
      alertStart[i]    = ms;
      doseTaken[i]     = false;
      Serial.println("Slot triggered: " + String(i));
      sendWiFiNotification(i);
    }
  }

  // run every slot
  for (int i = 0; i < 6; i++) {
    handleSlot(i, ms);
  }

  delay(50);
}

// SLOT HANDLER
void handleSlot(int i, long ms) {

  if (slotState[i] == IDLE || slotState[i] == ACKED || slotState[i] == MISSED) {
    digitalWrite(LED_PIN[i], LOW);
    digitalWrite(BUZ_PIN[i], LOW);
    return;
  }

  bool irOpen  = (digitalRead(IR_PIN[i]) == LOW);
  bool btnPres = readButton(i);

  if (irOpen || btnPres) {
    slotState[i]  = ACKED;
    doseTaken[i]  = true;
    digitalWrite(LED_PIN[i], LOW);
    digitalWrite(BUZ_PIN[i], LOW);
    Serial.println("Slot " + String(i) + " acknowledged.");
    return;
  }

  long elapsed = ms - alertStart[i];

  // STAGE 1 — gentle reminder
  if (slotState[i] == STAGE1) {
    if (elapsed < STAGE1_MS) {
      digitalWrite(LED_PIN[i], HIGH);
      digitalWrite(BUZ_PIN[i], (millis() / BUZZ_SLOW) % 2);
    } else {
      slotState[i] = STAGE2;
      Serial.println("Slot " + String(i) + " escalating to STAGE2.");
      send4GAlert(i < 3 ? 0 : 1);
    }
  }

  // STAGE 2 — emergency
  if (slotState[i] == STAGE2) {
    if (elapsed < STAGE2_MS) {
      bool flash = (millis() / BUZZ_FAST) % 2;
      digitalWrite(LED_PIN[i], flash);
      digitalWrite(BUZ_PIN[i], flash);
    } else {
      slotState[i] = MISSED;
      digitalWrite(LED_PIN[i], LOW);
      digitalWrite(BUZ_PIN[i], LOW);
      Serial.println("Slot " + String(i) + " missed.");
    }
  }
}

// WIFI NOTIFICATION 
void sendWiFiNotification(int slot) {
  if (WiFi.status() != WL_CONNECTED) return;

  const char* token = (slot < 3) ? FCM_TOKEN_U1 : FCM_TOKEN_U2;
  int user = (slot < 3) ? 1 : 2;
  int snum = (slot % 3) + 1;

  HTTPClient http;
  http.begin("https://fcm.googleapis.com/fcm/send");
  http.addHeader("Authorization", String("key=") + FCM_KEY);
  http.addHeader("Content-Type", "application/json");

  String body = "{";
  body += "\"to\":\"" + String(token) + "\",";
  body += "\"notification\":{";
  body += "\"title\":\"Medicine Reminder\",";
  body += "\"body\":\"User " + String(user) + " - Slot " + String(snum) + " is due!\",";
  body += "\"sound\":\"default\"";
  body += "},";
  body += "\"priority\":\"high\"";
  body += "}";

  int code = http.POST(body);
  Serial.println("Notification sent, response: " + String(code));
  http.end();
}

// 4G ALERT
void send4GAlert(int userIdx) {
  const char* phone = (userIdx == 0) ? PHONE_U1 : PHONE_U2;
  Serial.println("Calling user " + String(userIdx + 1) + " at " + String(phone));

  // missed call
  Serial2.println("AT");
  delay(500);
  Serial2.println(String("ATD") + phone + ";");
  delay(5000);
  Serial2.println("ATH");
  delay(500);

  // SMS
  Serial2.println("AT+CMGF=1");
  delay(300);
  Serial2.println(String("AT+CMGS=\"") + phone + "\"");
  delay(300);
  Serial2.print("URGENT: Take your medicine now! - Smart MedBox");
  Serial2.write(26);
  delay(3000);

  Serial.println("4G alert done.");
}

// BUTTON READ
bool readButton(int i) {
  bool current = digitalRead(BTN_PIN[i]);

  if (current != lastBtnState[i])
    lastDebounce[i] = millis();

  if (millis() - lastDebounce[i] > DEBOUNCE_MS) {
    if (current == LOW && lastBtnState[i] == HIGH) {
      lastBtnState[i] = LOW;
      return true;
    }
    lastBtnState[i] = current;
  }
  return false;
} 
