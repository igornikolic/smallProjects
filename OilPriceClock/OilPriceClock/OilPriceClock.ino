//// We have a Wemost D1 mini R1, not R2
// When we boot, the pin status has to be as follows :
//                   gpio  0   2   15
// Flash Startup (Normal)  1   1   0

//On the R1, the pins are mapped as follows :
// GPIO 0 - D8 - HIGH
// GPIO 2 - D9 - HIGH
// GPIO 15 - D10 - LOW
// On your devices, check the pin mapping, and eitehr do not use them, or make sure you have
// appropriate pullups/pulldowns

//LED setup
#include "FastLED.h"
FASTLED_USING_NAMESPACE
#if defined(FASTLED_VERSION) && (FASTLED_VERSION < 3001000)
#warning "Requires FastLED 3.1 or later; check github for latest code."
#endif

#define DATA_PIN  D4 
#define CLK_PIN   D2
#define LED_TYPE    APA102 //sk9822 - clone of apa102, almost completely compatible, but better
#define COLOR_ORDER RGB
#define NUM_LEDS    3

CRGB leds[NUM_LEDS];
#define BRIGHTNESS         100
#define FRAMES_PER_SECOND  120
CRGBPalette16 palette = RainbowColors_p;



//Start/End stop switches
//Swithes are NO, meaning the the default state of the pins is HIGH, 1
//when pressed, they connect to ground, and state is LOW, 0
#define startSwitchPin D1 
#define endSwitchPin  D3

//Motor control
#define ENA  D5 
#define DIR  D7
#define PUL  D6
//We are using a worm-geared stepper motor, geared down 1:49, 
int stepCountPerRevolution = 8764; //there are 8764 steps between start and end switches.        

// Price variables
// We will get data from the CME group website
// https://www.cmegroup.com/CmeWS/mvc/Quotes/FutureContracts/XNYM/G?quoteCodes=CLX8
// CLX8 is oil futures

double price = 0.0;       // the variable that will store the oil price
double lastPrice = 0.0;    // the previous price
int changes = 0;          // variable that will hold the number of times we changes values. Will be used to rehome the dial every now and then 
int val = 0;              //the value to be displayed is set to 0
int lastVal = 0;          // we store the last value we set, so that we know how much to move

//We are using a BTS 432 E2 to turn the motor controller on / of
#define CTRL D0           //Pin controlling the motor driver

//Including the wifi libraries and setting the network related stuff
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#define MQTT_HOST   "revspace.nl"
#define MQTT_PORT   1883
#define MQTT_TOPIC  "revspace/oilFuturesPrice"

WiFiManager wifiManager;
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

/////////////////////////////////// SETUP //////////////////////////////
void setup() {
  // 1 second delay for recovery
  delay(1000);      
  
  // initialize the serial port:
  Serial.begin(115200);


  //Wifi setup
  wifiManager.setDebugOutput(true);
  wifiManager.autoConnect ("AutoConnectAP");
  Serial.print ("Mac adress :");
  Serial.println(WiFi.macAddress());
  WiFi.printDiag(Serial);

  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setCallback(callback);


  //initialize the buttons pins
  // the switches are wired to be normally closed to Ground 
  pinMode(startSwitchPin, INPUT_PULLUP);
  pinMode(endSwitchPin, INPUT_PULLUP);

  // LED Setup    
  // tell FastLED about the LED strip configuration
  FastLED.addLeds < LED_TYPE, DATA_PIN, CLK_PIN, COLOR_ORDER > (leds,
                NUM_LEDS).setCorrection
    (TypicalLEDStrip);
  // set master brightness control
  FastLED.setBrightness (BRIGHTNESS);
  

 for (int i = 0; i < NUM_LEDS; i++)
     {
       leds[i] = CRGB::Green;
      }
    FastLED.show ();

  // initialize motor pins
  pinMode (PUL, OUTPUT);
  pinMode (DIR, OUTPUT);
  pinMode (ENA, OUTPUT);

  pinMode (CTRL, OUTPUT);
  //start the motor, turn CCW, untill we hit the startSwitch, then turn off motor. 
  //now we have te motor in the "0" position
  homeDial();
  Serial.println("Finished homeDial");

}


