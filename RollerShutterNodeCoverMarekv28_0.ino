/*
https://github.com/gryzli133/RollerShutterSplit

v27:
  - new logic for counting the position
  - new feature Toggle Button (UP/STOP/DOWN/STOP/UP/...)
      - you can choose UP and DOWN buttons or Toggle button -> or if needed all of them ;-)
      - if you don't need a button then use MP_PIN_NONE instead
      
v27.2:
  - issue with timers longer than 32s is now solved. The bilnds can now run up to 255s in each direction.
  
v27.3: 
  - issue with no calibration in direction DOWN is now solved. The bilnds will calibrate now in each direction.
  
v28.0:
  - new logic for Send() according to new Home Assistant MySensors implementation

*/

// Enable MySensors debug prints to serial monitor
//#define MY_DEBUG

// Enable MP_DEBUG_SHUTTER debug prints to serial monitor
//#define MP_DEBUG_SHUTTER

// Enable prints with loop cycle time
//#define MP_DEBUG_CYCLE_TIME

// Use a bit lower baudrate for serial prints on ESP8266 than default in MyConfig.h
//#define MY_BAUD_RATE 9600
//#define MY_BAUD_RATE 115200

#define MY_TRANSPORT_WAIT_READY_MS 1

// uncomment if we want to manually assign an ID
#define MY_NODE_ID 12


// !!!! CHOOSE ONLY ONE !!!!

//#include "ESPGateway.h" // for ESP8266 WiFi gateway -> set your WiFi in ESPGateway.h tab!
//#include "SerialGateway.h" // for Serial gateway
//#include "RF24Gateway.h" // for RF24 radio gateway
//#include "MQTTGateway.h" // for MQTT Ethernet gateway
#include "PJON.h" // for PJON wired gateway



#include <Bounce2.h>
#include <MySensors.h>
#include <SPI.h>

#include "RollerShutter.h"

/* define your RollerShutter objects here
 RollerShutter(int childId      // Cover/Roller Shutter device ID for MySensors and Controller (Domoticz, HA etc)   - also used as EEPROM address - must be unique !
              , int setIdUp     // Roll time UP setpoint device ID for MySensors and Controller (Domoticz, HA etc)  - also used as EEPROM address - must be unique !
              , int setIdDown   // Roll time DOWN setpointdevice ID for MySensors and Controller (Domoticz, HA etc) - also used as EEPROM address - must be unique !
              , int initId      // Initialization device ID for MySensors and Controller (Domoticz, HA etc)         - also used as EEPROM address - must be unique !
              , int buttonUp    // Button Pin for UP
              , int buttonDown  // Button Pin for DOWN
              , int buttonToggle// Button Pin to TOGGLE (UP/STOP/DOWN)
              , int relayUp     // Relay Pin for UP
              , int relayDown   // Relay Pin for DOWN
              , uint8_t initTimeUp          // Initial Roll time UP
              , uint8_t initTimeDown        // Initial Roll time DOWN
              , uint8_t initCalibrationTime // Initial calibration time (time that relay stay ON after reach 0 or 100%)
              , int debaunceTime            // Time to debounce button -> standard = 50
              , bool invertedRelay          // for High level trigger = 0; for Low level trigger = 1
              )
*/

bool service = 0;
int servicePin = 2;
int serviceLedPin = 13; // built-in LED
uint32_t lastCycle;
uint32_t minCycle = 0;
uint32_t maxCycle = 60000;
int cycleCount=0;
int newLevel = 0;
uint32_t lastAllUp;

// Enable thermostat object to change the time from Domoticz/HA
//#define MP_BLINDS_TIME_THERMOSTAT

