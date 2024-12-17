#define BLYNK_TEMPLATE_ID           "TMPL6-iZc6gSo"
#define BLYNK_TEMPLATE_NAME         "Final Project"
#define BLYNK_AUTH_TOKEN            "MfrPNin34-Pte1FHmwOXHQqkIFUj5lCP"

#include <WiFi.h>
#include <HTTPClient.h>
#include <BlynkSimpleEsp32.h>
#include "DHT.h"
#include "Fuzzy.h"

// Pin motor
int motor1Pin1 = 27; 
int motor1Pin2 = 26; 
int enable1Pin = 14; 

// Pin DHT11
#define DHTPIN 4
#define DHTTYPE DHT11

// Pin LDR
#define LIGHT_SENSOR_PIN 35

// Pin LED
#define LED 32

bool manualControl = false; // Flag for manual control
bool fanState = false;

int fanOnTemp = 20;

// WiFi credentials
const char* ssid = "Choclat";         // Ganti dengan nama WiFi Anda
const char* pass = "gituyaok"; // Ganti dengan password WiFi Anda

// ThingSpeak API
const char* server = "http://api.thingspeak.com";
String apiKey = "R2YMPS9Q2HOC0DGU"; // Ganti dengan API Key Anda

// Setting PWM motor
const int freq = 30000;
const int pwmChannel = 0;
const int resolution = 8;
int dutyCycle = 255;

// Variabel waktu simulasi
unsigned long simulatedTime = 0; // Waktu simulasi dalam milidetik
const unsigned long hourDuration = 1000; // 1 detik = 1 jam simulasi

// Inisialisasi objek DHT
DHT dht(DHTPIN, DHTTYPE);

// Fuzzy Logic
Fuzzy *fuzzy = new Fuzzy();

void setup() {
  Serial.begin(115200);

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  // Inisialisasi motor
  pinMode(motor1Pin1, OUTPUT);
  pinMode(motor1Pin2, OUTPUT);
  pinMode(enable1Pin, OUTPUT);

      
  // Mengonfigurasi PWM pada pin enable motor
  ledcAttachChannel(enable1Pin, freq, resolution, pwmChannel);

  // Inisialisasi komunikasi serial

  // Inisialisasi sensor DHT
  dht.begin();

  analogSetAttenuation(ADC_11db);

  pinMode(LED, OUTPUT);

  // Menghubungkan ke WiFi
  // connectToWiFi();

  // Konfigurasi Fuzzy Logic
  configureFuzzyLogic();

  Serial.println("Setup complete!");
}

