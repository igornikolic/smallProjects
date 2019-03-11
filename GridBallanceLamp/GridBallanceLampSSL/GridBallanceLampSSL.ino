#include "FastLED.h"
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <WiFiClientSecure.h>


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

const String upStart = "<UPWARD_DISPATCH>";
const String upStop = "</UPWARD_DISPATCH>";
const String downStart = "<DOWNWARD_DISPATCH>";
const String downStop = "</DOWNWARD_DISPATCH>";
const String emergencyStart = "<EMERGENCY_POWER>";
const String emergencyStop = "</EMERGENCY_POWER>";


//LED settings
CRGBPalette16 palette = RainbowColors_p;
CRGB leds[NUM_LEDS];
#define BRIGHTNESS         255
#define FRAMES_PER_SECOND  120
TBlendType currentBlending;

//WiFi settings
WiFiManager wifiManager;
WiFiClientSecure wifiClient;
static char esp_id[16];

//This does not work, because TenneT chose a type of certificate
// that does not suppot ciphers that the  library can handle.
// Left here if you want to use a different source 
const uint16_t port = 443;
const char *host = "www.tennet.org";  // ip or dns
//const char *host = "193.177.237.109";
//tennet
const char* fingerprint = "77 DB 8A D2 48 BF 68 A9 49 41 00 8A F1 17 E4 0A 9C 29 84 3C";


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

}


void loop (){

  //start network

  Serial.print ("connecting to ");
  Serial.print (host);
  Serial.print (" port ");
  Serial.println (port);

  // Use WiFiClient class to create TCP connections
   WiFiClientSecure  client;

  if (!client.connect (host, port))
    {
      Serial.println ("connection failed");
      Serial.println ("wait 30 sec...");
      delay (30000);
      return;
    }


  if (client.verify(fingerprint, host)) {
    Serial.println("certificate matches");
  } else {
    Serial.println("certificate doesn't match");
  }



  // This will send the request to the server

  // Send HTTP request
  client.println (F
		  ("GET /xml/balancedeltaprices/balans-delta_2h.xml HTTP/1.0"));
  client.println (F ("Host: www.tennet.org"));
  client.println (F ("Connection: close"));
  if (client.println () == 0)
    {
      Serial.println (F ("Failed to send request"));
      return;
    }

  // Check HTTP status
  char status[32] = { 0 };
  client.readBytesUntil ('\r', status, sizeof (status));
  if (strcmp (status, "HTTP/1.1 200 OK") != 0)
    {
      Serial.print (F ("Unexpected response: "));
      Serial.println (status);
      return;
    }

  // Skip HTTP headers
  char endOfHeaders[] = "\r\n\r\n";
  if (!client.find (endOfHeaders))
    {
      Serial.println (F ("Invalid response"));
      return;
    }
  String xmlFile;

  while (client.available ())
    {
 

      // Read till the EOF
      xmlFile = client.readStringUntil ('\0');
    } // end while connected

  // Disconnect the HTTP client
  client.stop ();

  //Lets get the values and print to the serial as well
  Serial.println ("---------------------------------");	// yay works

  //Up dispatch
  int indexUpStart = xmlFile.indexOf(upStart);

 
  int indexUpStop = xmlFile.indexOf(upStop);



  if (indexUpStart != -1 || indexUpStop != -1)
    {
      String upDispatchString = xmlFile.substring (indexUpStart + upStart.length(), indexUpStop);
      upDispatch = upDispatchString.toInt ();

      //Serial.print("upDispatch string: "); 
      //Serial.println(upDispatchString); 
      Serial.print ("upDispatch : ");
      Serial.println (upDispatch);
    }
  else
    {
      Serial.println (F ("No <UPWARD_DISPATCH> or </UPWARD_DISPATCH> string found!"));
      return;			// exit the loop 
    }

  // Down dispatch
  int indexDownStart = xmlFile.indexOf (downStart);
  int indexDownStop = xmlFile.indexOf (downStop);

  if (indexDownStart != -1 || indexDownStop != -1)
    {
      String downDispatchString =
	xmlFile.substring (indexDownStart + downStart.length (),
			   indexDownStop);
      downDispatch = downDispatchString.toInt ();
      Serial.print ("downDispatch :  ");
      Serial.println (downDispatch);
    }
  else
    {
      Serial.println (F("No <DOWNWARD_DISPATCH> or </DOWNWARD_DISPATCH> string found!"));
      return;	
    }

  //Emergency dispatch
  int indexEmergencyStart = xmlFile.indexOf (emergencyStart);
  int indexEmergencyStop = xmlFile.indexOf (emergencyStop);

  if (indexEmergencyStart != -1 || indexEmergencyStop != -1)
    {
      String emergencyString =
	xmlFile.substring (indexEmergencyStart + emergencyStart.length (),
			   indexEmergencyStop);
      emergency = emergencyString.toInt ();
      //Serial.print("Emergency string:  "); 
      //Serial.println(emergencyString); 
      Serial.print ("emergency :  ");
      Serial.println (emergency);
    }
  else
    {
      Serial.println (F
		      ("No <EMERGENCY_POWER> or </EMERGENCY_POWER> string found!"));
      return;			// exit the loop

    }

  int delta = upDispatch - downDispatch;
  Serial.print ("Delta: ");
  Serial.println (delta);

  bool upAndDownDispatched = false;
  if (upDispatch > 0 && downDispatch > 0)
    {
      upAndDownDispatched = true;
    }

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
      val = val + upDispatch / 2;
      if (val > 255)
	{
	  val = 255;
	  noOverload = false;
	}
    }
  else
    {
      val = val - downDispatch / 2;
      if (val < 0)
	{
	  val = 0;
	  noOverload = true;
	}

    }

  uint8_t colorIndex = val;

  Serial.print ("colorIndex: ");
  Serial.println (colorIndex);

if (upAndDownDispatched){
  
}

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

  delay (60000);		// We get the data once a minute


}				// end loop



