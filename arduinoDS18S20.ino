#include <OneWire.h>
#include <Wire.h>
#include <Adafruit_MCP23017.h>
#include <Adafruit_RGBLCDShield.h>
#include <elapsedMillis.h>

#define PIN_SENSOR       2
#define PIN_RELAIS_ONE   6
#define PIN_RELAIS_TWO   7
#define PIN_RELAIS_THREE 8
#define PIN_RELAIS_FOUR  9

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
OneWire ds(PIN_SENSOR);
Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();

String sensor;
String mode;
float tempHLT = 0;
float tempMLT = 0;
elapsedMillis elapsedTime;
boolean heatUp = true;
unsigned int interval = 1000;

/**
 *  Setup
 */
void setup() {
    Serial.begin(9600);
  
    // initialize lcd
    lcd.begin(16, 2);
    
    lcd.print("Welkom in");
    lcd.setCursor(0, 1);
    lcd.print("Brouwzaal 1");
    delay(1000);
    lcd.clear();
    
    // initialize Relais
    pinMode(PIN_RELAIS_ONE, OUTPUT);
    pinMode(PIN_RELAIS_TWO, OUTPUT);
    pinMode(PIN_RELAIS_THREE, OUTPUT);
}

/**
 *  Main loop
 */
void loop() {
    uint8_t buttons = lcd.readButtons();

    // handle buttons
    if (buttons) {
        if (buttons & BUTTON_UP) {
            mode = "time";
        }
        if (buttons & BUTTON_LEFT) {
            mode = "temp";
        }
        
        lcd.clear();
    }
    
    float temp = readTemperature();
    if (elapsedTime > interval) {

        prepareRelais(temp);

        // reset the counter to 0 so the counting starts over...
	elapsedTime = 0;
    }
    
    // handle button change
    if (mode == "time") {
        printTime();
    } else {
        printTemperature(temp);
    }
}

/*
 * ========================= FUNCTIONS =========================
 */

/**
 *  Reads the temperature of the sensors, and stores these in `temp` variable and `sensor` variable
 */
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
    //delay(800);
  
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

/**
 *  Prints the temperature to the LCD
 *
 *  @param float temp  The temperature in degrees Celsius
 */
void printTemperature(float temp) {
    if (sensor && temp) {
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
    } else if (elapsedTime > interval) {
        lcd.setCursor(0, 0);
        lcd.print("Geen sensoren");
        lcd.setCursor(0, 1);
        lcd.print("aangesloten... ");
        lcd.cursor();
    }
}

/**
 *  Handles the switching of the relais
 *
 *  @param float temp  The temperature in degrees Celsius
 */
void prepareRelais(float temp) {
    if (sensor && temp) {
        if (sensor == SENSOR_HLT) {
            tempHLT = temp;
        } else if (sensor == SENSOR_MLT) {
            tempMLT = temp;
        }
      
        // ensure we got the temperature of both sensor probes, so we can calculate a diff
        if (sensor == SENSOR_HLT && tempHLT != 0 && tempMLT != 0) {
            handleRelais(60, tempHLT, tempMLT);
        }
    } else {
        Serial.println("Geen sensoren aangesloten...");
    }
}

/** 
 *  Handles switching the relais, acting on temperature and temperature diff between the desired temperature and MLT temperature
 *
 *  @param float setTemp  The temperature to reach by the MLT
 *  @param float tempHLT  Current temperature of the HLT sensor probe
 *  @param float tempMLT  Current temperature of the MLT sensor probe
 */
void handleRelais(float setTemp, float tempHLT, float tempMLT) {
    float setTempDiff = setTemp - tempMLT;
    float hysterese = 1.0;
    
    Serial.print("Current set temp: ");
    Serial.println(setTemp);
    Serial.print("Current temp HLT: ");
    Serial.println(tempHLT);
    Serial.print("Current temp MLT: ");
    Serial.println(tempMLT);
    Serial.print("Temp diff: ");
    Serial.println(setTempDiff);
    
    if (tempMLT < (setTemp + hysterese)) {
        heatUp = true;
    } else if (tempMLT > (setTemp - hysterese)) {
        heatUp = false;
    }
  
    if (heatUp) {
        if ((setTempDiff + hysterese) > 0.5) {
            switchRelais(PIN_RELAIS_ONE, true);
        } else if ((setTempDiff - hysterese) < 0.5) {
            switchRelais(PIN_RELAIS_ONE, false);
        }
        if ((setTempDiff + hysterese) > 0) {
            switchRelais(PIN_RELAIS_TWO, true);
        } else if ((setTempDiff - hysterese) < 0) {
            switchRelais(PIN_RELAIS_TWO, false);
        }
        
        switchRelais(PIN_RELAIS_THREE, true);
    } else {
        switchRelais(PIN_RELAIS_ONE, false);
        switchRelais(PIN_RELAIS_TWO, false);
        switchRelais(PIN_RELAIS_THREE, false);
    }
}

/**
 *  Switch relais on or off
 *
 *  @param int relais  The pin of the relais
 *  @param boolean on  true / false
 */
void switchRelais(int relais, boolean on) {
    if (on) {
        Serial.println("Set relais " + String(relais) + " ON");
        digitalWrite(relais, LOW);
    } else {
        Serial.println("Set relais " + String(relais) + " OFF");
        digitalWrite(relais, HIGH);
    }
}

/**
 *  Prints the elapsed time
 */
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
   
    m -= h * 60;
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
}