/////////////////////////////////// LOOP //////////////////////////////
void loop() {
if (!mqttClient.connected()) {
  reconnect();
  }
  
 mqttClient.loop();

  //  checkForManualHoming(); - ideas to force rehoming by pressing both swithces at once
  // every 100 price changes we rehome the motor. this could also be done with NTP
  // and schedueled at a specific time


  // get price from mqtt
  //price variable is set in the callback() function.
  // save the previous price
  
  
/*
  // This is used for testing
  //read value input from serial
  double temp = readVarFromSerial();
  
  if (temp > 0.0001 ) { // default read is 0, so we need to check for 0, so that we do nnot flip flop
  price = temp;
  Serial.print("Read temp = ");
  Serial.println(temp);
  }
*/

       

  //number of stepps we have to make:
  // we need to scale the price to the number of steps in circle.
  // we will do 0-50$, so that we can do 50,100,150 for price, and use the leds to show which
  int delta = priceToPosition(price) - priceToPosition(lastPrice);
  
  // change the lamps, in case we dont need to more (eg 20->70$)
  priceToLeds(price);


  //Is there a price difference from the last time we got a value?
  if ( delta != 0 ){
    
  
    //rehome every X price changes
    if (changes > 10) {
    homeDial();
    up(priceToPosition(lastPrice)); //set the position back to lastPrice, so that we can use the delta
    changes = 0;
    }
    
  changes++; // increment the counter of the number of price changes

  Serial.print("Old price = ");
  Serial.println(lastPrice);
  Serial.print("Recieved new price = ");
  Serial.println(price);
  
  Serial.print("delta from recieved value= ");
  Serial.println(delta);

  // change the LEDs
  priceToLeds(price);
  
  startMotor();
  
  //while the delta between the previous state and new state is not 0
  // if are moving up to new val
  if(delta > 0){
    Serial.print("Going up. Delta = ");
    Serial.println(delta);
    up(delta);
  }
  else {
    Serial.print("Going down. Delta = ");
    Serial.println(delta);
    down(abs(delta));
   }
   delta = 0;        //reset delta
   lastPrice = price; //store the prica as last price, so we can detect a change

   
  stopMotor(); // we are done moving, wait for the next value
  } // end delta != 0
} //end loop

void priceToLeds(double price){
   clearLeds();

  if (price <= 50.0 ) {
    leds[0] = ColorFromPalette (palette, 95);
  }

  if (price > 50.0 && price <= 100.0 ) {
    leds[0] = ColorFromPalette (palette, 95);
    leds[1] = ColorFromPalette (palette, 130);
  }
  
  if (price > 100.0 && price <= 150.0 ) {
    leds[0] = ColorFromPalette (palette, 95);
    leds[1] = ColorFromPalette (palette, 130);
    leds[2] = ColorFromPalette (palette, 160);

    }

    if (price > 150.0 ) {
    leds[0] = ColorFromPalette (palette, 160);
    leds[1] = ColorFromPalette (palette, 160);
    leds[2] = ColorFromPalette (palette, 160);
    }

   FastLED.show();
}

int priceToPosition(double price){
  double absPrice;
  
  //price between 0 - 50$
  if (price < 50.0 ) {
    absPrice = price;
  }
  if (price > 50.0 && price < 100.0 ) {
    absPrice = price - 50;
  }
  if (price > 100.0 && price < 150.0 ) {
    absPrice = price - 100;
  }
  
  return (int) absPrice * (stepCountPerRevolution / 50);
}

