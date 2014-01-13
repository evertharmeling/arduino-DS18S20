#include <OneWire.h>
#include <Wire.h>
#include <Adafruit_MCP23017.h>
#include <Adafruit_RGBLCDShield.h>

#define SENSOR_PIN  2
#define SENSOR_HLT "10bab04c280b7"
#define SENSOR_MLT "104bbc4d2805c"

#define RED 0x1
#define YELLOW 0x3
#define GREEN 0x2
#define TEAL 0x6
#define BLUE 0x4
#define VIOLET 0x5
#define WHITE 0x7

// on pin 10 (a 2.2K resistor is necessary)
OneWire ds(SENSOR_PIN);
Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();

String sensor;
String mode;

void setup() {
    // initialize lcd
    lcd.begin(16, 2);
    
    lcd.print("Welkom in");
    lcd.setCursor(0, 1);
    lcd.print("Brouwzaal 1");
    delay(1000);
    lcd.clear();
    
    // show menu options, scrolled
}

void loop() {
    uint8_t buttons = lcd.readButtons();

    if (buttons) {
        if (buttons & BUTTON_UP) {
            mode = "time";
        }
        if (buttons & BUTTON_LEFT) {
            mode = "temp";
        }
        
        lcd.clear();
    }
    
    if (mode == "time") {
        printTime();
    } else {
        printTemperature();
    }
}

void printTime() {
    int s = millis() / 1000;
    int m = s / 60;
    int h = m / 60;
    lcd.setCursor(8,1);
   
    if (h < 10) {       // Add a zero, if necessary
        lcd.print(0);
    }
   
    lcd.print(h);
    lcd.print(":");     // And print the colon
   
    if (m < 10) {       // Add a zero, if necessary, as above
        lcd.print(0);
    }
   
    lcd.print(m);
    lcd.print(":");     // And print the colon
   
    s -= m * 60;
    if (s < 10) {       // Add a zero, if necessary, as above
        lcd.print(0);
    }
   
    lcd.print(s);
    delay(950);
}

float readTemperature() {
    byte i;
    byte data[12];
    byte addr[8];
    sensor = "";
  
    if (!ds.search(addr)) {
        ds.reset_search();
        delay(250);
        return 0;
    }

    if (OneWire::crc8(addr, 7) != addr[7]) {
        lcd.setCursor(0, 0);
        lcd.setBacklight(RED);
        lcd.println("CRC is not valid!");
        return 0;
    }

    ds.reset();
    ds.select(addr);
    for ( i = 0; i < 8; i++) {
        sensor += String(addr[i], HEX);
    }

    ds.write(0x44, 1);  
    delay(800);
  
    ds.reset();
    ds.select(addr);    
    ds.write(0xBE);

    for ( i = 0; i < 9; i++) {
        data[i] = ds.read();
    }

    int16_t raw = (data[1] << 8) | data[0];
    raw = raw << 3;
    if (data[7] == 0x10) {
        raw = (raw & 0xFFF0) + 12 - data[6];
    } 
    
    return (float) raw / 16.0;
}

void printTemperature() {
    float temp = readTemperature();
  
    if (temp) {
        if (sensor == SENSOR_HLT) {
            lcd.setCursor(0, 0);
            lcd.print("HLT: ");
        } else if (sensor == SENSOR_MLT) {
            lcd.setCursor(0, 1);
            lcd.print("MLT: ");
        } 
        
        lcd.print(temp);
        lcd.print(" ");
        lcd.print((char) 223);
        lcd.print("C");
        
        if (temp >= 100) {
            lcd.print(" ");
        } else {
            lcd.print("  "); 
        }
        
        lcd.cursor();
    }
}
