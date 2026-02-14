#include <Adafruit_NeoPixel.h>
#include <ArduinoOTA.h>
#include <HomeSpan.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <math.h>

constexpr char HOST_NAME[] = "LightBar";
constexpr char OTA_PASSWORD[] = "ota";
constexpr char PREF_NAMESPACE[] = "lightbar";
constexpr char PREF_KEY_SSID[] = "ssid";
constexpr char PREF_KEY_PASS[] = "pass";

constexpr uint8_t LED_PIN = 4;
constexpr uint16_t LED_COUNT = 26;
constexpr uint8_t CHASE_STEPS = 10;
constexpr uint8_t CHASE_STEP_DELAY_MS = 3;

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
String inputBuffer;
Preferences prefs;

uint8_t currentR = 0;
uint8_t currentG = 0;
uint8_t currentB = 0;

void setAllLeds(uint8_t r, uint8_t g, uint8_t b) {
  for (uint16_t i = 0; i < LED_COUNT; i++) {
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
  strip.show();
}

void animateToColor(uint8_t targetR, uint8_t targetG, uint8_t targetB) {
  setAllLeds(currentR, currentG, currentB);

  int16_t dR = static_cast<int16_t>(targetR) - static_cast<int16_t>(currentR);
  int16_t dG = static_cast<int16_t>(targetG) - static_cast<int16_t>(currentG);
  int16_t dB = static_cast<int16_t>(targetB) - static_cast<int16_t>(currentB);

  for (uint16_t i = 0; i < LED_COUNT; i++) {
    for (uint8_t step = 1; step <= CHASE_STEPS; step++) {
      uint8_t r = currentR + (dR * step) / CHASE_STEPS;
      uint8_t g = currentG + (dG * step) / CHASE_STEPS;
      uint8_t b = currentB + (dB * step) / CHASE_STEPS;
      strip.setPixelColor(i, strip.Color(r, g, b));
      strip.show();
      delay(CHASE_STEP_DELAY_MS);
    }
    strip.setPixelColor(i, strip.Color(targetR, targetG, targetB));
  }

  strip.show();
  currentR = targetR;
  currentG = targetG;
  currentB = targetB;
}

String normalizeInput(String s) {
  s.trim();
  s.toLowerCase();
  return s;
}

bool parseHexColor(const String &s, uint8_t &r, uint8_t &g, uint8_t &b) {
  String hex = s;
  if (hex.startsWith("#")) {
    hex = hex.substring(1);
  }

  if (hex.length() != 6) {
    return false;
  }

  for (uint8_t i = 0; i < hex.length(); i++) {
    if (!isxdigit(hex[i])) {
      return false;
    }
  }

  long value = strtol(hex.c_str(), nullptr, 16);
  r = (value >> 16) & 0xFF;
  g = (value >> 8) & 0xFF;
  b = value & 0xFF;
  return true;
}

bool parseRgbCsv(const String &s, uint8_t &r, uint8_t &g, uint8_t &b) {
  int c1 = s.indexOf(',');
  int c2 = s.indexOf(',', c1 + 1);
  if (c1 < 0 || c2 < 0) {
    return false;
  }

  String rStr = s.substring(0, c1);
  String gStr = s.substring(c1 + 1, c2);
  String bStr = s.substring(c2 + 1);

  rStr.trim();
  gStr.trim();
  bStr.trim();

  int ri = rStr.toInt();
  int gi = gStr.toInt();
  int bi = bStr.toInt();

  if (ri < 0 || ri > 255 || gi < 0 || gi > 255 || bi < 0 || bi > 255) {
    return false;
  }

  r = static_cast<uint8_t>(ri);
  g = static_cast<uint8_t>(gi);
  b = static_cast<uint8_t>(bi);
  return true;
}

bool parseNamedColor(const String &s, uint8_t &r, uint8_t &g, uint8_t &b) {
  if (s == "red") {
    r = 255;
    g = 0;
    b = 0;
    return true;
  }
  if (s == "green") {
    r = 0;
    g = 255;
    b = 0;
    return true;
  }
  if (s == "blue") {
    r = 0;
    g = 0;
    b = 255;
    return true;
  }
  if (s == "white") {
    r = 255;
    g = 255;
    b = 255;
    return true;
  }
  if (s == "warmwhite" || s == "warm white") {
    r = 255;
    g = 147;
    b = 41;
    return true;
  }
  if (s == "yellow") {
    r = 255;
    g = 255;
    b = 0;
    return true;
  }
  if (s == "orange") {
    r = 255;
    g = 80;
    b = 0;
    return true;
  }
  if (s == "purple") {
    r = 128;
    g = 0;
    b = 128;
    return true;
  }
  if (s == "pink") {
    r = 255;
    g = 20;
    b = 147;
    return true;
  }
  if (s == "cyan") {
    r = 0;
    g = 255;
    b = 255;
    return true;
  }
  if (s == "magenta") {
    r = 255;
    g = 0;
    b = 255;
    return true;
  }
  if (s == "off" || s == "black") {
    r = 0;
    g = 0;
    b = 0;
    return true;
  }
  return false;
}

bool parseColorInput(String raw, uint8_t &r, uint8_t &g, uint8_t &b) {
  String s = normalizeInput(raw);

  if (parseNamedColor(s, r, g, b)) {
    return true;
  }

  if (parseHexColor(s, r, g, b)) {
    return true;
  }

  if (parseRgbCsv(s, r, g, b)) {
    return true;
  }

  return false;
}

void printHelp() {
  Serial.println(F("Type a color, then press Enter:"));
  Serial.println(F("- Name: red, blue, green, white, warm white, cyan, magenta, off"));
  Serial.println(F("- Hex: #FF00AA or FF00AA"));
  Serial.println(F("- RGB: 255,0,170"));
}

void handleSerialInput() {
  while (Serial.available() > 0) {
    char c = static_cast<char>(Serial.read());

    if (c == '\r') {
      continue;
    }

    if (c == '\n') {
      if (inputBuffer.length() == 0) {
        continue;
      }

      uint8_t r, g, b;
      if (parseColorInput(inputBuffer, r, g, b)) {
        animateToColor(r, g, b);
        Serial.print(F("Color set to RGB("));
        Serial.print(r);
        Serial.print(',');
        Serial.print(g);
        Serial.print(',');
        Serial.print(b);
        Serial.println(')');
      } else {
        Serial.print(F("Could not parse: "));
        Serial.println(inputBuffer);
        printHelp();
      }

      inputBuffer = "";
    } else {
      inputBuffer += c;
    }
  }
}

struct LightBarService : Service::LightBulb {
  SpanCharacteristic *power;
  SpanCharacteristic *H;
  SpanCharacteristic *S;
  SpanCharacteristic *V;

  LightBarService() : Service::LightBulb() {
    power = new Characteristic::On(0);
    H = new Characteristic::Hue(0);
    S = new Characteristic::Saturation(0);
    V = new Characteristic::Brightness(100);
    V->setRange(1, 100, 1);
  }

  boolean update() override {
    bool p = power->getNewVal();
    float h = H->getNewVal<float>();
    float s = S->getNewVal<float>();
    float v = V->getNewVal<float>();

    float rf = 0;
    float gf = 0;
    float bf = 0;
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;

    if (p) {
      LedPin::HSVtoRGB(h, s / 100.0f, v / 100.0f, &rf, &gf, &bf);
      r = static_cast<uint8_t>(roundf(rf * 255.0f));
      g = static_cast<uint8_t>(roundf(gf * 255.0f));
      b = static_cast<uint8_t>(roundf(bf * 255.0f));
    }

    animateToColor(r, g, b);

    return true;
  }
};

void setupWiFiWithManager() {
  WiFi.mode(WIFI_STA);

  String ssid;
  String pass;

  prefs.begin(PREF_NAMESPACE, true);
  ssid = prefs.getString(PREF_KEY_SSID, "");
  pass = prefs.getString(PREF_KEY_PASS, "");
  prefs.end();

  if (ssid.length() > 0) {
    Serial.print(F("Trying saved WiFi: "));
    Serial.println(ssid);
    WiFi.begin(ssid.c_str(), pass.c_str());

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
      delay(250);
    }
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("Saved WiFi failed, starting WiFiManager AP..."));
    WiFi.disconnect(true, true);

    WiFiManager wm;
    wm.setHostname(HOST_NAME);
    wm.setConfigPortalTimeout(180);

    bool connected = wm.autoConnect("LightBar-Setup");
    if (!connected) {
      Serial.println(F("WiFiManager failed. Restarting..."));
      delay(1500);
      ESP.restart();
    }

    ssid = WiFi.SSID();
    pass = wm.getWiFiPass(true);

    prefs.begin(PREF_NAMESPACE, false);
    prefs.putString(PREF_KEY_SSID, ssid);
    prefs.putString(PREF_KEY_PASS, pass);
    prefs.end();
  }

  if (ssid.length() > 0) {
    homeSpan.setWifiCredentials(ssid.c_str(), pass.c_str());
  }

  Serial.print(F("WiFi connected. IP: "));
  Serial.println(WiFi.localIP());
}

void setupOTA() {
  ArduinoOTA.setHostname(HOST_NAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.begin();
  Serial.println(F("ArduinoOTA ready."));
}

void setupHomeSpan() {
  homeSpan.setHostNameSuffix("");
  homeSpan.begin(Category::Lighting, "LightBar", HOST_NAME);

  new SpanAccessory();
    new Service::AccessoryInformation();
      new Characteristic::Identify();
      new Characteristic::Name("LightBar");
      new Characteristic::Manufacturer("Joe");
      new Characteristic::Model("ESP32 LightBar");
      new Characteristic::SerialNumber("LB-001");
      new Characteristic::FirmwareRevision("1.0");
    new LightBarService();

  Serial.println(F("HomeSpan ready."));
}

void setup() {
  Serial.begin(115200);

  strip.begin();
  strip.setBrightness(255);
  strip.show();

  setupWiFiWithManager();
  setupOTA();
  setupHomeSpan();

  Serial.println(F("WS2812 serial + HomeKit color controller ready."));
  printHelp();
}

void loop() {
  homeSpan.poll();
  ArduinoOTA.handle();
  handleSerialInput();
}
