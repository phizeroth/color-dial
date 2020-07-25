#include <FastLED.h>
#include <LiquidCrystal.h>
#include <neopixel.h>

FASTLED_USING_NAMESPACE;
SYSTEM_THREAD(ENABLED);

// ---------------------------------------
//  RGB & HSV Color Dialer w/ LCD Display
// ---------------------------------------

// Loop delay
const uint32_t DISPLAY_PERIOD_MS = 100;
uint32_t lastDisplayTime = millis();

// Firebase stuff
void publishData();
const char *PUBLISH_EVENT_NAME = "firepot";
const uint32_t PUBLISH_PERIOD_MS = 300;
uint32_t lastPublishTime = millis();

// Display stuff
const byte rs = D0, en = D1, lcdD4 = D2, lcdD5 = D3, lcdD6 = D4, lcdD7 = D5;
LiquidCrystal lcd(rs, en, lcdD4, lcdD5, lcdD6, lcdD7);

// Pot stuff
const byte pinPots[] = { A5, A4, A3 };
const int NUM_READINGS = 10;

// Button stuff
const byte BUTTON_PIN = D7;
int buttonState = HIGH;

int lastButtonState = HIGH;
const byte DEBOUNCE_PERIOD_MS = 50;
uint32_t lastDebounceTime = millis();

// LED stuff
const byte LED_PIN = D6;
const byte NUM_LEDS = 2;
Adafruit_NeoPixel neo(NUM_LEDS, LED_PIN);

// Mode toggles
byte colorMode = 0;   // 0: RGB, 1: HSV, 2: RAW
bool displayMode = 0; // 0: default, 1: alternate

// Custom characters
byte symbolPhi[] = { 0x04, 0x0E, 0x15, 0x15, 0x15, 0x0E, 0x04, 0x00 };
byte symbolDot[] = { 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00 };

class Pot {
    int pin;
    int readings[NUM_READINGS];
    int readIndex;
    int total;
    int average;

    public:
    Pot(byte potPin) {
        pin = potPin;
        for (int thisReading = 0; thisReading < NUM_READINGS; thisReading++) {
            readings[thisReading] = 0;
        }
        readIndex = 0;
        total = 0;
        average = 0;
    }

    byte smooth(byte mode = 0) {
        total = total - readings[readIndex];
        readings[readIndex] = analogRead(pin);
        total = total + readings[readIndex];
        readIndex = readIndex + 1;

        if (readIndex >= NUM_READINGS) {
            readIndex = 0;
        }

        average = total / NUM_READINGS;
        if (mode == 2) {
            return average;
        } else {
            return constrain(map(average, 255, 4000, 255, 0), 0, 255);
        }
    }

};

Pot pots[] = {
    Pot(pinPots[0]),    // r
    Pot(pinPots[1]),    // g
    Pot(pinPots[2])     // b
};

const byte NUM_POTS = 3;

int raw[] = {0, 0, 0};
int vals[] = {0, 0, 0};
int valsDisplayed[] = {0, 0, 0};
int valsPublished[] = {0, 0, 0};

void setup() {

    Serial.begin(9600);

    pinMode(BUTTON_PIN, INPUT_PULLUP);

    neo.begin();
    neo.clear();
    neo.setBrightness(255);
    neo.show();

    lcd.begin(16, 2);
    lcd.createChar(0, symbolPhi);
    lcd.createChar(1, symbolDot);

    Particle.function("setColorMode", setColorMode);    // accepts "rgb", "hsv", or "raw"
}

void loop() {

    for (byte i = 0; i < NUM_POTS; i++) {
        vals[i] = pots[i].smooth();
    }

    debounce();

    if (colorMode == 0) {                       // if in RGB mode
        showNeo(vals);

        if (displayMode == 0) {                 // if in default display (RGB)
            display(vals);
            publishData(valsDisplayed);
            
        } else {                                // if in alternate display (HSV)
            CRGB rgb(vals[0], vals[1], vals[2]);
            CHSV hsv(rgb2hsv_approximate(rgb));
            int hsvVals[3] = {hsv.hue, hsv.sat, hsv.val};
            display(hsvVals);
            publishData(vals);
        }

    } else if (colorMode == 1) {                // if in HSV mode
        CHSV hsv(vals[0], vals[1], vals[2]);
        CRGB rgb;
        hsv2rgb_rainbow(hsv, rgb);

        int rgbVals[3] = {rgb.r, rgb.g, rgb.b};
        showNeo(rgbVals);

        if (displayMode == 0) {                 // if in default display (HSV)
            display(vals);
            publishData(rgbVals);
        } else {                                // if in alternate display (RGB)
            display(rgbVals);
            publishData(rgbVals);
        }

    } else if (colorMode == 2) {                // if in raw mode

        for (byte i = 0; i < NUM_POTS; i++) {
            raw[i] = pots[i].smooth(2);
        }

        showNeo(vals);
        display(raw);
        publishData(vals);

        Serial.printlnf("%d | %d | %d", raw[0], raw[1], raw[2]);
    }

}

