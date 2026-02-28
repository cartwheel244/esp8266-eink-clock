#include "Adafruit_ThinkInk.h"
#include "secrets.h"
#include "weather_icons.h"
#include <Adafruit_GFX.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <WiFiClientSecure.h>
#include <time.h>

// =========================================================================
// CONFIGURATION: WI-FI & TIME
// =========================================================================
const char *WIFI_SSID = SECRET_WIFI_SSID;
const char *WIFI_PASSWORD = SECRET_WIFI_PASSWORD;

// NTP Servers
const char *NTP_SERVER_1 = "pool.ntp.org";
const char *NTP_SERVER_2 = "time.nist.gov";

// POSIX Timezone Strings
const char *TZ_EASTERN = "EST5EDT,M3.2.0,M11.1.0";
const char *TZ_CENTRAL = "CST6CDT,M3.2.0,M11.1.0";

// =========================================================================
// CONFIGURATION: WEATHER
// =========================================================================
// Register at openweathermap.org and place API key in secrets.h
const char *OWM_API_KEY = SECRET_OPENWEATHER_API_KEY;

// Hardware Buttons connected to ESP8266 GPIO
#define BUTTON_A 0  // Left Button
#define BUTTON_B 16 // Center Button (Unused)
#define BUTTON_C 2  // Right Button

String activeCity = "Woburn,MA,US";
String displayCityName = "Woburn";

// =========================================================================
// CONFIGURATION: eINK DISPLAY (Feather Wing 2.13" Monochrome)
// =========================================================================
#define EPD_CS 0
#define EPD_DC 15
#define SRAM_CS 16
#define EPD_RESET -1
#define EPD_BUSY                                                               \
  -1 // Can set to -1 on FeatherWings since we don't always use it

// The 2.9" Grayscale FeatherWing (Product 4777) uses the SSD1680 chipset
ThinkInk_290_Grayscale4_EAAMFGN display(EPD_DC, EPD_RESET, EPD_CS, SRAM_CS,
                                        EPD_BUSY);

// Globals
int lastMinute = -1;
int lastWeatherMinute = -1; // To check weather every 15 minutes
const char *days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

struct WeatherData {
  float temp;
  String iconCode;
  bool valid;
};
WeatherData currentWeather = {0.0, "", false};

// Function prototypes
void fetchWeather();
void updateDisplay(struct tm *timeinfo);
// Reclaim Pins: Set the shared SPI pins (12, 13, 14) back to INPUT_PULLUP to
// read the buttons!
void reclaimButtons() {
  pinMode(12, INPUT_PULLUP); // Button B (Middle)
  pinMode(13, INPUT_PULLUP); // Button C (Left)
  pinMode(14, INPUT_PULLUP); // Button A (Right)
}

// Prepare Display: Restore pins to SPI mode
void prepareDisplay() {
  pinMode(EPD_CS, OUTPUT);
  pinMode(SRAM_CS, OUTPUT);
  pinMode(EPD_DC, OUTPUT);

  // Re-initialize SPI hardware to take back Pins 13 (MOSI) and 14 (SCK)
  // from our manual pinMode(INPUT_PULLUP) state.
  SPI.begin();
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n--- ESP8266 eInk Clock ---");

  // --- Display & Hardware Setup ---
  Serial.println("Initializing eInk display and buttons...");
  pinMode(BUTTON_A, INPUT_PULLUP);
  pinMode(BUTTON_C, INPUT_PULLUP);

  display.begin();

  // Rotate to landscape (0 = standard orientation, wider than tall)
  prepareDisplay(); // Ensure pins are OUTPUT before display commands
  display.setRotation(0);
  display.clearBuffer();
  display.display();
  reclaimButtons(); // Reclaim for button polling during idle
  Serial.println("eInk display initialized and cleared.");

  // --- Network Setup ---
  Serial.print("Connecting to WiFi ");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int retry_count = 0;
  while (WiFi.status() != WL_CONNECTED && retry_count < 20) {
    delay(500);
    Serial.print(".");
    retry_count++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(" Failed to connect. Will continue trying in loop.");
  }

  // --- Time Setup ---
  Serial.println("Configuring NTP Timezones...");

  // Configure NTP with the exact TZ string so ESP8266 natively converts to
  // Eastern Time
  configTime(TZ_EASTERN, NTP_SERVER_1, NTP_SERVER_2);

  // We do not wait here anymore.
  // The ESP8266 will sink time automatically in the background once WiFi
  // connects.
  Serial.println("Setup complete. Entering main loop.");
}

