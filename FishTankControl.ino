#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <functional>
#include "switch.h"
#include "UpnpBroadcastResponder.h"
#include "CallbackFunction.h"
#include <Arduino.h>

//code modified to use the newer LC Technology X2 relay board that is interfaced with serial, rather than using GPIO Drive

// To Do
// DONE sus out serial debug from the X2 module - can we program from there?

 // DONE add DI's on GPIO 0,2,3 and link to tank fill, tank mt and local control button for tank cycle
 // DONE add logic for tank fill & mt
 // Add temp switches and buttons for level sensors and buttons
 // DONE add logic for tank cycle
 // Intergate level sensor module
 // build into box
 // add indicator leds for tank full, tank mt if not on level sensor
 // Maybe correct the reported states so Alexa App shows fill and empty modes
 
 // bugs
/*
 * DONE The values fed back to alexa on commds are screwed. she says "not responding a lot
 * 
 */
 

// Site specfic variables

const char* ssid = "A_Virtual_Information";
const char* password = "BananaRock";

byte rel1ON[] = {0xA0, 0x01, 0x01, 0xA2};  //Hex command to send to serial for open relay 1
byte rel1OFF[] = {0xA0, 0x01, 0x00, 0xA1}; //Hex command to send to serial for close relay 1
byte rel2ON[] = {0xA0, 0x02, 0x01, 0xA3};  //Hex command to send to serial for open relay 2
byte rel2OFF[] = {0xA0, 0x02, 0x00, 0xA2}; //Hex command to send to serial for close relay 2

// const int RelayPulseLength = 1000;    //Length of relay pulse in ms 
// prototypes
boolean connectWifi();

//on/off callbacks
bool FillFishTankOnRequest();
bool FillFishTankOffRequest();
bool EmptyFishTankOnRequest();
bool EmptyFishTankOffRequest();
bool CycleFishTankOnRequest();
bool CycleFishTankOffRequest();

bool FillingFishTankState = false; // Tank is filling
bool EmptyingFishTankState = false; // Tank is Emptying
bool CyclingFishTankState = false; // Tank is Cycling


bool TankFullState = false ; // Current State of the Tank Full Sensor
bool TankEmptyState = false; // Current State of the Tank Empty Sensor
bool PushButtonState = false; // Current State of the Local Push Button

bool PrevTankFullState;
bool PrevTankEmptyState;
bool PrevPushButtonState;

bool CycleModeControlToggle = false;

bool FillBackOff = false;
int  FillBackOffCount = 0;
int  FillBackOffTerminalCount = 60;
bool EmptyBackOff = false;
int  EmptyBackOffCount = 0;
int  EmptyBackOffTerminalCount = 60;

boolean wifiConnected = false;

UpnpBroadcastResponder upnpBroadcastResponder;

Switch *FillFishTank = NULL;
Switch *EmptyFishTank = NULL;
Switch *CycleFishTank = NULL;

bool isFillFishTankOn = false;
bool isEmptyFishTankOn = false;
bool isCycleFishTankOn = false;
bool GarageDoorState = false;
//bool PrevGarageDoorState = false;
//bool PulseRelaySemaphore = false;

//const int GarageDoorRelay = 0;  // GPIO0 pin.
const int GPIO2 = 2;  // GPIO2 pin.

//long duration, distance; // Duration used to calculate distance

const int TankFullPin = 3;  // 3 is the RX pin
const int TankEmptyPin = 2;  
const int PushButtonPin = 0; 



