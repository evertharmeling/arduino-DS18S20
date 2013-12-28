// DS18S20 temperature sensor
#include <OneWire.h>
#include <Time.h>

#define SENSOR_PIN  2 

// on pin 10 (a 2.2K resistor is necessary)
OneWire ds(SENSOR_PIN);

void setup() {
    Serial.begin(9600);
    setTime(0, 0, 0, 1, 1, 14);
}

void loop() {
    byte i;
    byte p = 0;
    byte data[12];
    byte addr[8];
    float temp;
  
    if ( !ds.search(addr)) {
        ds.reset_search();
        delay(250);
        return;
    }

    if (OneWire::crc8(addr, 7) != addr[7]) {
        Serial.println("CRC is not valid!");
        return;
    }

    printTime();
    Serial.print(" : ");

    ds.reset();
    ds.select(addr);
    for ( i = 0; i < 8; i++) {
        Serial.print(addr[i], HEX);
        Serial.print(" ");
    }
    Serial.print(": ");
    ds.write(0x44, 1);
  
    delay(750);
  
    p = ds.reset();
    ds.select(addr);    
    ds.write(0xBE);  // Read Scratchpad

    for ( i = 0; i < 9; i++) {
        data[i] = ds.read();
    }

    int16_t raw = (data[1] << 8) | data[0];
    raw = raw << 3;
    if (data[7] == 0x10) {
        raw = (raw & 0xFFF0) + 12 - data[6];
    }
  
    temp = (float) raw / 16.0;
    Serial.print(temp);
    Serial.println(" Celsius");
}

void printTime() {
    int h = hour();
    int m = minute();
    int s = second();
   
    if (h < 10) {       // Add a zero, if necessary
        Serial.print(0);
    }
   
    Serial.print(h);
    Serial.print(":");  // And print the colon
   
    if (m < 10) {       // Add a zero, if necessary, as above
        Serial.print(0);
    }
   
    Serial.print(m);
    Serial.print(":");  // And print the colon
   
    if (s < 10) {       // Add a zero, if necessary, as above
        Serial.print(0);
    }
   
    Serial.print(s);
}