RollerShutter blinds[] =
{
  //A12
  {1, 21, 41, 61, 22, 23, MP_PIN_NONE, 38, 39, 30, 30, 30, 50, 0, "Screen w kuchni"},
  {2, 22, 42, 62, 24, 25, MP_PIN_NONE, 40, 41, 30, 30, 30, 50, 0, "Screnn HS lewy"},
  {3, 23, 43, 63, 26, 27, MP_PIN_NONE, 42, 43, 30, 30, 30, 50, 0, "Screnn HS prawy"},
  {4, 24, 44, 64, 28, 29, MP_PIN_NONE, 44, 45, 30, 30, 30, 50, 0, "Screen FIX"},
  {5, 25, 45, 65, 30, 31, MP_PIN_NONE, 46, 47, 30, 30, 30, 50, 0, "Screen 5"},
  {6, 26, 46, 66, 32, 33, MP_PIN_NONE, 48, 49, 30, 30, 30, 50, 0, "Screen 6"},
  {7, 27, 47, 67, 34, 35, MP_PIN_NONE, 50, 51, 30, 30, 30, 50, 0, "Screen 7"},
  {8, 28, 48, 68, 36, 37, MP_PIN_NONE, 52, 53, 30, 30, 30, 50, 0, "Screen 8"},
  {9, 29, 49, 69, A0, A1, MP_PIN_NONE, A8, A9, 30, 30, 30, 50, 0, "Screen 9"},
  {10, 30, 50, 70, A2, A3, MP_PIN_NONE, A10, A11, 30, 30, 30, 50, 0, "Screen 10"}

  //A41
  /*{1, 21, 41, 61, 22, 23, MP_PIN_NONE, 38, 39, 21, 20, 21, 50, 0, "Roleta WC biuro"},
  {2, 22, 42, 62, 24, 25, MP_PIN_NONE, 40, 41, 30, 29, 30, 50, 0, "Roleta socjalny"},
  {3, 23, 43, 63, 26, 27, MP_PIN_NONE, 42, 43, 30, 29, 30, 50, 0, "Roleta lewa od górnej biuro"},
  {4, 24, 44, 64, 28, 29, MP_PIN_NONE, 44, 45, 30, 29, 30, 50, 0, "Roleta prawa od górnej biuro"},
  {5, 25, 45, 65, 30, 31, MP_PIN_NONE, 46, 47, 31, 30, 31, 50, 0, "Roleta lewa od bocznej biuro"},
  {6, 26, 46, 66, 32, 33, MP_PIN_NONE, 48, 49, 20, 19, 20, 50, 0, "Roleta prawa od bocznej biuro"},
  {7, 27, 47, 67, 34, 35, MP_PIN_NONE, 50, 51, 17, 16, 17, 50, 0, "Roleta okno balkonowe biuro"}*/
};

const int blindsCount = sizeof(blinds) / sizeof(RollerShutter);

void setup() 
{ 
  Serial.println("Started");
  // Service Mode input = D2 -> only if pass-through mode is needed, f.e. for programming the build-in motor's limit switches.
  pinMode(servicePin, INPUT_PULLUP);
  service = !digitalRead(servicePin);
  pinMode(serviceLedPin, OUTPUT);
  digitalWrite(serviceLedPin, service);
  
  // Setup locally attached sensors
  wait(1100);
  for(int i = 0; i < blindsCount; i++)
  {
    if(service)
    {
      blinds[i].enterServiceMode(); 
    }
    else
    {
      blinds[i].SyncController(); 
    }
  }
}

void presentation()  
{   
  // Send the sketch version information to the gateway and Controller
  sendSketchInfo("MP_RollerShutter", "28.0");
  for(int i = 0; i < blindsCount; i++)
  {
    blinds[i].Present(); 
  }
}

void loop() 
{ 
  
  for(int i = 0; i < blindsCount; i++)
  {
    blinds[i].Update(); 
  }

  #ifdef MP_DEBUG_CYCLE_TIME
  if(cycleCount>1000)
  {
    Serial.println(String(minCycle));
    Serial.println(String(maxCycle));
    cycleCount = 0;
    minCycle = 60000;
    maxCycle = 0;
  }
  minCycle = min(millis() - lastCycle , minCycle);
  maxCycle = max(millis() - lastCycle , maxCycle);
  cycleCount++;
  lastCycle = millis();
  #endif
  
}

void receive(const MyMessage &message) 
{
  for(int i = 0; i < blindsCount; i++)
  {
    blinds[i].Receive(message); 
  }
}