void setup()
{

  Serial.begin(115200);

  Serial.println("Booting...");
  delay(2000);
  Serial.println("flashing LED on GPIO2...");
  //flash fast a few times to indicate CPU is booting
  digitalWrite(GPIO2, LOW);
  delay(100);
  digitalWrite(GPIO2, HIGH);
  delay(100);
  digitalWrite(GPIO2, LOW);
  delay(100);
  digitalWrite(GPIO2, HIGH);
  delay(100);
  digitalWrite(GPIO2, LOW);
  delay(100);
  digitalWrite(GPIO2, HIGH);

  Serial.println("Delaying a bit...");
  delay(2000);

  // Initialise wifi connection
  wifiConnected = connectWifi();

  if (wifiConnected) {
    Serial.println("flashing slow to indicate wifi connected...");
    //flash slow a few times to indicate wifi connected OK
    digitalWrite(GPIO2, LOW);
    delay(1000);
    digitalWrite(GPIO2, HIGH);
    delay(1000);
    digitalWrite(GPIO2, LOW);
    delay(1000);
    digitalWrite(GPIO2, HIGH);
    delay(1000);
    digitalWrite(GPIO2, LOW);
    delay(1000);
    digitalWrite(GPIO2, HIGH);

    Serial.println("starting upnp responder");
    upnpBroadcastResponder.beginUdpMulticast();

    // Define your switches here. Max 10
    // Format: Alexa invocation name, local port no, on callback, off callback
    FillFishTank = new Switch("Fill the fish tank", 90, FillFishTankOnRequest, FillFishTankOffRequest);
    EmptyFishTank = new Switch("Empty the fish tank", 91, EmptyFishTankOnRequest, EmptyFishTankOffRequest);
    CycleFishTank = new Switch("Cycle the fish tank", 92, CycleFishTankOnRequest, CycleFishTankOffRequest);

    Serial.println("Adding switches upnp broadcast responder");
    upnpBroadcastResponder.addDevice(*FillFishTank);
    upnpBroadcastResponder.addDevice(*EmptyFishTank);
    upnpBroadcastResponder.addDevice(*CycleFishTank);

  }

  //digitalWrite(GarageDoorRelay, LOW); // turn off relay
  digitalWrite(GPIO2, HIGH); // turn off LED

  Serial.println("Making RX pin into an INPUT"); // used to detect the tankFill State
 

  pinMode(TankFullPin, FUNCTION_3);
  pinMode(TankFullPin, INPUT);

    Serial.println("Making GPIO2  into an INPUT"); // used to detect the tank Empty State
 

  pinMode(TankEmptyPin, FUNCTION_3);
  pinMode(TankEmptyPin, INPUT);



  pinMode(TankFullPin, FUNCTION_3);
  pinMode(TankFullPin, INPUT);

    Serial.println("Making GPIO0  into an INPUT"); // used for the local push button control
 

  pinMode(PushButtonPin, FUNCTION_3);
  pinMode(PushButtonPin, INPUT);



}

void loop()
{

  delay(1000); // Main loop timer - 


  // Detect Tank water levels and push button states

  // Tank Full 
  TankFullState = digitalRead(TankFullPin); 
  delay(10);  
  if (TankFullState == LOW) {
      if (PrevTankFullState == HIGH){ 
        Serial.println("Tank has just reached the Full State - LOW");
      }
  }       
      
  if (TankFullState == HIGH) {
       if (PrevTankFullState == LOW){ 
        Serial.println("Tank has just left the Full State - HIGH");
      }
  }      

   PrevTankFullState = TankFullState;   // remember prev state for next pass

  // Tank Empty 
  TankEmptyState = digitalRead(TankEmptyPin); 
  delay(10);  
  if (TankEmptyState == LOW) {
      if (PrevTankEmptyState == HIGH){ 
        Serial.println("Tank has just reached the Empty State");
      }
  }       
      
  if (TankEmptyState == HIGH) {
       if (PrevTankEmptyState == LOW){ 
        Serial.println("Tank has just left the Empty State");
      }
  }      

   PrevTankEmptyState = TankEmptyState;   // remember prev state for next pass

  // Push Button 
  PushButtonState = digitalRead(PushButtonPin); 
  delay(10);  
  if (PushButtonState == LOW) {
      if (PrevPushButtonState == HIGH){ 
        Serial.println("Push Button Just pushed");
                // toggle in and out of Cycle mode each time button released
        CycleModeControlToggle = !CycleModeControlToggle;

        if (CycleModeControlToggle == true) {
          CycleFishTankOnRequest();
        }
        if (CycleModeControlToggle == false) {
          CycleFishTankOffRequest();
        }
      }
  }       
      
  if (PushButtonState == HIGH) {
       if (PrevPushButtonState == LOW){ 
        Serial.println("Push Button Just released");

        
      }
  }      

   PrevPushButtonState = PushButtonState;   // remember prev state for next pass

  
  // stop filling when tank is full on fill command

if (FillingFishTankState == true) { 
  //Serial.println("FillingFishTankState is active (true)");
  if (TankFullState == LOW) { // LOW is the Full state
   //Serial.println("FillingFishTankState is active and TankFullState is active (LOW) ");
    FillFishTankOffRequest(); // Call the stop filling command function
  }
}

  // stop emptying when tank is empty on empty command

  if (EmptyingFishTankState == true) { 
  //Serial.println("EmptyingFishTankState is active (true)");
  if (TankEmptyState == LOW) { // LOW is the Empty state
   //Serial.println("EmptyingFishTankState is active and TankEmptyState is active (LOW) ");
    EmptyFishTankOffRequest(); // Call the stop Emptying command function
  }
}

  // check tank isnt going to overfill or underfill on cycle  command
  // stop filling or emptying and backoff filling again for FillBackOffCounter

  if (CyclingFishTankState == true) { 
  //Serial.println("CyyclingFishTankState is active (true)");
  
  if (TankEmptyState == LOW) { // LOW is the Empty state
   //Serial.println("CyclingFishTankState is active and TankEmptyState is active (LOW) ");
    EmptyFishTankOffRequest(); // Call the stop Emptying command function
    Serial.println("Stopping emptying for Backoff period..");
    EmptyBackOff = true;   

  } 


  if (EmptyBackOff == true) {
     EmptyBackOffCount = EmptyBackOffCount + 1;
     if (EmptyBackOffCount >= EmptyBackOffTerminalCount) {
         Serial.write(rel2ON, sizeof(rel2ON)); // start Emptying tank again
         Serial.println("Restarting emptying ..");
         EmptyBackOffCount = 0 ; // ready to start again next time
         EmptyBackOff = false;
     }
  }

   if (TankFullState == LOW) { // LOW is the Full state
   //Serial.println("CyclingFishTankState is active and TankFullState is active (LOW) ");
    FillFishTankOffRequest(); // Call the stop filling command function
    Serial.println("Stopping filling for backoff period..");
    FillBackOff = true;
  } 

    if (FillBackOff == true) {
     FillBackOffCount = FillBackOffCount + 1;
     if (FillBackOffCount >= FillBackOffTerminalCount) {
         Serial.write(rel1ON, sizeof(rel1ON)); // start Filling tank again
         FillBackOffCount = 0 ; // ready to start again next time
         FillBackOff = false;
         Serial.println("Restarting Filling..");
     }
  } 
}







  if (wifiConnected) {

    upnpBroadcastResponder.serverLoop();

    CycleFishTank->serverLoop();
    EmptyFishTank->serverLoop();
    FillFishTank->serverLoop();
  }

 
}