void loop() {
  // 1. Maintain WiFi Connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected. Attempting to connect...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    // Give it 5 seconds to try
    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 10) {
      delay(500);
      Serial.print(".");
      retries++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWiFi Reconnected!");
    } else {
      Serial.println("\nWiFi connection failed. Will retry next loop.");
    }
  }
  time_t now = time(nullptr);
  struct tm *timeinfo = localtime(&now);

  // Only update the display if the minute has actually rolled over
  if (timeinfo->tm_min != lastMinute) {
    lastMinute = timeinfo->tm_min;

    // We only update if time is actually valid
    if (timeinfo->tm_year > 100) {
      // Pull weather every 15 minutes or if we don't have valid weather yet
      if (!currentWeather.valid ||
          abs(timeinfo->tm_min - lastWeatherMinute) >= 15 ||
          lastWeatherMinute == -1) {
        lastWeatherMinute = timeinfo->tm_min;
        fetchWeather();
      }

      Serial.println(
          "Minute rolled over and time is valid. Updating display...");
      updateDisplay(timeinfo);
    } else {
      Serial.println("Minute rolled over but time is NOT valid (year <= 100). "
                     "Skipping screen refresh.");
    }
  } else {
    // Optional: uncomment if you want to see every loop iteration
    // Serial.println("Minute has not rolled over yet.");
  }

  // --- Deep modem sleep & Button Polling ---
  int currentSecond = timeinfo->tm_sec;
  int secondsToWait = 60 - currentSecond;
  if (secondsToWait <= 0)
    secondsToWait = 60;

  unsigned long msToWait = (unsigned long)secondsToWait * 1000;
  unsigned long startSleep = millis();
  unsigned long lastHeartbeat = 0;

  bool forceUpdate = false;

  // SCAN BUTTON PINS (12, 13, 14)
  reclaimButtons();
  int scanPins[] = {12, 13, 14};

  Serial.printf("Sleeping %d seconds. POLLING BUTTONS (L:13, M:12, R:14)...\n",
                secondsToWait);

  while (millis() - startSleep < msToWait) {
    // Diagnostic Heartbeat every 1 second
    if (millis() - lastHeartbeat >= 1000) {
      lastHeartbeat = millis();
      Serial.printf("[SCAN] P12:%d P13:%d P14:%d | Left:%d ms\n",
                    digitalRead(12), digitalRead(13), digitalRead(14),
                    (int)(msToWait - (millis() - startSleep)));
    }

    // Check for button presses
    for (int p : scanPins) {
      if (digitalRead(p) == LOW) {
        delay(50); // debounce
        if (digitalRead(p) == LOW) {
          Serial.printf(">>> LOW DETECTED ON PIN %d <<<\n", p);

          if (p == 13 || p == 12) {
            // Left or Middle -> Woburn (Eastern)
            if (activeCity != "Woburn,MA,US") {
              Serial.printf(
                  ">>> Switching to Woburn (Eastern Time) via Pin %d <<<\n", p);
              activeCity = "Woburn,MA,US";
              displayCityName = "Woburn";
              configTime(TZ_EASTERN, NTP_SERVER_1, NTP_SERVER_2);
              forceUpdate = true;
            }
          } else if (p == 14) {
            // Right -> Cypress (Central)
            if (activeCity != "Cypress,TX,US") {
              Serial.println(
                  ">>> Switching to Cypress (Central Time) via Pin 14 <<<");
              activeCity = "Cypress,TX,US";
              displayCityName = "Cypress";
              configTime(TZ_CENTRAL, NTP_SERVER_1, NTP_SERVER_2);
              forceUpdate = true;
            }
          }
        }
      }
    }

    if (forceUpdate)
      break;
    delay(100);
  }

  if (forceUpdate) {
    Serial.println("Immediate Refresh Triggered by Button.");
    fetchWeather();
    now = time(nullptr);
    timeinfo = localtime(&now);
    updateDisplay(timeinfo);
  }
}

