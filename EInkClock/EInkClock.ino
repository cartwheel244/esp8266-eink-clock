#include "secrets.h"
#include <Adafruit_EPD.h>
#include <Adafruit_GFX.h>
#include <ESP8266WiFi.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <time.h>

// =========================================================================
// CONFIGURATION: WI-FI & TIME
// =========================================================================
const char *WIFI_SSID = SECRET_WIFI_SSID;
const char *WIFI_PASSWORD = SECRET_WIFI_PASSWORD;

// ESP8266 Core Timezone String
// "EST5EDT,M3.2.0,M11.1.0" defines:
// EST is UTC-5. EDT is UTC-4.
// DST starts the 2nd Sunday (2) of March (M3) at 02:00 (0).
// DST ends the 1st Sunday (1) of November (M11) at 02:00 (0).
// The ESP8266 internal C library handles this math perfectly in the background.
const char *TZ_INFO = "EST5EDT,M3.2.0,M11.1.0";

// NTP Servers
const char *NTP_SERVER_1 = "pool.ntp.org";
const char *NTP_SERVER_2 = "time.nist.gov";

// =========================================================================
// CONFIGURATION: eINK DISPLAY (Feather Wing 2.13" Monochrome)
// =========================================================================
#define EPD_CS 0
#define EPD_DC 15
#define SRAM_CS 16
#define EPD_RESET -1
#define EPD_BUSY                                                               \
  -1 // Can set to -1 on FeatherWings since we don't always use it

// The 2.13" Monochrome FeatherWing uses the SSD1680 driver
Adafruit_SSD1680 display(250, 122, EPD_DC, EPD_RESET, EPD_CS, SRAM_CS,
                         EPD_BUSY);

// Globals
int lastMinute = -1;
const char *days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n--- ESP8266 eInk Clock ---");

  // --- Display Setup ---
  Serial.println("Initializing eInk display...");
  display.begin();
  // Rotate to landscape (3 = 270 degrees / wider than tall with USB port on the
  // right)
  display.setRotation(3);
  display.clearBuffer();
  display.display();
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

  // Set the timezone parsing rule
  setenv("TZ", TZ_INFO, 1);
  tzset();

  // Configure NTP (Note: timezone offset args are 0 because TZ env handles it
  // dynamically)
  configTime(0, 0, NTP_SERVER_1, NTP_SERVER_2);

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

  // Deep modem sleep to conserve power, waking precisely at the top of the next
  // minute
  int currentSecond = timeinfo->tm_sec;
  int secondsToWait = 60 - currentSecond;

  Serial.printf("Sleeping %d seconds until next minute rollover...\n",
                secondsToWait);

  // A simple tight delay saves significant CPU power on the ESP8266.
  // Delay takes milliseconds.
  delay(secondsToWait * 1000);
}

void updateDisplay(struct tm *timeinfo) {
  // Clear the internal buffer
  display.clearBuffer();

  // Explicitly draw a full white rectangle over the entire screen dimensions
  // This physically forces the Adafruit GFX buffer to overwrite the entire
  // 250x122 grid and prevents the "20% unrefreshed" trailing artifact issue
  // caused by driver padding.
  display.fillRect(0, 0, display.width(), display.height(), EPD_WHITE);

  // 1. Format the Time (12-Hour HH:MM format)
  int displayHour = timeinfo->tm_hour;
  if (displayHour == 0) {
    displayHour = 12; // Midnight is 12:00
  } else if (displayHour > 12) {
    displayHour -= 12; // Convert 13+ to 1+
  }

  char timeStr[10];
  // Using %d instead of %02d for the hour so "01:00" becomes "1:00" natively
  sprintf(timeStr, "%d:%02d", displayHour, timeinfo->tm_min);

  // 2. Format the Date (Day Mon Date)
  char dateStr[20];
  sprintf(dateStr, "%s %s %d", days[timeinfo->tm_wday],
          months[timeinfo->tm_mon], timeinfo->tm_mday);

  Serial.printf("Drawing Screen -> %s | %s\n", timeStr, dateStr);

  // --- Draw Text ---
  // Note: Adafruit GFX prints custom fonts from their baseline (bottom-left)
  // not top-left like default fonts. So the Y coord must be > 0.

  // Draw Time
  display.setFont(&FreeSansBold18pt7b);
  display.setTextColor(EPD_BLACK);
  // Center roughly horizontally and position in upper half
  display.setCursor(65, 55);
  display.print(timeStr);

  // Draw Date
  display.setFont(&FreeSans9pt7b);
  display.setCursor(65, 95);
  display.print(dateStr);

  // --- Execute Screen Update ---
  // Partial refresh: Setting this to `true` on supported drivers uses less
  // power and dramatically reduces exactly the aggressive screen flashing eInks
  // do.
  display.display(true);
}