boolean up(int steps){
    digitalWrite(DIR,LOW); //set directiop clockwise ( up in value)
    digitalWrite(ENA,HIGH); //unlock motor

  while (steps > 0) {
    if(digitalRead(endSwitchPin) == 1){ //make sure we do not move past the end stop
      digitalWrite(PUL,HIGH);
      delayMicroseconds(25);
      digitalWrite(PUL,LOW);
      delayMicroseconds(25);
      Serial.print("Moving up. Steps to go: ");
      Serial.println(steps);
      steps--;  
      yield(); 
      }
   else {
    int i = 0;
      while(digitalRead(endSwitchPin) == 0){
        down(1);
        i++;
        Serial.print("End switch triggered. Moving down. Step : ");
        Serial.println(steps);
        } // we moved back sufficiently
      return false; // We tripped the end switch and are exiting.
    }
  }  
  return true;
}


//Function moving the dial down. Exits true if the end stop was not reached. false when endstop was triggered
boolean down(int steps){
    digitalWrite(DIR,HIGH); //set directiop counter-clockwise ( down in value)
    digitalWrite(ENA,HIGH); //unlock motor

  while (steps > 0) {
    if(digitalRead(startSwitchPin) == 1){ //going down, make sure we dont go past the start
      digitalWrite(PUL,HIGH);
      delayMicroseconds(25);
      digitalWrite(PUL,LOW);
      delayMicroseconds(25);
      Serial.print("Moving down. Steps to go: ");
      Serial.println(steps);
      steps--;
      yield();
      }
    else {
    // we hit the start stop, we went too far down
        int i = 0;
        while (digitalRead(startSwitchPin) == 0){
          up(1);
          i++;
          Serial.print("Start switch triggered. Moving up. Step :");
          Serial.println(i);
          yield();
          } // we moved back sufficiently
          return false; // We tripped start switch and are exiting.
        }
  }
  return true;
}

void stopMotor(){
  // These are commands sent to ENA, but then the motor keeps on zooming and using power.
  //digitalWrite(ENA,LOW); //lock motor
  //digitalWrite(ENA,HIGH); //unlock  motor
  digitalWrite(CTRL,LOW); // disable the controller the controller
  //Serial.println("Controller Disabled !");
}

void startMotor(){
  digitalWrite(CTRL,HIGH); // disable the controller the controller
  //Serial.println("Controller Enabled!");
  
}

void homeDial(){
  startMotor();
  Serial.println("Start dial homing");

  while(down(stepCountPerRevolution)){
      Serial.println("Finished dial homing");
    };  
  stopMotor();
}

void calibrate(){
  startMotor();
  Serial.println("Performing calibration");
  while(digitalRead(startSwitchPin) == 1){ // go CCW untill we hit the switch
      down(1);
      Serial.println("Going down...");
        yield();
  }
  while(digitalRead(startSwitchPin) == 0){ // move slowly forward untill the switch is off again
      up(10);
      delay(50);
      Serial.println("We hit the switch, backing up untill depressed");
  yield();
  }

  
  //lets start counting steps
  int count = 0;
  while(digitalRead(endSwitchPin) == 1){
    up(1);
    count++ ;
    Serial.print("Step count = ");
    Serial.println(count);
    yield();
  }
  while(digitalRead(endSwitchPin) == 0){
    down(1);
    count-- ;
    Serial.print("Decreasing step count = ");
    Serial.println(count);
    yield();
  }

    Serial.print("Final step count from stard to end = ");
    Serial.println(count);
  
}

double readVarFromSerial(){
  if (Serial.available()) {
    double x = (double) Serial.parseFloat();
    Serial.print(x);
    return x;
  }
}

//Set all leds to black
void clearLeds (){
for (int i = 0; i < NUM_LEDS; i++)
     {
       leds[i] = CRGB::Black;
      }
    FastLED.show ();
  
}


// A loop to run a calibration
void _loop() {
  calibrate();

  while(1){
    yield();
  }
  
}


void checkForManualHoming(){
if ( (digitalRead(endSwitchPin) == 1) && (digitalRead(startSwitchPin) == 1)){
  homeDial();
  }
}


void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i=0;i<length;i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  //Here we cast the 
  price = atof(reinterpret_cast<const char*>(payload));
  
}

void reconnect() {
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqttClient.connect("OilPriceClock")) {
      Serial.println("connected");
      // ... and resubscribe
      mqttClient.subscribe(MQTT_TOPIC);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}