void fetchWeather() {
  if (WiFi.status() != WL_CONNECTED)
    return;

  Serial.println("Fetching current weather from OpenWeatherMap...");
  WiFiClientSecure client;
  client.setInsecure(); // OWM uses HTTPS. Skip validating cert for simple
                        // weather pull

  HTTPClient https;
  String url = String("https://api.openweathermap.org/data/2.5/weather?q=") +
               activeCity + "&appid=" + OWM_API_KEY + "&units=imperial";

  if (https.begin(client, url)) {
    int httpCode = https.GET();
    if (httpCode == HTTP_CODE_OK) {
      String payload = https.getString();

      StaticJsonDocument<1024> doc;
      DeserializationError error = deserializeJson(doc, payload);

      if (!error) {
        currentWeather.temp = doc["main"]["temp"];
        const char *icon = doc["weather"][0]["icon"];
        currentWeather.iconCode = String(icon);
        currentWeather.valid = true;
        Serial.printf("Weather updated: %.1fF, Icon: %s\n", currentWeather.temp,
                      icon);
      } else {
        Serial.println("JSON Parsing failed.");
      }
    } else {
      Serial.printf("Weather API call failed, error: %s\n",
                    https.errorToString(httpCode).c_str());
    }
    https.end();
  } else {
    Serial.println("Unable to connect to HTTPS endpoint.");
  }
}

void printCenteredInRegion(const char *text, int16_t y, int16_t regionX,
                           int16_t regionW) {
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, y, &x1, &y1, &w, &h);
  int16_t xCenter = regionX + (regionW - w) / 2 - x1;
  display.setCursor(xCenter, y);
  display.print(text);
}

void drawScaledBitmap(int16_t x, int16_t y, const unsigned char *bitmap,
                      int16_t w, int16_t h, uint16_t color, int16_t scale) {
  int16_t byteWidth = (w + 7) / 8;
  uint8_t byte = 0;
  for (int16_t j = 0; j < h; j++, y += scale) {
    for (int16_t i = 0; i < w; i++) {
      if (i & 7)
        byte <<= 1;
      else
        byte = pgm_read_byte(&bitmap[j * byteWidth + i / 8]);
      if (byte & 0x80)
        display.fillRect(x + i * scale, y, scale, scale, color);
    }
  }
}