bool FillFishTankOnRequest() {
  Serial.println("Request to fill Fish Tank received - Relay 1 on...");
  Serial.write(rel1ON, sizeof(rel1ON));
  FillingFishTankState = true;

  isFillFishTankOn = false;
  return FillingFishTankState;
}

bool FillFishTankOffRequest() {

  Serial.println("Request to stop filling fish tank received - Relay#1 Off ...");

Serial.write(rel1OFF, sizeof(rel1OFF));
FillingFishTankState = false;
  isFillFishTankOn = false;
  return FillingFishTankState;
}

bool EmptyFishTankOnRequest() {
  Serial.println("Request to Empty Fish tank  received - Relay 2 on...");

Serial.write(rel2ON, sizeof(rel2ON));
EmptyingFishTankState = true;
  isEmptyFishTankOn = false;
  return EmptyingFishTankState;
}

bool EmptyFishTankOffRequest() {

  Serial.println("Request to stop empting tank received relay 2 off ..");

 Serial.write(rel2OFF, sizeof(rel2OFF));
EmptyingFishTankState = false;
  isEmptyFishTankOn = false;
  return EmptyingFishTankState;
}




bool CycleFishTankOnRequest() {

Serial.println("Request to cycle tank received relay  1 & 2 on ..");
 Serial.write(rel1ON, sizeof(rel1ON));
  delay (100);
 Serial.write(rel2ON, sizeof(rel2ON));
CyclingFishTankState = true;
  isCycleFishTankOn = false;
  return CycleFishTankOnRequest;
}

bool CycleFishTankOffRequest() {

  Serial.println("Request to stop cycling tank received relay  1 & 2 off ..");
 Serial.write(rel1OFF, sizeof(rel1OFF));
 delay (100);
 Serial.write(rel2OFF, sizeof(rel2OFF));
 CyclingFishTankState = false;
  isCycleFishTankOn = false;
  return CyclingFishTankState;
}



// connect to wifi – returns true if successful or false if not
boolean connectWifi() {
  boolean state = true;
  int i = 0;

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");
  Serial.println("Connecting to WiFi Network");

  // Wait for connection
  Serial.print("Connecting ...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(5000);
    Serial.print(".");
    if (i > 10) {
      state = false;
      break;
    }
    i++;
  }

  if (state) {
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }
  else {
    Serial.println("");
    Serial.println("Connection failed. Bugger");
  }

  return state;
}


