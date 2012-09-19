//------------------------------------------------------------------------
// Single file IR_PID 
//
// IR_PID - Main File
// PID Controller for a Hot Plate for Surface Mount Soldering.
//  Scott Dixon & Jim Larson
// January 2010
// 
// Based on this original work:
// BBCC Main
// Tim Hirzel
// February 2008
//
// Main file for the Bare Bones Coffee Controller PID 
// setup for Arduino.
// This project is set up such that each tab acts as a 
// "module" or "library" that incporporates some more 
// functionality.  Each tab correlates 
// to a particular device (Nunchuck),  protocol (ie. SPI), 
// or Algorithm (ie. PID).

// The general rule for any of these tabs/sections is that 
// if they include a setup* or update* function, those should be added
// into the main setup and main loop functions. Also, in main loop, and in 
// extra code, delays should probably be avoided.
// Instead, use millis() and check for a certain interval to have passed.
//
// All code released under
// Creative Commons Attribution-Noncommercial-Share Alike 3.0 


// These are addresses into EEPROM memory.  The values to be stores are floats which 
// need 4 bytes each.  Thus 0,4,8,12,...
#define PGAIN_ADR 0
#define IGAIN_ADR 4
#define DGAIN_ADR 8

#define TEMP_SETTING_ADR 12
#define TEMP_ERROR_VALUE= -127.00

#define PID_UPDATE_INTERVAL 200 // milliseconds
#define WINDUP_GUARD_GAIN 100.0

#define IR_DATA 4
#define IR_CLK 3 
#define IR_INT 1
#define HEAT_RELAY_PIN 13
// pid settings.


#include <avr/EEPROM.h>
#include "WProgram.h"

void setup();
void setTargetTemp(float t);
float getTargetTemp();
void loop();
float readFloat(int address);
void writeFloat(float value, int address);
void setupPID(unsigned int padd, int iadd, int dadd);
float getP();
float getI();
float getD();
void setP(float p);
void setI(float i);
void setD(float d);
float updatePID(float targetTemp, float curTemp);
void printPIDDebugString();
void setupHeater();
void updateHeater();
void setHeatPowerPercentage(float power);
float getHeatCycles();
void _turnHeatElementOnOff(boolean on);
void setupSerialInterface();
void printHelp();
void updateSerialInterface();
void printStatus();
void printStatusForGraph();
void printFloat(float value, int places);
void setupTempSensor();
void updateTempSensor();
void readBit();
float getFreshTemp();
float getLastTemp();
float targetTemp;  //current temperature goal
float heatPower; // 0 - 1000  milliseconds on per second
float thermo_temp;

unsigned long lastPIDTime;  // most recent PID update time in ms 
volatile unsigned long lastRead;
volatile unsigned long epoch;


//-------------------------------------------------------------EEPROM float stuff
// Simple extension to the EEPROM library
// Tim Hirzel
// All code released under
// Creative Commons Attribution-Noncommercial-Share Alike 3.0 


float readFloat(int address) {
    float out;
    eeprom_read_block((void *) &out, (unsigned char *) address ,4 );
    return out;
}

void writeFloat(float value, int address) {
    eeprom_write_block((void *) &value, (unsigned char *) address ,4);
}

//------------------------------------------------------PID control code
// Tim Hirzel
// December 2007

// This is a module that implements a PID control loop
// initialize it with 3 values: p,i,d
// and then tune the feedback loop with the setP etc funcs
//
// this was written based on a great PID by Tim Wescott:
// http://www.embedded.com/2000/0010/0010feat3.htm
//
//
// All code released under
// Creative Commons Attribution-Noncommercial-Share Alike 3.0 



float iState = 0;
float lastTemp = 0;

float pgain;
float igain;
float dgain;

float pTerm, iTerm, dTerm; 

int pgainAddress, igainAddress, dgainAddress;

void setupPID(unsigned int padd, int iadd, int dadd) {
    // with this setup, you pass the addresses for the PID algorithm to use to 
    // for storing the gain settings.  This way wastes 6 bytes to store the addresses,
    // but its nice because you can keep all the EEPROM address allocaton in once place.
    
    pgainAddress = padd;
    igainAddress = iadd;
    dgainAddress = dadd;
    
    pgain = readFloat(pgainAddress);
    igain = readFloat(igainAddress);
    dgain = readFloat(dgainAddress);
}