void display(int *arr) {
    if (millis() - lastDisplayTime > DISPLAY_PERIOD_MS) {
        if (valChange(valsDisplayed, arr)) {
            Serial.println(valsDisplayed[2]);
            String headingsHSV[3] = {"H", "  S", "  V"};
            String headingsRGB[3] = {"R", "  G", "  B"};
            String headingsRAW[3] = {"0", "  1", "  2"};
            lcd.clear();
            lcd.setCursor(0, 0);
            if (colorMode == 2) {
                lcdPrintHeader(headingsRAW);
            } else if ((colorMode + displayMode) % 2) {
                lcdPrintHeader(headingsHSV);
            } else {
                lcdPrintHeader(headingsRGB);
            }
            lcd.setCursor(0, 1);  lcd.print(arr[0]);
            lcd.setCursor(5, 1);  lcd.print(arr[1]);
            lcd.setCursor(10, 1); lcd.print(arr[2]);
            lcd.setCursor(15, 0); lcd.write(0);

            for (byte i = 0; i < NUM_POTS; i++) {
                valsDisplayed[i] = arr[i];
            }
        }
    lastDisplayTime = millis();
    }
}

void lcdPrintHeader(String headings[]) {
    for (byte i = 0; i < 3; i++) {
        lcd.print(headings[i]);
        lcd.write(1);
        lcd.write(1);
    }
}

int8_t setColorMode(String s) {
    if (s == "rgb") {
        colorMode = 0;
    } else if (s == "hsv") {
        colorMode = 1;
    } else if (s == "raw") {
        colorMode = 2;
    } else {
        return -1;
    }
    return colorMode;
}

void showNeo(int *vals) {
    for (byte i = 0; i < NUM_LEDS; i++) {
        neo.setPixelColor(i, vals[0], vals[1], vals[2]);
        neo.show();
    }
}

/*void button() {
    uint32_t timeElapsed;
    buttonState = digitalRead(BUTTON_PIN);
    if (buttonState == LOW) {
        displayMode = 1;
        timeElapsed = millis() - lastDebounceTime;
    } else {
        displayMode = 0;
        if (timeElapsed < 450) {
            colorMode = !colorMode;
        }
        lastDebounceTime = millis();
    }
}*/

void debounce() {
  int buttonReading = digitalRead(BUTTON_PIN);

  if (buttonReading != lastButtonState) {
    lastDebounceTime = millis();
  }

  uint32_t timeElapsed = millis() - lastDebounceTime;

  if (timeElapsed > DEBOUNCE_PERIOD_MS) {
    if (buttonReading != buttonState) {
      buttonState = buttonReading;
      if (buttonState == LOW) {
          displayMode = 1;
      } else {
          displayMode = 0;
      }
    }
  }

  lastButtonState = buttonReading;
}

void publishData(int *vals) {

    if (millis() - lastPublishTime > PUBLISH_PERIOD_MS) {
        if (valChange(valsPublished, vals)) {
            char buf[256];
            snprintf(buf, sizeof(buf), "{\"r\":%d,\"g\":%d,\"b\":%d}", vals[0], vals[1], vals[2]);
            Particle.publish(PUBLISH_EVENT_NAME, buf, PRIVATE);
            
            for (byte i = 0; i < NUM_POTS; i++) {
                valsPublished[i] = vals[i];
            }
        }
        lastPublishTime = millis();
    }
}

boolean valChange(int *arr1, int *arr2) {
    for (byte i = 0; i < NUM_POTS; i++) {
        if (abs(arr2[i] - arr1[i]) >= 2) {
            // Serial.printf("%d | %d  CHANGE\n", arr1[1], arr2[1]);
            return true;
        }
    }
    return false;
}