void updateDisplay(struct tm *timeinfo) {
  prepareDisplay(); // Restore pin modes for display communication
  display.clearBuffer();

  // Explicitly draw a full white rectangle over the entire screen dimensions
  display.fillRect(0, 0, display.width(), display.height(), EPD_WHITE);

  // 1. Format the Time (12-Hour HH:MM format)
  int displayHour = timeinfo->tm_hour;
  if (displayHour == 0) {
    displayHour = 12; // Midnight is 12:00
  } else if (displayHour > 12) {
    displayHour -= 12; // Convert 13+ to 1+
  }

  char timeStr[10];
  sprintf(timeStr, "%d:%02d", displayHour, timeinfo->tm_min);

  // 2. Format the Date (Day Mon Date)
  char dateStr[20];
  sprintf(dateStr, "%s %s %d", days[timeinfo->tm_wday],
          months[timeinfo->tm_mon], timeinfo->tm_mday);

  Serial.printf("Drawing Screen -> %s | %s\n", timeStr, dateStr);

  display.setTextColor(EPD_BLACK);

  // Layout bounds (296x128 total)
  int leftWidth = 196;  // ~2/3 of screen
  int rightWidth = 100; // ~1/3 of screen

  // --- Top Location Labels (Aligned with Buttons) ---
  int16_t topY = 16; // Baseline for 9pt font

  // Woburn (Left Button / Pins 12&13)
  if (activeCity == "Woburn,MA,US") {
    display.setFont(&FreeSansBold9pt7b);
  } else {
    display.setFont(&FreeSans9pt7b);
  }
  display.setTextSize(1);
  display.setCursor(5, topY);
  display.print("Woburn");

  // Cypress (Right Button / Pin 14)
  if (activeCity == "Cypress,TX,US") {
    display.setFont(&FreeSansBold9pt7b);
  } else {
    display.setFont(&FreeSans9pt7b);
  }
  int16_t bx1, by1;
  uint16_t bw, bh;
  display.getTextBounds("Cypress", 0, topY, &bx1, &by1, &bw, &bh);
  // Put cypress at the far right edge
  display.setCursor(display.width() - bw - 5, topY);
  display.print("Cypress");

  // --- Left 2/3 (Time & Date) ---
  // Draw Time with 2x scaled 18pt font (~36pt, approx +25-50% larger than 24pt)
  // Shifted down slightly to make room for top labels
  display.setFont(&FreeSansBold18pt7b);
  display.setTextSize(2);
  printCenteredInRegion(timeStr, 75, 0, leftWidth);

  // Reset text size to 1x for remaining operations
  display.setTextSize(1);

  // Draw Date with 12pt font below time
  display.setFont(&FreeSans12pt7b);
  printCenteredInRegion(dateStr, 115, 0, leftWidth);

  // --- Right 1/3 (Weather & Temp) ---
  if (currentWeather.valid) {
    // Determine the icon to draw
    const unsigned char *iconPtr = icon_cloudy; // Default
    if (currentWeather.iconCode.indexOf("01") >= 0) {
      iconPtr = icon_sunny;
    } else if (currentWeather.iconCode.indexOf("02") >= 0 ||
               currentWeather.iconCode.indexOf("03") >= 0 ||
               currentWeather.iconCode.indexOf("04") >= 0 ||
               currentWeather.iconCode.indexOf("50") >= 0) {
      iconPtr = icon_cloudy;
    } else if (currentWeather.iconCode.indexOf("09") >= 0 ||
               currentWeather.iconCode.indexOf("10") >= 0) {
      iconPtr = icon_rain;
    } else if (currentWeather.iconCode.indexOf("11") >= 0) {
      iconPtr = icon_thunder;
    } else if (currentWeather.iconCode.indexOf("13") >= 0) {
      iconPtr = icon_snow;
    }

    // Draw Weather Icon manually scaled 200% (64x64 natively mapped to pixels)
    int iconSize = 64;
    int iconX = leftWidth + (rightWidth - iconSize) / 2;
    int iconY = 15;
    drawScaledBitmap(iconX, iconY, iconPtr, 32, 32, EPD_BLACK, 2);

    // Format temperature
    char tempValStr[10];
    sprintf(tempValStr, "%.0f", currentWeather.temp);

    // Draw Temp with massive 24pt font
    display.setFont(&FreeSansBold24pt7b);

    int16_t yBase = 115;
    int16_t x1, y1;
    uint16_t w1, h1;
    display.getTextBounds(tempValStr, 0, yBase, &x1, &y1, &w1, &h1);

    int16_t fx1, fy1;
    uint16_t fw, fh;
    display.getTextBounds("F", 0, yBase, &fx1, &fy1, &fw, &fh);

    int degreeGap = 16;
    int totalTextWidth = w1 + degreeGap + fw;
    int textStartX = leftWidth + (rightWidth - totalTextWidth) / 2;

    // Draw Temp Number
    display.setCursor(textStartX - x1, yBase);
    display.print(tempValStr);

    // Draw thick 24pt Degree Symbol
    int16_t rightEdge = textStartX + w1;
    int16_t degX = rightEdge + 8;
    int16_t degY = y1 + 6;
    display.drawCircle(degX, degY, 5, EPD_BLACK);
    display.drawCircle(degX, degY, 4, EPD_BLACK);
    display.drawCircle(degX, degY, 3, EPD_BLACK);

    // Draw F
    display.setCursor(rightEdge + degreeGap - fx1, yBase);
    display.print("F");
  }

  // --- Execute Screen Update ---
  display.display(true);
  reclaimButtons();
}
