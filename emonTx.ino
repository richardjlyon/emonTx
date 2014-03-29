/*
 EmonTx CT123
 
 emontx module firmware for CT and Pulse electricity monitoring.
 Reads CT, pulse, and battery state
 Sends to NanodeRF

 v1.1
 
 Builds upon JeeLabs RF12 library and Arduino
 
 emonTx documentation: 	http://openenergymonitor.org/emon/modules/emontx/
 emonTx firmware code explanation: http://openenergymonitor.org/emon/modules/emontx/firmware
 emonTx calibration instructions: http://openenergymonitor.org/emon/modules/emontx/firmware/calibration

 THIS SKETCH REQUIRES:

 Libraries in the standard arduino libraries folder:
	- JeeLib		https://github.com/jcw/jeelib
	- EmonLib		https://github.com/openenergymonitor/EmonLib.git

 Other files in project directory (should appear in the arduino tabs above)
	- emontx_lib.ino
 
*/

/*Recommended node ID allocation
------------------------------------------------------------------------------------------------------------
-ID-	-Node Type- 
0	- Special allocation in JeeLib RFM12 driver - reserved for OOK use
1-4     - Control nodes 
5-10	- Energy monitoring nodes
11-14	--Un-assigned --
15-16	- Base Station & logging nodes
17-30	- Environmental sensing nodes (temperature humidity etc.)
31	- Special allocation in JeeLib RFM12 driver - Node31 can communicate with nodes on any network group
-------------------------------------------------------------------------------------------------------------
*/

#define FILTERSETTLETIME 5000                                           //  Time (ms) to allow the filters to settle before sending data

const int CT1 = 1; 
const int CT2 = 1;                                                      // Set to 0 to disable CT channel 2
const int CT3 = 1;                                                      // Set to 0 to disable CT channel 3

const int RED = 0;                                                      // EmonCMS LED widget feed value - Red
const int GREY = 2;                                                     // EmonCMS LED widget feed value - Grey

#define freq RF12_868MHZ                                                // Frequency of RF12B module can be RF12_433MHZ, RF12_868MHZ or RF12_915MHZ. You should use the one matching the module you have.
const int nodeID = 10;                                                  // emonTx RFM12B node ID
const int networkGroup = 210;                                           // emonTx RFM12B wireless network group - needs to be same as emonBase and emonGLCD

const int UNO = 1;                                                      // Set to 0 if your not using the UNO bootloader (i.e using Duemilanove) - All Atmega's shipped from OpenEnergyMonitor come with Arduino Uno bootloader
#include <avr/wdt.h>                                                     

#include <JeeLib.h>                                                     // Download JeeLib: http://github.com/jcw/jeelib
ISR(WDT_vect) { Sleepy::watchdogEvent(); }                              // Attached JeeLib sleep function to Atmega328 watchdog -enables MCU to be put into sleep mode inbetween readings to reduce power consumption 

#include "EmonLib.h"
EnergyMonitor ct1,ct2,ct3;                                              // Create  instances for each CT channel

typedef struct { int power1, power2, power3, powerPulse, battery, heatingLED; } PayloadTX;      // create structure - a neat way of packaging data for RF comms
PayloadTX emontx;                                                       

const int LEDpin = 9;                                                   // On-board emonTx LED 

boolean settled = false;

// Pulse counting settings 
long pulseCount = 0;                                                    // Number of pulses, used to measure energy.
unsigned long pulseTime,lastTime;                                       // Used to measure power.
double power, elapsedWh;                                                // power and energy
int ppwh = 1;                                                           // 1000 pulses/kwh = 1 pulse per wh - Number of pulses per wh - found or set on the meter.

void setup() 
{
  Serial.begin(57600);
  Serial.println("emonTX CT123 v1.1"); 
  Serial.println("OpenEnergyMonitor.org");
  Serial.print("Node: "); 
  Serial.print(nodeID); 
  Serial.print(" Freq: "); 
  if (freq == RF12_433MHZ) Serial.print("433Mhz");
  if (freq == RF12_868MHZ) Serial.print("868Mhz");
  if (freq == RF12_915MHZ) Serial.print("915Mhz"); 
  Serial.print(" Network: "); 
  Serial.println(networkGroup);
           
  if (CT1) ct1.currentTX(1, 111.1);                                     // Setup emonTX CT channel (ADC input, calibration)  NOTE: calibrated was 105.4
  if (CT2) ct2.currentTX(2, 111.1);                                     // Calibration factor = CT ratio / burden resistance
  if (CT3) ct3.currentTX(3, 111.1);                                     // Calibration factor = (100A / 0.05A) / 33 Ohms
  
  rf12_initialize(nodeID, freq, networkGroup);                          // initialize RFM12B
  rf12_sleep(RF12_SLEEP);

  pinMode(LEDpin, OUTPUT);                                              // Setup indicator LED
  digitalWrite(LEDpin, HIGH);
  
  if (UNO) wdt_enable(WDTO_8S);                                         // Enable anti crash (restart) watchdog if UNO bootloader is selected. Watchdog does not work with duemilanove bootloader                                                             // Restarts emonTx if sketch hangs for more than 8s
  attachInterrupt(1, onPulse, FALLING);                                 // KWH interrupt attached to IRQ 1  = pin3 - hardwired to emonTx pulse jackplug. For connections see: http://openenergymonitor.org/emon/node/208
}

void loop() 
{ 
  if (CT1) {
    emontx.power1 = ct1.calcIrms(1480) * 240.0;                         //ct.calcIrms(number of wavelengths sample)*AC RMS voltage
    Serial.print(emontx.power1); Serial.print(" ");                                      
  }

  if (CT2) {
    emontx.power2 = ct2.calcIrms(1480) * 240.0;                         //ct.calcIrms(number of wavelengths sample)*AC RMS voltage
    Serial.print(emontx.power2); Serial.print(" ");                                      
  }

  if (CT3) {
    emontx.power3 = ct3.calcIrms(1480) * 240.0;
    Serial.print(emontx.power3); Serial.print(" ");
  } 

  if (emontx.power3 > 1000) {
    emontx.heatingLED = RED; 
  }
  else {
    emontx.heatingLED = GREY;
  } 

  emontx.battery = ct1.readVcc();
  //emontx.pulse = pulseCount; 
  pulseCount=0; 

  Serial.print(emontx.powerPulse); Serial.print(" ");                                      
  //Serial.print(emontx.pulse);
 
  Serial.println(); delay(100);

  // because millis() returns to zero after 50 days ! 
  if (!settled && millis() > FILTERSETTLETIME) settled = true;

  if (settled)                                                            // send data only after filters have settled
  { 
    send_rf_data();                                                       // *SEND RF DATA* - see emontx_lib
    digitalWrite(LEDpin, HIGH); delay(2); digitalWrite(LEDpin, LOW);      // flash LED
    emontx_sleep(5);                                                      // sleep or delay in seconds - see emontx_lib
  }
}

// The interrupt routine - runs each time a falling edge of a pulse is detected
void onPulse()                  
{
  lastTime = pulseTime;        //used to measure time between pulses.
  pulseTime = micros();
  pulseCount++;                                                           //pulseCounter               
  emontx.powerPulse = int((3600000000.0 / (pulseTime - lastTime))/ppwh);  //Calculate power
}
