// @todo first maisch step may begin when the temp of the MLT is higher than first maisch step
// @todo keep HLT on the last maisch temp always +/- 5 degrees warmer

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

// Define modes
#define MODE_TIME     "time"
#define MODE_HLT_MLT  "hlt_mlt"
#define MODE_BLT_EXT  "blt_ext"

#define DISPLAY_SERIAL "serial"
#define DISPLAY_LCD    "lcd"

// on pin 10 (a 2.2K resistor is necessary)
OneWire ds(PIN_SENSOR_TEMPERATURE);
Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();

// Initiate variables
String sensor;
String mode;
elapsedMillis loopTime;
elapsedMillis stepTime;

unsigned int interval  = 2000;
float tempHLT          = 0;
float tempMLT          = 0;
float tempBLT          = 0;
float tempEXT          = 0;
boolean heatUpHLT      = true;
boolean heatUpMLT      = true;
float hysterese        = 0.5;

boolean firstMaischPeriod  = false;
int currentMaischStep      = 0;
int maischTimes[]          = { 10, 45, 30, 1 };
int maischTemperatures[]   = { 27, 62, 73, 78 };

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
    
    mode = MODE_HLT_MLT;
}

/**
 *  Main loop
 */
void loop() {
    uint8_t buttons = lcd.readButtons();

    // handle buttons
    if (buttons) {
        if (buttons & BUTTON_UP) {
            mode = MODE_TIME;
        }
        if (buttons & BUTTON_LEFT) {
            mode = MODE_HLT_MLT;
        }
        if (buttons & BUTTON_RIGHT) {
            mode = MODE_BLT_EXT;
        }
        
        lcd.clear();
    }
    
    float temp = readTemperature();
    prepareRelais(temp);
    
    // handle button change
    if (mode == MODE_TIME) {
        lcdElapsedTime();
    } else if (mode == MODE_HLT_MLT | mode == MODE_BLT_EXT) {
        printTemperature(temp, mode);        
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
      
        // ensure we got the temperature of the HLT and MLT sensor probes, so we can calculate a diff
        if (tempHLT != 0 && tempMLT != 0 && loopTime > interval) {
          
            int currentMaischTime = maischTimes[currentMaischStep];
            int elapsedMinutesCurrentStep = stepTime / 1000 / 60;
            
            // initial check, to start first period when the temperature is reached
            if (tempMLT >= maischTemperatures[currentMaischStep] && !firstMaischPeriod) {
                stepTime = 0;
                firstMaischPeriod = true;
            }
            
            // when we hit the temperature start counting the current period
            if (tempMLT >= maischTemperatures[currentMaischStep] && elapsedMinutesCurrentStep == currentMaischTime) {
                currentMaischStep++;
                stepTime = 0;
            }
            
            Serial.println("---------------------------------");
            Serial.print("Current Maisch Time: ");
            Serial.println(currentMaischTime);
            Serial.print("Current Maisch Step: ");
            Serial.println(currentMaischStep);
            Serial.print("Time elapsed: ");
            serialElapsedTime();
            Serial.println("");
            Serial.print("Time elapsed current maisch step: ");
            serialStepTime();
            Serial.println("");
          
            // set temperature for the maisch steps!
            handleRelais(tempHLT, tempMLT);
            // reset the counter to 0 so the counting starts over...
	    loopTime = 0;
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
 *  @param float temp   The temperature in degrees Celsius
 *  @param String mode  The mode to print the right temperature value
 */
void printTemperature(float temp, String mode) {
    if (sensor && temp) {
        if (mode == MODE_HLT_MLT) {
            if (sensor == SENSOR_HLT) {
                lcd.setCursor(0, 0);
                lcd.print("HLT: ");
            } else if (sensor == SENSOR_MLT) {
                lcd.setCursor(0, 1);
                lcd.print("MLT: ");
            } 
        } else if (mode == MODE_BLT_EXT) {
            if (sensor == SENSOR_BLT) {
                lcd.setCursor(0, 0);
                lcd.print("BLT: ");
            } else if (sensor == SENSOR_EXT) {
                lcd.setCursor(0, 1);
                lcd.print("EXT: ");
            }
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

/** 
 *  Handles switching the relais for heating up the HLT (by heaters) and MLT (by pumping wort through HLT spiral)
 *  The temperature of the HLT will be increased with 30 degrees, so heating up the MLT is quicker. The only catch
 *  is to not heat up the HLT too much at the last maisch step (the water will be used to rinse the MLT)
 *
 *  @param float setTemp  The temperature to reach by the MLT
 *  @param float tempHLT  Current temperature of the HLT sensor probe
 *  @param float tempMLT  Current temperature of the MLT sensor probe
 */
void handleRelais(float tempHLT, float tempMLT) { 
  
    float setTemp = maischTemperatures[currentMaischStep];
  
    float setTempHLT = setTemp + 30;
    float setTempDiff = tempMLT - setTemp;

    Serial.print("Current set temp HLT: ");
    Serial.println(setTempHLT + hysterese);
    Serial.print("Current temp HLT: ");
    Serial.println(tempHLT);
    Serial.print("Current set temp MLT: ");
    Serial.println(setTemp + hysterese);
    Serial.print("Current temp MLT: ");
    Serial.println(tempMLT);
    Serial.print("Temp diff: ");
    Serial.println(setTempDiff);
    Serial.println("=================================");
     
    // Heat up the HLT
    if (tempHLT > maischTemperatures[sizeof(maischTemperatures) - 1]) {
        heatUpHLT = false;
    } else {
        heatUpHLT = handleHysterese(tempHLT, setTempHLT, heatUpHLT);
        switchRelais(PIN_RELAIS_ONE,   heatUpHLT);
        switchRelais(PIN_RELAIS_TWO,   heatUpHLT);
        switchRelais(PIN_RELAIS_THREE, heatUpHLT);
    }
    
    // According to the setTemp heat up the MLT by turning on the pump
    heatUpMLT = handleHysterese(tempMLT, setTemp, heatUpMLT);
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
 *  @param boolean heating  Boolean to hold current heating status
 */
boolean handleHysterese(float temp, float setTemp, boolean heating)
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
 *  Prints the elapsed time to Serial
 */
void printTime(float time, String type) {
    int s = time / 1000;
    int m = s / 60;
    int h = m / 60;
    
    m -= h * 60;
    s -= m * 60;
    
    if (type == DISPLAY_LCD) {
        lcd.setCursor(8,1);
    }
   
    if (h < 10) {       // Add a zero, if necessary
        if (type == DISPLAY_SERIAL) {
            Serial.print("0");
        } else if (type == DISPLAY_LCD) {
            lcd.print(0);
        }   
    }
   
    if (type == DISPLAY_SERIAL) {
        Serial.print(h);
        Serial.print(":");     // And print the colon
    } else if (type == DISPLAY_LCD) {
        lcd.print(h);
        lcd.print(":");     // And print the colon
    }
   
    if (m < 10) {       // Add a zero, if necessary, as above
        if (type == DISPLAY_SERIAL) {
            Serial.print("0");
        } else if (type == DISPLAY_LCD) {
            lcd.print(0);
        }
    }
   
    if (type == DISPLAY_SERIAL) {
        Serial.print(m);
        Serial.print(":");     // And print the colon
    } else if (type == DISPLAY_LCD) {
        lcd.print(m);
        lcd.print(":");     // And print the colon
    }
   
    if (s < 10) {       // Add a zero, if necessary, as above
        if (type == DISPLAY_SERIAL) {
            Serial.print("0");
        } else if (type == DISPLAY_LCD) {
            lcd.print(0);
        }
    }
   
    if (type == DISPLAY_SERIAL) {
        Serial.print(s);
    } else if (type == DISPLAY_LCD) {
        lcd.print(s);
    }
}

/**
 *  Prints the elapsed time to LCD
 */
void lcdElapsedTime() {
    printTime(millis(), DISPLAY_LCD);
}

/**
 *  Prints the elapsed time to Serial
 */
void serialElapsedTime() {
    printTime(millis(), DISPLAY_SERIAL);
}

/**
 *  Prints the step time to Serial
 */
void serialStepTime() {
    printTime(stepTime, DISPLAY_SERIAL);
}