float getP() {
    // get the P gain 
    return pgain;
}
float getI() {
    // get the I gain
    return igain;
}
float getD() {
    // get the D gain
    return dgain;
}


void setP(float p) {
    // set the P gain and store it to eeprom
    pgain = p; 
    writeFloat(p, pgainAddress);
}

void setI(float i) {
    // set the I gain and store it to eeprom
    igain = i; 
    writeFloat(i, igainAddress);
}

void setD(float d) {
    // set the D gain and store it to eeprom
    dgain = d; 
    writeFloat(d, dgainAddress);
}

float updatePID(float targetTemp, float curTemp)
{
    // these local variables can be factored out if memory is an issue, 
    // but they make it more readable
    double result;
    float error;
    float windupGaurd;
    
    // determine how badly we are doing
    error = targetTemp - curTemp;
    
    // the pTerm is the view from now, the pgain judges 
    // how much we care about error we are this instant.
    pTerm = pgain * error;
    
    // iState keeps changing over time; it's 
    // overall "performance" over time, or accumulated error
    iState += error;
    
    // to prevent the iTerm getting huge despite lots of 
    //  error, we use a "windup guard" 
    // (this happens when the machine is first turned on and
    // it cant help be cold despite its best efforts)
    
    // not necessary, but this makes windup guard values 
    // relative to the current iGain
    windupGaurd = WINDUP_GUARD_GAIN / igain;  
    
    if (iState > windupGaurd) 
        iState = windupGaurd;
    else if (iState < -windupGaurd) 
        iState = -windupGaurd;
    iTerm = igain * iState;
    
    // the dTerm, the difference between the temperature now
    //  and our last reading, indicated the "speed," 
    // how quickly the temp is changing. (aka. Differential)
    dTerm = (dgain* (curTemp - lastTemp));
    
    // now that we've use lastTemp, put the current temp in
    // our pocket until for the next round
    lastTemp = curTemp;
    
    // the magic feedback bit
    return  pTerm + iTerm - dTerm;
}

void printPIDDebugString() {
    // A  helper function to keep track of the PID algorithm 
    Serial.print("PID formula (P + I - D): ");
    
    printFloat(pTerm, 2);
    Serial.print(" + ");
    printFloat(iTerm, 2);
    Serial.print(" - ");
    printFloat(dTerm, 2);
    Serial.print(" POWER: ");
    printFloat(getHeatCycles(), 0);
    Serial.print(" ");
    
}

//-----------------------------------------------------------------HeaterControl
// Adapted for Surface Mount Soldering with a Hot Plate
// Jim Larson
// Jan 2010
//
// Original work by (and all credit to):
// Tim Hirzel 
// Dec 2007
// 
// This file is for controlling a heater via a solid state zero crossing relay
// since these are zero-crossing relays, it makes sense to just match my local
// AC frequency, 60hz
//
// All code released under
// Creative Commons Attribution-Noncommercial-Share Alike 3.0 
//----------------------------------------------------------------
// Define here the pin used for the control output to the Hot Plate
//  AC controller. Any digital output pin can be used.
//#define HEAT_RELAY_PIN PIN_B4
//----------------------------------------------------------------

float heatcycles; // the number of millis out of 1000 for the current heat amount (percent * 10)

boolean heaterState = 0;

unsigned long heatCurrentTime, heatLastTime;

void setupHeater() {
    pinMode(HEAT_RELAY_PIN , OUTPUT);
}


void updateHeater() {
    boolean h;
    heatCurrentTime = millis();
    if(heatCurrentTime - heatLastTime >= 1000 or heatLastTime > heatCurrentTime) { //second statement prevents overflow errors
        // begin cycle
        _turnHeatElementOnOff(1);  // 
        heatLastTime = heatCurrentTime;   
    } 
    if (heatCurrentTime - heatLastTime >= heatcycles) {
        _turnHeatElementOnOff(0);
    }
}

void setHeatPowerPercentage(float power) {
    if (power <= 0.0) {
        power = 0.0;
    }	
    if (power >= 1000.0) {
        power = 1000.0;
    }
    heatcycles = power;
}

float getHeatCycles() {
    return heatcycles;
}

