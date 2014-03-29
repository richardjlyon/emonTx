/*
 EmonTx CT123
 
 emontx module firmware for CT and Pulse electricity monitoring.
 Reads CT, pulse, and battery state
 Sends to NanodeRF

 v1.2
 
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

#include <avr/wdt.h>  
#include "EmonLib.h"
#include <JeeLib.h>                                                     // Download JeeLib: http://github.com/jcw/jeelib

// Setup onboaard indicator LED
const int LEDpin = 9;                                                   
  
//  Time (ms) to allow the filters to settle before sending data
#define FILTERSETTLETIME 5000                                           

// Frequency of RF12B module can be RF12_433MHZ, RF12_868MHZ or RF12_915MHZ
#define freq RF12_868MHZ                                                

// emonTx RFM12B node ID
const int nodeID = 10; 
// emonTx RFM12B wireless network group - needs to be same as emonBase and emonGLCD
const int networkGroup = 210;                                           

// set 0 to disable CT channels
const int CT1 = 1; 
const int CT2 = 1;                                                      
const int CT3 = 1;                                                      

// EmonCMS LED widget feed value
const int RED = 0;                                                      
const int GREY = 2;                                                     

// Set to 0 if your not using the UNO bootloader (i.e using Duemilanove) - All Atmega's shipped from OpenEnergyMonitor come with Arduino Uno bootloader
const int UNO = 1;                                                      

// Attached JeeLib sleep function to Atmega328 watchdog -enables MCU to be put into sleep mode inbetween readings to reduce power consumption 
ISR(WDT_vect) { Sleepy::watchdogEvent(); }                              

// Create  instances for each CT channel
EnergyMonitor ct1,ct2,ct3;                                              

// create structure - a neat way of packaging data for RF comms
typedef struct { int power1, power2, power3, battery, heatingLED; } PayloadTX;      
PayloadTX emontx;                                                       

boolean settled = false;

void setup() 
{
  Serial.begin(57600);
  Serial.println("emonTX CT123 v1.2"); 
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
  
  // initialize RFM12B
  rf12_initialize(nodeID, freq, networkGroup);                          
  rf12_sleep(RF12_SLEEP);
  
  pinMode(LEDpin, OUTPUT);                                              
  digitalWrite(LEDpin, HIGH);

  // Enable anti crash (restart) watchdog if UNO bootloader is selected. Watchdog does not work with duemilanove bootloader
  // Restarts emonTx if sketch hangs for more than 8s
  if (UNO) wdt_enable(WDTO_8S);                                                                                                     
}

void loop() 
//NOTE: ct.calcIrms(number of wavelengths sample)*AC RMS voltage
{ 
  if (CT1) {
    emontx.power1 = ct1.calcIrms(1480) * 240.0;                         
    Serial.print(emontx.power1); Serial.print(" ");                                      
  }

  if (CT2) {
    emontx.power2 = ct2.calcIrms(1480) * 240.0;                         
    Serial.print(emontx.power2); Serial.print(" ");                                      
  }

  if (CT3) {
    emontx.power3 = ct3.calcIrms(1480) * 240.0;
    Serial.print(emontx.power3); Serial.print(" ");
  } 

 // set heating LED if drawing more than 1kW
  if (emontx.power2 > 1000) {
    emontx.heatingLED = RED; 
  }
  else {
    emontx.heatingLED = GREY;
  } 

  emontx.battery = ct1.readVcc();
 
  Serial.println(); 
  delay(100);

  // because millis() returns to zero after 50 days ! 
  if (!settled && millis() > FILTERSETTLETIME) settled = true;

  if (settled)                                                            // send data only after filters have settled
  { 
    send_rf_data();                                                       // *SEND RF DATA* - see emontx_lib
    digitalWrite(LEDpin, HIGH); delay(2); digitalWrite(LEDpin, LOW);      // flash LED
    emontx_sleep(5);                                                      // sleep or delay in seconds - see emontx_lib
  }
}
