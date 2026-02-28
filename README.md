# ESP8266 Arduino eInk Clock Setup

This guide walks you through configuring your **Adafruit Feather HUZZAH with ESP8266 (Product #3213)** and the **Adafruit 2.13" Monochrome eInk FeatherWing (SSD1680)** using the Arduino IDE.

---

## Part 1: Arduino IDE & ESP8266 Core Setup

Since the ESP8266 chip requires compiling C++ rather than copying Python files, we must set up the Arduino IDE.

1. **Download the Arduino IDE**: 
   - Get the latest version from [arduino.cc/en/software](https://www.arduino.cc/en/software).

2. **Install the ESP8266 Board Package**:
   - Open Arduino IDE -> `Preferences` (or `Settings` on macOS).
   - In "Additional Boards Manager URLs", paste:
     `http://arduino.esp8266.com/stable/package_esp8266com_index.json`
   - Go to `Tools` -> `Board` -> `Boards Manager...`
   - Search for `esp8266` by ESP8266 Community and click **Install**.

3. **Select Your Board**:
   - Go to `Tools` -> `Board` -> `ESP8266 Boards` and select **Adafruit Feather HUZZAH ESP8266**.
   - Set the `Upload Speed` to `115200` to ensure stable initial flashing (you can try faster later).
   - Plug in your ESP8266 and select the appropriate serial port in `Tools` -> `Port`.

---

## Part 2: Wiring & Hardware Assembly

Because both the ESP8266 board and the eInk display are in the "Feather" format, the easiest way to wire them is to simply solder stacking headers and plug them directly into each other! 

However, if you are wiring them side-by-side on a breadboard or with jumper wires, here is the exact pinout mapping used by default in `EInkClock.ino`:

| Feather ESP8266 Pin | eInk FeatherWing Pin | Function |
| :--- | :--- | :--- |
| **3V** | **3V** | Power |
| **GND** | **GND** | Ground |
| **SCK** | **SCK** | SPI Clock |
| **MOSI** | **MOSI** | SPI Data Out |
| **MISO** | **MISO** | SPI Data In (SRAM reading) |
| **Pin 15 (RX)** | **CS (EPD_CS)** | eInk Chip Select |
| **Pin 16 (TX)** | **DC (EPD_DC)** | Data/Command |
| **Pin 2** | **SRCS (SRAM_CS)** | SRAM Chip Select |
| **Pin 0** | **RST (EPD_RESET)** | Reset |
| *(Not connected)* | **BUSY** | Busy Signal (Optional) |

*Note: The Busy pin is explicitly set to `-1` in the code, meaning the library will use fixed internal delays instead of waiting on the physical hardware busy pin, freeing up a GPIO pin for you.*

---

## Part 3: Install Required Libraries

The code relies on Adafruit's standard graphics and eInk display libraries.

1. Go to `Sketch` -> `Include Library` -> `Manage Libraries...`
2. Search for and **Install** the following exact libraries:
   - `Adafruit EPD` (When prompted to install dependencies like Adafruit BusIO or Adafruit GFX, select **Install All**).
   - `Adafruit GFX Library` (Should be installed via above step, but verify).

*Note: You do not need an external NTP library; the modern ESP8266 Arduino core has fantastic robust NTP and timezone support natively built-in.*

---

## Part 3: Configure and Upload the Code

1. **Open the Project:**
   - Double click the `EInkClock/EInkClock.ino` file to open it in the Arduino IDE.

2. **Add Your Wi-Fi Credentials:**
   - Near the top of the `EInkClock.ino` file, find the lines defining `WIFI_SSID` and `WIFI_PASSWORD`. 
   - Change `"your_wifi_ssid_here"` and `"your_wifi_password_here"` to your home network details.

3. **Upload the Code!**
   - Click the right-arrow **Upload** button in the top-left corner of the IDE.
   - Wait for the sketch to compile and write to the ESP8266.

Once it says "Done uploading", the ESP8266 will restart. Open the `Serial Monitor` (magnifying glass top right) and set the baud rate to `115200` to watch the device connect to Wi-Fi, fetch exact UTC time, convert it to Eastern Time (automatically calculating if DST applies today!), and begin drawing to your eInk display.