void _turnHeatElementOnOff(boolean on) {
    digitalWrite(HEAT_RELAY_PIN, on);	//turn pin high
    heaterState = on;
}

//-------------------------------------------------serialInterface
//
// Slightly modified for use with SMT Hot Plate soldering system.
// Jim Larson, January 2010
//
// Based on original work by:
// Tim Hirzel February 2008
// This is a very basic serial interface for controlling the PID loop.
// thanks to the Serial example code  

// All code released under
// Creative Commons Attribution-Noncommercial-Share Alike 3.0 
//---------------------------------------------------------------
// Specify your baud rate here
//int myBaud = 115200;
int myBaud = 9600;
//---------------------------------------------------------------

#define AUTO_PRINT_INTERVAL 200  // milliseconds
#define MAX_DELTA  100
#define MIN_DELTA  0.01
#define PRINT_PLACES_AFTER_DECIMAL 2  // set to match MIN_DELTA


int incomingByte = 0;
float delta = 1.0;
boolean autoupdate;
boolean printmode = 0;

unsigned long lastUpdateTime = 0;
void setupSerialInterface()  {
    Serial.begin(myBaud);
    Serial.println("\nWelcome to the HPSS, the Hot Plate Solder System for Arduino");
    Serial.println("\nBased on the BBCC, the Bare Bones Coffee Controller for Arduino");
    Serial.println("Send back one or more characters to setup the controller.");
    Serial.println("If this is your initial run, please enter 'R' to Reset the EEPROM.");
    Serial.println("Enter '?' for help.  Here's to a great cup!");
}

void printHelp() {
    Serial.println("Send these characters for control:");
    Serial.println("<space> : print status now");
    Serial.println("u : toggle periodic status update");
    Serial.println("g : toggle update style between human and graphing mode");
    Serial.println("R : reset/initialize PID gain values");
    Serial.println("b : print PID debug values");
    Serial.println("? : print help");  
    Serial.println("+/- : adjust delta by a factor of ten");
    Serial.println("P/p : up/down adjust p gain by delta");
    Serial.println("I/i : up/down adjust i gain by delta");
    Serial.println("D/d : up/down adjust d gain by delta");
    Serial.println("T/t : up/down adjust set temp by delta");
    
    
}

void updateSerialInterface() {
    while(Serial.available()){
        
        incomingByte = Serial.read();
        if (incomingByte == 'R') {
            setP(30.0); // make sure to keep the decimal point on these values
            setI(0.0);  // make sure to keep the decimal point on these values
            setD(0.0);  // make sure to keep the decimal point on these values
            setTargetTemp(200.0); // here too
        } 
        if (incomingByte == 'P') {
            setP(getP() + delta);
        } 
        if (incomingByte == 'p') {
            setP(getP() - delta);
        } 
        if (incomingByte == 'I') {
            setI(getI() + delta);
        } 
        if (incomingByte == 'i') {
            setI(getI() - delta);
        } 
        if (incomingByte == 'D') {
            setD(getD() + delta);
        } 
        if (incomingByte == 'd' ){
            setD(getD() - delta);
        } 
        if (incomingByte == 'T') {
            setTargetTemp(getTargetTemp() + delta);
        } 
        if (incomingByte == 't') {
            setTargetTemp(getTargetTemp() - delta);
        }
        if (incomingByte == '+') {
            delta *= 10.0;
            if (delta > MAX_DELTA)
                delta = MAX_DELTA;
        } 
        if (incomingByte == '-') {
            delta /= 10.0;
            if (delta < MIN_DELTA)
                delta = MIN_DELTA;
            
        }
        if (incomingByte == 'u') {
            // toggle updating
            
            autoupdate = not autoupdate;
        }
        if (incomingByte == 'g') {
            // toggle updating
            
            printmode = not printmode;
        }
        if (incomingByte == ' ') {
            // toggle updating
            
            printStatus();
        }
        if (incomingByte == '?') {
            printHelp(); 
        }
        if (incomingByte == 'b') {
            printPIDDebugString(); 
            Serial.println();
        }
    }
    
    if (millis() < lastUpdateTime) {
        lastUpdateTime = 0;
    }
    if ((millis() - lastUpdateTime) > AUTO_PRINT_INTERVAL) {
        // this is triggers every slightly more than a second from the delay between these two millis() calls
        lastUpdateTime += AUTO_PRINT_INTERVAL;
        if (autoupdate) {
            if (printmode) {
                printStatusForGraph();
            }
            else {
                printStatus();
            }
        } 
    }
}

