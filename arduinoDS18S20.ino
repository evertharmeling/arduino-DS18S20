#include <OneWire.h>
#include <Wire.h>
#include <Adafruit_MCP23017.h>
#include <Adafruit_RGBLCDShield.h>
#include <elapsedMillis.h>

// Declaration pins
#define PIN_SENSOR_TEMPERATURE  2
#define PIN_RELAIS_PUMP         5
#define PIN_RELAIS_ONE          6
#define PIN_RELAIS_TWO          7
#define PIN_RELAIS_THREE        8
#define PIN_RELAIS_FOUR         9

// Temperature probe addresses
#define SENSOR_HLT "10bab04c280b7"
#define SENSOR_MLT "104bbc4d2805c"
#define SENSOR_BLT "103d1325280a6"
#define SENSOR_EXT "1097e4242804d"

// Define LCD colors
#define RED     0x1
#define GREEN   0x2
#define YELLOW  0x3
#define BLUE    0x4
#define VIOLET  0x5
#define TEAL    0x6
#define WHITE   0x7

// on pin 10 (a 2.2K resistor is necessary)
OneWire ds(PIN_SENSOR_TEMPERATURE);
Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();

String sensor;
String mode;
elapsedMillis elapsedTime;

unsigned int interval  = 1000;
float tempHLT          = 0;
float tempMLT          = 0;
float tempBLT          = 0;
float tempEXT          = 0;
float lastTemp         = 78;
float maischTemp       = 25;
boolean heatUpHLT      = true;
boolean heatUpMLT      = true;

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
    
    Serial.println("Brouwen gestart");
    
    // initialize Relais
    pinMode(PIN_RELAIS_ONE,   OUTPUT);
    pinMode(PIN_RELAIS_TWO,   OUTPUT);
    pinMode(PIN_RELAIS_THREE, OUTPUT);
    pinMode(PIN_RELAIS_PUMP,  OUTPUT);
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
    prepareRelais(temp);
    
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
        } else if (sensor == SENSOR_BLT) {
            tempBLT = temp;
        } else if (sensor == SENSOR_EXT) {
            tempEXT = temp;
        }
      
        // ensure we got the temperature of both sensor probes, so we can calculate a diff
        if (tempHLT != 0 && tempMLT != 0 && elapsedTime > interval) {
            // set temperature for the maisch steps!
            handleRelais(maischTemp, tempHLT, tempMLT);
            // reset the counter to 0 so the counting starts over...
	    elapsedTime = 0;
        }
    }
}

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
 *  Handles switching the relais for heating up the HLT (by heaters) and MLT (by pumping wort through HLT spiral)
 *  The temperature of the HLT will be increased with 20 degrees, so heating up the MLT is quicker. The only catch
 *  is to not heat up the HLT too much at the last maisch step (the water will be used to rinse the MLT)
 *
 *  @param float setTemp  The temperature to reach by the MLT
 *  @param float tempHLT  Current temperature of the HLT sensor probe
 *  @param float tempMLT  Current temperature of the MLT sensor probe
 */
void handleRelais(float setTemp, float tempHLT, float tempMLT) { 
    float setTempHLT = setTemp + 20;
    float setTempDiff = setTemp - tempMLT;
    float hysterese = 0.5;

    Serial.print("Current set temp HLT: ");
    Serial.print(lastTemp);
    Serial.print(" or ");
    Serial.println(setTempHLT + hysterese);
    Serial.print("Current temp HLT: ");
    Serial.println(tempHLT);
    Serial.print("Current set temp MLT: ");
    Serial.println(setTemp + hysterese);
    Serial.print("Current temp MLT: ");
    Serial.println(tempMLT);
    Serial.print("Temp diff: ");
    Serial.println(setTempDiff);
     
    // Heat up the HLT
    if (tempHLT > lastTemp) {
        heatUpHLT = false;
    } else {
        heatUpHLT = handleHysterese(tempHLT, setTempHLT, heatUpHLT, hysterese);
        switchRelais(PIN_RELAIS_ONE,   heatUpHLT);
        switchRelais(PIN_RELAIS_TWO,   heatUpHLT);
        switchRelais(PIN_RELAIS_THREE, heatUpHLT);
    }
    
    // According to the setTemp heat up the MLT by turning on the pump
    heatUpMLT = handleHysterese(tempMLT, setTemp, heatUpMLT, hysterese);
    switchRelais(PIN_RELAIS_PUMP, heatUpMLT);
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
 *  Uniform way of handling the hysterese check, to prevent switching the relais too fast on/off
 *
 *  @param float temp       Current temperature value
 *  @param float setTemp    Desired temperature
 *  @param float hysterese  The amount added / substracted to the setTemp
 */
boolean handleHysterese(float temp, float setTemp, boolean heating, float hysterese)
{
    float top = setTemp + hysterese;
    float bottom = setTemp - hysterese;

    if (temp >= bottom && temp <= top) {
        // hysterese zone, don't change the heating variable
        Serial.println("Hysteresing");
    } else if (temp < bottom) {
        Serial.println("We are heating");
        heating = true;
    } else if (temp > top) {
        Serial.println("Temperature reached wait...");
        heating = false;
    }

    return heating;
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
