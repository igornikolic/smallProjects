#include "FastLED.h"
#include <ESP8266WiFi.h>
#include <WiFiManager.h>

#include <PubSubClient.h>
#define MQTT_HOST   "revspace.nl"
#define MQTT_PORT   1883
#define MQTT_TOPIC  "revspace/TennetBallansDeltaDispatch"


FASTLED_USING_NAMESPACE
#if defined(FASTLED_VERSION) && (FASTLED_VERSION < 3001000)
#warning "Requires FastLED 3.1 or later; check github for latest code."
#endif

//Hardware definition
#define DATA_PIN  D7
#define CLK_PIN   D5
#define LED_TYPE    APA102 //sk9822 - clone of apa102, almost completely compatible, but better
#define COLOR_ORDER RGB
#define NUM_LEDS    5

// Explanation of values used

// We are tracking the amount of MW that TenneT has  dispatched in the last minute for 
// grind ballancing. 
// This power is automatically dispatched using frequencey driven control.
// For details see http://www.tennet.org/english/operational_management/System_data_relating_implementation/system_balance_information/BalansDeltawithPrices.aspx#PanelTabDescription
//The latest read dispatch values :
int upDispatch;
int downDispatch;
int emergency;

// The strings for delimiting the relevant data in the XML 
// http://www.tennet.org/xml/balancedeltaprices/balans-delta.xml

//const String upStart = "<UPWARD_DISPATCH>";
//const String upStop = "</UPWARD_DISPATCH>";
//const String downStart = "<DOWNWARD_DISPATCH>";
//const String downStop = "</DOWNWARD_DISPATCH>";
//const String emergencyStart = "<EMERGENCY_POWER>";
//const String emergencyStop = "</EMERGENCY_POWER>";

//The delta = Up dispatch - down dispatch
int delta = 0;

//LED settings
CRGBPalette16 palette = RainbowColors_p;
CRGB leds[NUM_LEDS];
#define BRIGHTNESS         255
#define FRAMES_PER_SECOND  120
TBlendType currentBlending;

//WiFi settings
WiFiManager wifiManager;
WiFiClient wifiClient;
static char esp_id[16];
PubSubClient mqttClient(wifiClient);


void setup()
{
  delay (1000);			// 1 second delay for recovery
  Serial.begin (115200);
  WiFiManager wifiManager;
  wifiManager.setDebugOutput(true);
  wifiManager.autoConnect ("AutoConnectAP");
  Serial.println ("connected...yeey :)");
  Serial.println(WiFi.macAddress());

  // LED Setup
  // tell FastLED about the LED strip configuration
  FastLED.addLeds < LED_TYPE, DATA_PIN, CLK_PIN, COLOR_ORDER > (leds,NUM_LEDS).setCorrection (TypicalLEDStrip);
  // set master brightness control
  FastLED.setBrightness (BRIGHTNESS);



  //MQTT setup
  snprintf(esp_id, sizeof(esp_id), "%08x", ESP.getChipId());
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setCallback(callback);

}


void loop (){
  if (!mqttClient.connected()) {
  reconnect();
  }
  mqttClient.loop();
 

  

//For color, we start in the middle of the palette. 
//if positive, start in middle, and go up
//if negative, start in middle and go down
// We assume 250 MIN/MAX MW up down, above that it is messed up

//The colorIndex that determines the color,is a uint_t 8 bit variable, 
// so it can only hold 0-255

  int val = 128;		// middle of the range 0-255so we can go up or down. we will later put 

//upAndDownDispatched;
  bool noOverload = true;

  if (delta > 0)
    {
      val = val + delta / 2;
      if (val > 255)
	      {
	        val = 255;
	        noOverload = false;
	      }
     }
  else
    {
      val = val - delta / 2;
      if (val < 0)
	      {
      	  val = 0;
      	  noOverload = true;
      	}

    }

  uint8_t colorIndex = val;

  //Serial.print ("colorIndex: ");
  //Serial.println (colorIndex);


  if (noOverload){

      for (int i = 0; i < NUM_LEDS; i++)
	    {
	    leds[i] = ColorFromPalette (palette, colorIndex);
    FastLED.show ();

	    }
    }				// no overload, normal light
  else				// yes overload, blink a bit
    {

      // Find a nice pulse effect
      for (int i = 0; i < NUM_LEDS; i++)
	    {
	     leds[i] = ColorFromPalette (palette, colorIndex, BRIGHTNESS / 4);
	    }
    FastLED.show ();

      delay (500);
      for (int i = 0; i < NUM_LEDS; i++)
	{
	  leds[i] = ColorFromPalette (palette, colorIndex, BRIGHTNESS);
	}
      FastLED.show ();

      delay (500);
      
      for (int i = 0; i < NUM_LEDS; i++)
	{
	  leds[i] = ColorFromPalette (palette, colorIndex, BRIGHTNESS / 4);
    FastLED.show ();
	}
       for (int i = 0; i < NUM_LEDS; i++)
 {
    leds[i] = ColorFromPalette (palette, colorIndex, BRIGHTNESS );
     FastLED.show ();

  }

    }


  // send the 'leds' array out to the actual LED strip, in case we forgot it some
  FastLED.show ();


}				// end loop


void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i=0;i<length;i++) {
    Serial.print((char)payload[i]);
  }
  //Here we cast the 
  delta = atof(reinterpret_cast<const char*>(payload));
  Serial.print ("New delta recieved: ");
  Serial.println (delta);

  
}

void reconnect() {
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqttClient.connect(esp_id)) {
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