void printStatus() { 
    // A means for getting feedback on the current system status and controllable parameters
    Serial.print(" SET TEMP:");
    printFloat(getTargetTemp(),PRINT_PLACES_AFTER_DECIMAL);
    Serial.print(", CUR TEMP:");
    printFloat(getLastTemp(),PRINT_PLACES_AFTER_DECIMAL);
    
    Serial.print(", GAINS p:");
    printFloat(getP(),PRINT_PLACES_AFTER_DECIMAL);
    Serial.print(" i:");
    printFloat(getI(),PRINT_PLACES_AFTER_DECIMAL);
    Serial.print(" d:");
    printFloat(getD(),PRINT_PLACES_AFTER_DECIMAL);
    Serial.print(", Delta: ");
    printFloat(delta,PRINT_PLACES_AFTER_DECIMAL);
    Serial.print(", Power: ");
    printFloat((float)getHeatCycles(), 0);
    
    Serial.print("\r\n");
}

void printStatusForGraph() {
    printFloat(getTargetTemp(),PRINT_PLACES_AFTER_DECIMAL);
    Serial.print(", ");
    printFloat(getLastTemp(),PRINT_PLACES_AFTER_DECIMAL);
    Serial.print(", ");
    printFloat(getP(),PRINT_PLACES_AFTER_DECIMAL);
    Serial.print(", ");
    printFloat(getI(),PRINT_PLACES_AFTER_DECIMAL);
    Serial.print(", ");
    printFloat(getD(),PRINT_PLACES_AFTER_DECIMAL);
    Serial.print(", ");
    printFloat((float)getHeatCycles(), 0);
    Serial.println();
}

// printFloat prints out the float 'value' rounded to 'places' places after the decimal point
void printFloat(float value, int places) {
    // this is used to cast digits 
    int digit;
    float tens = 0.1;
    int tenscount = 0;
    int i;
    float tempfloat = value;
    
    // make sure we round properly. this could use pow from <math.h>, but doesn't seem worth the import
    // if this rounding step isn't here, the value  54.321 prints as 54.3209
    
    // calculate rounding term d:   0.5/pow(10,places)  
    float d = 0.5;
    if (value < 0)
        d *= -1.0;
    // divide by ten for each decimal place
    for (i = 0; i < places; i++)
        d/= 10.0;    
    // this small addition, combined with truncation will round our values properly 
    tempfloat +=  d;
    
    // first get value tens to be the large power of ten less than value
    // tenscount isn't necessary but it would be useful if you wanted to know after this how many chars the number will take
    
    if (value < 0)
        tempfloat *= -1.0;
    while ((tens * 10.0) <= tempfloat) {
        tens *= 10.0;
        tenscount += 1;
    }
    
    
    // write out the negative if needed
    if (value < 0)
        Serial.print('-');
    
    if (tenscount == 0)
        Serial.print(0, DEC);
    
    for (i=0; i< tenscount; i++) {
        digit = (int) (tempfloat/tens);
        Serial.print(digit, DEC);
        tempfloat = tempfloat - ((float)digit * tens);
        tens /= 10.0;
    }
    
    // if no places after decimal, stop now and return
    if (places <= 0)
        return;
    
    // otherwise, write the point and continue on
    Serial.print('.');  
    
    // now write out each decimal place by shifting digits one by one into the ones place and writing the truncated value
    for (i = 0; i < places; i++) {
        tempfloat *= 10.0; 
        digit = (int) tempfloat;
        Serial.print(digit,DEC);  
        // once written, subtract off that digit
        tempfloat = tempfloat - (float) digit; 
    }
}

// temp - routines to read temperature strings from an IR 
//        Temperature Sensor.
// Created by Scott Dixon January, 2010.
// Based on documentation from 
// http://www.zytemp.com/download/TNm_302.pdf
// All code released under
// Creative Commons Attribution-Noncommercial-Share Alike 3.0 
//
//------------------------------------------------------------
// Define here the pin numbers to be used for clock and data
//  from the sensor. The clock line must be able to attach to
//  an interrupt.
//#define IR_CLK PIN_D1
//#define IR_DATA PIN_D0
//-------------------------------------------------------------