void loop() {
  Blynk.run();
  // Perbarui waktu simulasi
  simulatedTime = millis();
  unsigned int currentHour = (simulatedTime / hourDuration) % 24;

  // Membaca suhu dari DHT11
  float temperature = dht.readTemperature();

  // Periksa jika pembacaan gagal
  if (isnan(temperature)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  // Membaca nilai dari LDR
  int lightValue = analogRead(LIGHT_SENSOR_PIN);

  handleFanControl(temperature);

  // Logika lampu berdasarkan fuzzy logic
  handleLightControl(lightValue, currentHour);

  // Kirim data suhu ke ThingSpeak
  // sendToThingSpeak(temperature, dutyCycle, lightValue);

  // Delay 1 detik
  delay(1000);
}

void configureFuzzyLogic() {
  // Define fuzzy input for light level
  FuzzyInput *light = new FuzzyInput(1); // Index 1 for "light"
  FuzzySet *dark = new FuzzySet(0, 0, 300, 500);
  FuzzySet *dim = new FuzzySet(400, 600, 600, 800);
  FuzzySet *bright = new FuzzySet(700, 1023, 1023, 1023);
  light->addFuzzySet(dark);
  light->addFuzzySet(dim);
  light->addFuzzySet(bright);
  fuzzy->addFuzzyInput(light);

  // Define fuzzy output for LED intensity
  FuzzyOutput *intensity = new FuzzyOutput(1); // Index 1 for "intensity"
  FuzzySet *low = new FuzzySet(0, 0, 50, 100);
  FuzzySet *medium = new FuzzySet(80, 120, 120, 200);
  FuzzySet *high = new FuzzySet(180, 255, 255, 255);
  intensity->addFuzzySet(low);
  intensity->addFuzzySet(medium);
  intensity->addFuzzySet(high);
  fuzzy->addFuzzyOutput(intensity);

  // Define rules
  FuzzyRuleAntecedent *ifLightIsDark = new FuzzyRuleAntecedent();
  ifLightIsDark->joinSingle(dark);

  FuzzyRuleConsequent *thenIntensityIsHigh = new FuzzyRuleConsequent();
  thenIntensityIsHigh->addOutput(high);

  FuzzyRule *rule1 = new FuzzyRule(1, ifLightIsDark, thenIntensityIsHigh);
  fuzzy->addFuzzyRule(rule1);

  FuzzyRuleAntecedent *ifLightIsDim = new FuzzyRuleAntecedent();
  ifLightIsDim->joinSingle(dim);

  FuzzyRuleConsequent *thenIntensityIsMedium = new FuzzyRuleConsequent();
  thenIntensityIsMedium->addOutput(medium);

  FuzzyRule *rule2 = new FuzzyRule(2, ifLightIsDim, thenIntensityIsMedium);
  fuzzy->addFuzzyRule(rule2);

  FuzzyRuleAntecedent *ifLightIsBright = new FuzzyRuleAntecedent();
  ifLightIsBright->joinSingle(bright);

  FuzzyRuleConsequent *thenIntensityIsLow = new FuzzyRuleConsequent();
  thenIntensityIsLow->addOutput(low);

  FuzzyRule *rule3 = new FuzzyRule(3, ifLightIsBright, thenIntensityIsLow);
  fuzzy->addFuzzyRule(rule3);
}

void handleFanControl(float temperature) {
  // if (!isnan(temperature)) {
    Serial.print("Temperature: ");
    Serial.println(temperature);

    // Example: Send temperature to Blynk Virtual Pin V0
  //   Blynk.virtualWrite(V0, temperature);
  // }

  if (temperature > fanOnTemp && manualControl == 0) {
    fanState = true;
    Serial.print("Temperature is above ");
    Serial.print(fanOnTemp);
    Serial.print(" - Fan ON ");
    turnFanOn();
    
  } else if (temperature < fanOnTemp && manualControl == 1) {
    fanState = false;
    Serial.print("Temperature is below ");
    Serial.print(fanOnTemp);
    Serial.print(" - Fan OFF ");
    turnFanOff();
    // dutyCycle = 0;
  } else {
    Serial.print("Temperature is below ");
    Serial.print(fanOnTemp);
    Serial.print(" - Manual Overide ON ");
  }
}

void handleLightControl(int lightValue, unsigned int currentHour) {
  // Aturan tambahan untuk waktu malam (6 sore - 11 malam)
  Serial.println("Time: ");
  Serial.println(currentHour);
  if (currentHour >= 18 && currentHour <= 23) {
    analogWrite(LED, 50); // Lampu otomatis redup
    Serial.println("Evening hours: LED dimmed");
    return;
  }

  // Set fuzzy input and fuzzify
  fuzzy->setInput(1, lightValue); // 1 is the index for "light"
  fuzzy->fuzzify();

  // Get defuzzified intensity
  int ledIntensity = fuzzy->defuzzify(1); // 1 is the index for "intensity"

  // Set LED intensity
  analogWrite(LED, ledIntensity);

  Serial.print("Light Value: ");
  Serial.print(lightValue);
  Serial.print(" | LED Intensity: ");
  Serial.println(ledIntensity);
}

void turnFanOn() {
  digitalWrite(motor1Pin1, HIGH);
  digitalWrite(motor1Pin2, LOW);
  ledcWrite(enable1Pin, dutyCycle);
}

// Function to turn the fan OFF
void turnFanOff() {
  digitalWrite(motor1Pin1, LOW);
  digitalWrite(motor1Pin2, LOW);
  ledcWrite(pwmChannel, 0);
  Serial.println("Fan OFF");
}

BLYNK_WRITE(V5) {
  int value = param.asInt();
  if (value == 1){
    manualControl = true;
  } else {
    manualControl = false;
  } 
}

BLYNK_WRITE(V0) {
  int value = param.asInt();
  
  fanOnTemp = value;
}

BLYNK_WRITE(V1) {
    int value = param.asInt(); // Read button value (0 = OFF, 1 = ON)

    if (value == 1) {
        fanState = true; // Turn fan ON manually
        turnFanOn();
        Serial.println("Manual control enabled: Fan ON");
    } else if (value == 0) {
        turnFanOff();
        Serial.println("Manual control enabled: Fan OFF");
    } else {
        Serial.println("Manual control disabled, switching to auto mode.");
    }
}


// void connectToWiFi() {
//   Serial.print("Connecting to WiFi");
//   WiFi.begin(ssid, pass);

//   while (WiFi.status() != WL_CONNECTED) {
//     delay(500);
//     Serial.print(".");
//   }
//   Serial.println("\nConnected to WiFi!");
// }

// void sendToThingSpeak(float temperature, int fanSpeed, int lightValue) {
//   if (WiFi.status() == WL_CONNECTED) {
//     HTTPClient http;
//     String url = String(server) + "/update?api_key=" + apiKey + 
//                  "&field1=" + String(temperature) + 
//                  "&field2=" + String(fanSpeed) +
//                  "&field3=" + String(lightValue);

//     http.begin(url);
//     int httpResponseCode = http.GET();

//     if (httpResponseCode > 0) {
//       Serial.print("Data sent to ThingSpeak. Response code: ");
//       Serial.println(httpResponseCode);
//     } else {
//       Serial.print("Error sending data: ");
//       Serial.println(http.errorToString(httpResponseCode).c_str());
//     }

//     http.end();
//   } else {
//     Serial.println("WiFi not connected. Unable to send data to ThingSpeak.");
//   }
// }
