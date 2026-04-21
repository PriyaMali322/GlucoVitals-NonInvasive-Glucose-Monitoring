#define BLYNK_PRINT Serial
#define BLYNK_TEMPLATE_ID "TMPL3Tj6SKkuL"
#define BLYNK_TEMPLATE_NAME "Glucovitals"
#define BLYNK_AUTH_TOKEN "fCzFizoCS-6xkRJJDOBYZkHP-dVodoLk"

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "MAX30100_PulseOximeter.h"
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

char ssid[] = "12345678";
char pass[] = "12345678";

PulseOximeter pox;
BlynkTimer timer;
LiquidCrystal_I2C lcd(0x27, 16, 2);

#define LM35_PIN 34
#define GLUCOSE_PIN 35
#define CALIBRATION_OFFSET 7.5

float bpm = 0;
float spo2 = 0;
float temperatureC = 0;

// -------- GLUCOSE VARIABLES --------
int glucose = 100;
String glucoseStatus = "NORMAL";
bool manualMode = false;
unsigned long lastManualTime = 0;

TaskHandle_t maxTask;

// ---------------- MAX30100 TASK (CORE 1) ----------------
void max30100Task(void * parameter)
{
  while (1)
  {
    pox.update();
    vTaskDelay(1);
  }
}

// ---------------- BLYNK SLIDER (V4) ----------------
BLYNK_WRITE(V4)
{
  glucose = param.asInt();     // Manual glucose value
  manualMode = true;
  lastManualTime = millis();
}

// ---------------- SEND DATA FUNCTION ----------------
void sendData()
{
  bpm = pox.getHeartRate();
  spo2 = pox.getSpO2();

  // ---- Temperature ----
  float rawADC = analogRead(LM35_PIN);
  float voltage = rawADC * (3.3 / 4095.0);
  temperatureC = (voltage * 100.0) + CALIBRATION_OFFSET;

// -------- GLUCOSE PIN CHECK --------
if (digitalRead(GLUCOSE_PIN) == LOW)
{
  if (!manualMode)
  {
    int change = random(-2, 3);
    glucose += change;

    if (glucose < 80) glucose = 80;
    if (glucose > 120) glucose = 120;
  }

  // -------- Exit Manual Mode After 10 Seconds --------
  if (manualMode && millis() - lastManualTime > 10000)
  {
    manualMode = false;
  }

// -------- GLUCOSE CLASSIFICATION --------
if (glucose < 70)
  glucoseStatus = "HYPOGLYCEMIA";
else if (glucose > 140)
  glucoseStatus = "HYPERGLYCEMIA";
else
  glucoseStatus = "";   // No status for normal

}
else
{
  glucose = 0;
  glucoseStatus = "NO GLUC";
}

  // -------- Send to Blynk --------
  Blynk.virtualWrite(V0, bpm);
  Blynk.virtualWrite(V1, spo2);
  Blynk.virtualWrite(V2, temperatureC);
  Blynk.virtualWrite(V3, glucose);

// -------- ALERT ONLY FOR HYPO & HYPER --------
if (glucoseStatus == "HYPOGLYCEMIA" || glucoseStatus == "HYPERGLYCEMIA")
{
  Blynk.logEvent("glucose_alert", glucoseStatus);
  Blynk.virtualWrite(V5, glucoseStatus);
}
else{
    Blynk.virtualWrite(V5, " ");
}
  // -------- LCD UPDATE --------

  // BPM
  lcd.setCursor(2,0);
  lcd.print("   ");
  lcd.setCursor(2,0);
  lcd.print((int)bpm);

  // SpO2
  lcd.setCursor(11,0);
  lcd.print("   ");
  lcd.setCursor(11,0);
  lcd.print((int)spo2);

  // Temperature
  lcd.setCursor(2,1);
  lcd.print("     ");
  lcd.setCursor(2,1);
  lcd.print(temperatureC,1);

  // Glucose
  lcd.setCursor(11,1);
  lcd.print("   ");
  lcd.setCursor(11,1);
  lcd.print(glucose);
}

// ---------------- SETUP ----------------
void setup()
{
  Serial.begin(115200);

  Wire.begin(21, 22);
  Wire.setClock(100000);

  pinMode(GLUCOSE_PIN, INPUT);
  pinMode(LM35_PIN, INPUT);

  lcd.init();
  lcd.backlight();

  lcd.setCursor(0,0);
  lcd.print("H:     S:   ");
  lcd.setCursor(0,1);
  lcd.print("T:     G:   ");

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  if (!pox.begin())
  {
    lcd.clear();
    lcd.print("Sensor Error");
    while (1);
  }

  pox.setIRLedCurrent(MAX30100_LED_CURR_50MA);

  // Create MAX30100 task on Core 1
  xTaskCreatePinnedToCore(
    max30100Task,
    "MAX30100 Task",
    4000,
    NULL,
    1,
    &maxTask,
    1);

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  timer.setInterval(1000L, sendData);
}

// ---------------- LOOP ----------------
void loop()
{
  Blynk.run();
  timer.run();
}