volatile int nbits = 0;
volatile  byte hexbyte = 0;
volatile  byte read_byte;
volatile int byte_ready = 0;

volatile unsigned char message[4];
volatile int nbytes = 0;
volatile int message_waiting = 0;

unsigned long last_time = 0;
unsigned int consectutive_timeouts = 0;

float temp= -127.0;
float ambient;


float tcSum = 0.0;
float latestReading = 0.0;
int readCount = 0;
float multiplier;
void setupTempSensor() {
    pinMode(IR_CLK, INPUT);
//    digitalWrite(IR_CLK,HIGH);
    pinMode(IR_DATA, INPUT);
//    digitalWrite(IR_DATA,HIGH);
    //attachInterrupt(1, readBit, FALLING);
    attachInterrupt(IR_INT, readBit, FALLING);
}  

void updateTempSensor() {
    if (message_waiting == 1) {
        last_time = millis();
        consectutive_timeouts=0;
        if (message[0] == 0x4c) {
            int t = message[1]<<8 | message[2];
            temp = t/16.0 -273.15;
        } else if (message[0] == 0x66) {
            int t = message[1]<<8 | message[2];
            ambient = t/16.0 -273.15;
        }
        message_waiting = 0;
    }
    tcSum += temp;
    readCount +=1;
    
    if (millis() - last_time > 1000) {
        nbits = 0;
        nbytes = 0;
        hexbyte = 0;
        message_waiting = 0;
        byte_ready = 0;
        last_time = millis();
        consectutive_timeouts++;
    }
    
    
}
// Interupt routine for handling IR sensor clock trailing edge
void readBit() {
    int val = digitalRead(IR_DATA);
    nbits++;
    int bit = (val == HIGH) ? 1 : 0;
    hexbyte = (hexbyte << 1) | bit;
    if (nbits == 8) {
        if (byte_ready == 0) {
            read_byte = hexbyte;
            byte_ready = 1;
        }
        if (hexbyte == 0xd) {
            nbytes = 0;
            message_waiting = 1;
        } else if (message_waiting == 0) {
            if (nbytes < 4) {
                message[nbytes] = hexbyte;
            }
            nbytes++;
        }
        hexbyte = 0;
        nbits = 0;
    }
}

float getFreshTemp() { 
    latestReading = temp;
    readCount = 0;
    tcSum = 0.0;
    return latestReading;
    
}

float getLastTemp() {
    return latestReading;
    
}

//-----------------------------------------------------main (Setup and loop)


void setup()
{

  setupPID(PGAIN_ADR, IGAIN_ADR, DGAIN_ADR ); // Send addresses to the PID module 
  targetTemp = readFloat(TEMP_SETTING_ADR); // from EEPROM. load the saved value
  lastPIDTime = millis();
  // module setup calls
  setupHeater();
  setupSerialInterface();
  setupTempSensor();
    epoch=millis()/1000L;
}

void setTargetTemp(float t) {
  targetTemp = t;
  writeFloat(t, TEMP_SETTING_ADR);
}

float getTargetTemp() {
  return targetTemp;
}


void loop()
{ 
  epoch=millis()/1000L;
  // this call interprets characters from the serial port
  // its a very basic control to allow adjustment of gain values, and set temp
  updateSerialInterface(); 
  updateTempSensor();
  
  // every second, udpate the current heat control, and print out current status

  // This checks for rollover with millis()
  if (millis() < lastPIDTime) {
    lastPIDTime = 0;
  }

  if ((millis() - lastPIDTime) > PID_UPDATE_INTERVAL) {
    lastPIDTime +=  PID_UPDATE_INTERVAL;
    heatPower = updatePID(targetTemp, getFreshTemp());
    setHeatPowerPercentage(heatPower);
  }
    /* safety */
    //if (getFreshTemp() == -127.00) {
    //    setHeatPowerPercentage(0);
        //bark real loud!!!!
    //    Serial.println("DBG: Sensor returned error");
    //}
    if (consectutive_timeouts>20){
        setHeatPowerPercentage(0);
        //bark real loud!!!!
        //Serial.println("DBG: missing overriding temp");
    }
    
  updateHeater();

}


