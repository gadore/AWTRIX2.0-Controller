#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <FastLED.h>
#include <FastLED_NeoMatrix.h>
#include <Fonts/TomThumb.h>
#include <LightDependentResistor.h>
#include <Wire.h>
#include <SparkFun_APDS9960.h>
#include "SoftwareSerial.h"
#include <DFPlayerMini_Fast.h>
#include "awtrix-conf.h"
#include <WiFiManager.h>

#define LEDPIN 2

String version = "0.9b"; 

#ifndef USB_CONNECTION
	WiFiClient espClient;
	PubSubClient client(espClient);
#endif

LightDependentResistor photocell(LDR_PIN, LDR_RESISTOR, LDR_PHOTOCELL);
#define APDS9960_INT    D6
#define APDS9960_SDA    D3
#define APDS9960_SCL    D1
SparkFun_APDS9960 apds = SparkFun_APDS9960();
volatile bool isr_flag = 0;

#ifndef ICACHE_RAM_ATTR
#define ICACHE_RAM_ATTR IRAM_ATTR
#endif
bool updating = false;
DFPlayerMini_Fast myMP3;

//SoftwareSerial mySoftwareSerial(D7, D5); // RX, TX

SoftwareSerial mySoftwareSerial(D5, D4); // RX, TX

CRGB leds[256];
#ifdef MATRIX_MODEV2
  FastLED_NeoMatrix *matrix = new FastLED_NeoMatrix(leds, 32, 8, NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_ROWS + NEO_MATRIX_ZIGZAG);
#else
  FastLED_NeoMatrix *matrix = new FastLED_NeoMatrix(leds, 32, 8, NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG);
#endif

//WiFiManager
bool shouldSaveConfig = false;
char ssid[40];
char pwd[40];


byte myBytes[1000];
int bufferpointer;
String message;

//callback notifying us of the need to save config
void saveConfigCallback () {
  	Serial.println("Should save config");
  	shouldSaveConfig = true;
}

static byte c1;  // Last character buffer
byte utf8ascii(byte ascii) {
  if ( ascii < 128 ) // Standard ASCII-set 0..0x7F handling
  { c1 = 0;
    return ( ascii );
  }
  // get previous input
byte last = c1;   // get last char
  c1 = ascii;       // remember actual character
  switch (last)     // conversion depending on first UTF8-character
  { case 0xC2: return  (ascii) - 34;  break;
    case 0xC3: return  (ascii | 0xC0) - 34;  break;
    case 0x82: if (ascii == 0xAC) return (0xEA);   
  }
  return  (0);
}

void debuggingWithMatrix(String text){
	matrix->setCursor(7, 6);
		matrix->clear();
		matrix->print(text);
		matrix->show();
}

String utf8ascii(String s) {
  String r = "";
  char c;
  for (int i = 0; i < s.length(); i++)
  {
    c = utf8ascii(s.charAt(i));
    if (c != 0) r += c;
  }
  return r;
}



void utf8ascii(char* s) {
  int k = 0;
  char c;
  for (int i = 0; i < strlen(s); i++)
  {
    c = utf8ascii(s[i]);
    if (c != 0)
      s[k++] = c;
  }
  s[k] = 0;
}


String GetChipID()
{
	return String(ESP.getChipId());
}

int GetRSSIasQuality(int rssi)
{
	int quality = 0;

	if (rssi <= -100)
	{
		quality = 0;
	}
	else if (rssi >= -50)
	{
		quality = 100;
	}
	else
	{
		quality = 2 * (rssi + 100);
	}
	return quality;
}

unsigned long startTime = 0;
unsigned long endTime = 0;
unsigned long duration;

#ifndef USB_CONNECTION
void callback(char *topic, byte *payload, unsigned int length)
{
	int y_offset = 5;

	switch(payload[0]){
		case 0:{
			//Command 0: DrawText

			//Prepare the coordinates
			uint16_t x_coordinate = int(payload[1]<<8)+int(payload[2]);
			uint16_t y_coordinate = int(payload[3]<<8)+int(payload[4]);

			Serial.printf("X: %d - Y: %d\n",x_coordinate,y_coordinate);

			matrix->setCursor(x_coordinate+1, y_coordinate+y_offset);
			matrix->setTextColor(matrix->Color(payload[5],payload[6],payload[7]));
		
			String myText = "";
			char myChar;
			for(int i = 8;i<length;i++){
				char c = payload[i];
				myText += c;
			}
			Serial.printf("Text: %s\n",myText.c_str());
			matrix->print(utf8ascii(myText));
			break;
		}
		 
		case 1:{
			//Command 1: DrawBMP

			//Prepare the coordinates
			uint16_t x_coordinate = int(payload[1]<<8)+int(payload[2]);
			uint16_t y_coordinate = int(payload[3]<<8)+int(payload[4]);

			int16_t width = payload[5];
			int16_t height = payload[6];
	
			unsigned short colorData[width*height];

			for(int i = 0; i<width*height*2; i++){
				colorData[i/2] = (payload[i+7]<<8)+payload[i+1+7];
				i++;
			}
			
			for (int16_t j = 0; j < height; j++, y_coordinate++){
				for (int16_t i = 0; i < width; i++){
					matrix->drawPixel(x_coordinate + i, y_coordinate, (uint16_t)colorData[j*width+i]);
				}
			}
			break;
		}
		
		case 2:{
			//Command 2: DrawCircle

			//Prepare the coordinates
			uint16_t x0_coordinate = int(payload[1]<<8)+int(payload[2]);
			uint16_t y0_coordinate = int(payload[3]<<8)+int(payload[4]);
			uint16_t radius = payload[5];
			matrix->drawCircle(x0_coordinate, y0_coordinate, radius, matrix->Color(payload[6], payload[7], payload[8]));
			break;
		}
		case 3:{
			//Command 3: FillCircle

			//Prepare the coordinates
			uint16_t x0_coordinate = int(payload[1]<<8)+int(payload[2]);
			uint16_t y0_coordinate = int(payload[3]<<8)+int(payload[4]);
			uint16_t radius = payload[5];
			matrix->fillCircle(x0_coordinate, y0_coordinate, radius, matrix->Color(payload[6], payload[7], payload[8]));
			break;
		}
		case 4:{
			//Command 4: DrawPixel

			//Prepare the coordinates
			uint16_t x0_coordinate = int(payload[1]<<8)+int(payload[2]);
			uint16_t y0_coordinate = int(payload[3]<<8)+int(payload[4]);
			matrix->drawPixel(x0_coordinate, y0_coordinate, matrix->Color(payload[5], payload[6], payload[7]));
			break;
		}
		case 5:{
			//Command 5: DrawRect

			//Prepare the coordinates
			uint16_t x0_coordinate = int(payload[1]<<8)+int(payload[2]);
			uint16_t y0_coordinate = int(payload[3]<<8)+int(payload[4]);
			int16_t width = payload[5];
			int16_t height = payload[6];

			matrix->drawRect(x0_coordinate, y0_coordinate, width, height, matrix->Color(payload[7], payload[8], payload[9]));
			break;
		}
		case 6:{
			//Command 6: DrawLine

			//Prepare the coordinates
			uint16_t x0_coordinate = int(payload[1]<<8)+int(payload[2]);
			uint16_t y0_coordinate = int(payload[3]<<8)+int(payload[4]);
			uint16_t x1_coordinate = int(payload[5]<<8)+int(payload[6]);
			uint16_t y1_coordinate = int(payload[7]<<8)+int(payload[8]);
			matrix->drawLine(x0_coordinate, y0_coordinate, x1_coordinate, y1_coordinate, matrix->Color(payload[9],payload[10],payload[11]));
			break;
		}

		case 7:{
			//Command 7: FillMatrix

			matrix->fillScreen(matrix->Color(payload[1],payload[2],payload[3]));
			break;
		}

		case 8:{
			//Command 8: Show
			matrix->show();
			break;
		}
		case 9:{
			//Command 9: Clear
			matrix->clear();
			break;
		}
		case 10:{
			//Command 10: Play
			myMP3.volume(payload[3]);
			delay(10);
			myMP3.playFolder(payload[1],payload[2]);
			break;
		}
		case 11:{
			//Command 11: GetLux
			client.publish("matrixLux", String(photocell.getCurrentLux()).c_str());
			break;
		}
		case 12:{
			//Command 12: GetMatrixInfo
			StaticJsonBuffer<200> jsonBuffer;
			JsonObject& root = jsonBuffer.createObject();
			root["version"] = version;
			root["wifirssi"] = String(WiFi.RSSI());
			root["wifiquality"] =GetRSSIasQuality(WiFi.RSSI());
			root["wifissid"] =WiFi.SSID();
			root["getIP"] =WiFi.localIP().toString();
			String JS;
			root.printTo(JS);
			client.publish("matrixInfo", JS.c_str());
			break;
		}
	}
}

void reconnect()
{
	while (!client.connected())
	{
		String clientId = "AWTRIXController-";
    clientId += String(random(0xffff), HEX);
		if (client.connect(clientId.c_str()))
		{
			client.subscribe("awtrixmatrix/#");
			client.publish("matrixstate", "connected");
		}
		else
		{
			delay(5000);
		}
	}
}
#else
void processing(int length)
{
int y_offset = 5;

debuggingWithMatrix("gefunden...");

byte payload[length];
for(int i=0; i<length;i++){
	payload[i] = message[i];
}

	switch(payload[0]){
		case 0:{
			//Command 0: DrawText

			//Prepare the coordinates
			uint16_t x_coordinate = int(payload[3]<<8)+int(payload[4]);
			uint16_t y_coordinate = int(payload[5]<<8)+int(payload[6]);

			//Serial.printf("X: %d - Y: %d\n",x_coordinate,y_coordinate);

			matrix->setCursor(x_coordinate+1, y_coordinate+y_offset);
			matrix->setTextColor(matrix->Color(payload[7],payload[8],payload[9]));
		
			String myText = "";
			char myChar;
			for(int i = 10;i<length;i++){
				char c = payload[i];
				myText += c;
			}
			//Serial.printf("Text: %s\n",myText.c_str());
			matrix->print(utf8ascii(myText));
			break;
		}
		case 1:{
			//Command 1: DrawBMP

			//Prepare the coordinates
			uint16_t x_coordinate = int(payload[1]<<8)+int(payload[2]);
			uint16_t y_coordinate = int(payload[3]<<8)+int(payload[4]);

			int16_t width = payload[5];
			int16_t height = payload[6];
	
			unsigned short colorData[width*height];
			for(int i = 0; i<width*height*2; i++){
				colorData[i/2] = payload[i+7]<<8+payload[i+1+8];
				i++;
			}
			
			for (int16_t j = 0; j < height; j++, y_coordinate++){
				for (int16_t i = 0; i < width; i++){
					matrix->drawPixel(x_coordinate + i, y_coordinate, (uint16_t)colorData[j*width+i]);
				}
			}
			break;
		}

		case 2:{
			//Command 2: DrawCircle

			//Prepare the coordinates
			uint16_t x0_coordinate = int(payload[3]<<8)+int(payload[4]);
			uint16_t y0_coordinate = int(payload[5]<<8)+int(payload[6]);
			uint16_t radius = payload[7];
			matrix->drawCircle(x0_coordinate, y0_coordinate, radius, matrix->Color(payload[8], payload[9], payload[10]));
			break;
		}
		case 3:{
			//Command 3: FillCircle

			//Prepare the coordinates
			uint16_t x0_coordinate = int(payload[3]<<8)+int(payload[4]);
			uint16_t y0_coordinate = int(payload[5]<<8)+int(payload[6]);
			uint16_t radius = payload[7];
			matrix->fillCircle(x0_coordinate, y0_coordinate, radius, matrix->Color(payload[8], payload[9], payload[10]));
			break;
		}
		case 4:{
			//Command 4: DrawPixel

			//Prepare the coordinates
			uint16_t x0_coordinate = int(payload[3]<<8)+int(payload[4]);
			uint16_t y0_coordinate = int(payload[5]<<8)+int(payload[6]);
			matrix->drawPixel(x0_coordinate, y0_coordinate, matrix->Color(payload[7], payload[8], payload[9]));
			break;
		}
		case 5:{
			//Command 5: DrawRect

			//Prepare the coordinates
			uint16_t x0_coordinate = int(payload[3]<<8)+int(payload[4]);
			uint16_t y0_coordinate = int(payload[5]<<8)+int(payload[6]);
			int16_t width = payload[7];
			int16_t height = payload[8];

			matrix->drawRect(x0_coordinate, y0_coordinate, width, height, matrix->Color(payload[9], payload[10], payload[11]));
			break;
		}
		case 6:{
			//Command 6: DrawLine

			//Prepare the coordinates
			uint16_t x0_coordinate = int(payload[3]<<8)+int(payload[4]);
			uint16_t y0_coordinate = int(payload[5]<<8)+int(payload[6]);
			uint16_t x1_coordinate = int(payload[7]<<8)+int(payload[8]);
			uint16_t y1_coordinate = int(payload[9]<<8)+int(payload[10]);
			matrix->drawLine(x0_coordinate, y0_coordinate, x1_coordinate, y1_coordinate, matrix->Color(payload[11],payload[12],payload[13]));
			break;
		}

		case 7:{
			//Command 7: FillMatrix

			matrix->fillScreen(matrix->Color(payload[3],payload[4],payload[5]));
			break;
		}

		case 8:{
			//Command 8: Show
			matrix->show();
			break;
		}
		case 9:{
			//Command 9: Clear
			matrix->clear();
			break;
		}
		case 10:{
			//Command 10: Play
			myMP3.volume(payload[5]);
			delay(10);
			myMP3.playFolder(payload[3],payload[4]);
			break;
		}
		case 11:{
			//Command 11: GetLux
			Serial.println(String(photocell.getCurrentLux()).c_str());
			break;
		}
		case 12:{
			//Command 12: GetMatrixInfo
			StaticJsonBuffer<200> jsonBuffer;
			JsonObject& root = jsonBuffer.createObject();
			root["version"] = version;
			root["wifirssi"] = String(WiFi.RSSI());
			root["wifiquality"] =GetRSSIasQuality(WiFi.RSSI());
			root["wifissid"] =WiFi.SSID();
			root["getIP"] =WiFi.localIP().toString();
			String JS;
			root.printTo(JS);
			Serial.println((String)JS);
			break;
		}
	}
}
#endif

void ICACHE_RAM_ATTR interruptRoutine() {
  isr_flag = 1;
}

void handleGesture() {
		String control;
    if (apds.isGestureAvailable()) {
    switch ( apds.readGesture() ) {
      case DIR_UP:
			control = "UP";
        break;
      case DIR_DOWN:
			control = "DOWN";
        break;
      case DIR_LEFT:
			control = "LEFT";
        break;
      case DIR_RIGHT:
				control = "RIGHT";
        break;
      case DIR_NEAR:
				control = "NEAR";
        break;
      case DIR_FAR:
				control = "FAR";
        break;
      default:
				control = "NONE";
    }
		#ifdef USB_CONNECTION
			StaticJsonBuffer<200> jsonBuffer;
			JsonObject& root = jsonBuffer.createObject();
			String JS;
			root.printTo(JS);
			Serial.println(String(JS));
		#else
			client.publish("control", control.c_str());
		#endif
  }
}

uint32_t Wheel(byte WheelPos, int pos) {
  if(WheelPos < 85) {
   return matrix->Color((WheelPos * 3)-pos, (255 - WheelPos * 3)-pos, 0);
  } else if(WheelPos < 170) {
   WheelPos -= 85;
   return matrix->Color((255 - WheelPos * 3)-pos, 0, (WheelPos * 3)-pos);
  } else {
   WheelPos -= 170;
   return matrix->Color(0, (WheelPos * 3)-pos, (255 - WheelPos * 3)-pos);
  }
}

void flashProgress(unsigned int progress, unsigned int total) {
    matrix->setBrightness(100);   
    long num = 32 * 8 * progress / total;
    for (unsigned char y = 0; y < 8; y++) {
        for (unsigned char x = 0; x < 32; x++) {
            if (num-- > 0) matrix->drawPixel(x, 8 - y - 1, Wheel((num*16) & 255,0));
        }
    }
    matrix->setCursor(0, 6);
		matrix->setTextColor(matrix->Color(255, 255, 255));
    matrix->print("FLASHING");
    matrix->show();
}

void setup()
{
	pinMode(D5, OUTPUT);

	#ifndef USB_CONNECTION
		Serial.begin(9600);
	#endif
	mySoftwareSerial.begin(9600);
	FastLED.addLeds<NEOPIXEL, D2>(leds, 256).setCorrection(TypicalLEDStrip);
	Serial.println("Hey, I´m your Awtrix!\n");
	WiFiManager wifiManager;
	WiFi.mode(WIFI_STA);
	WiFi.begin(wifiConfig.ssid, wifiConfig.password);
	matrix->begin();
	matrix->setTextWrap(false);
	matrix->setBrightness(80);
	matrix->setFont(&TomThumb);
	matrix->setCursor(7, 6);
	matrix->print("WiFi...");
	matrix->show();
	while (WiFi.status() != WL_CONNECTED)
	{
		delay(3000);
		wifiManager.autoConnect("AwtrixWiFiSetup");
	}

	matrix->clear();
	matrix->setCursor(6, 6);
	matrix->setTextColor(matrix->Color(0,255,0));
	matrix->print("Ready!");
	matrix->show();
	photocell.setPhotocellPositionOnGround(false);

 #ifdef USB_CONNECTION
	Serial.begin(115200);
 #else
	client.setServer(wifiConfig.awtrix_server, 7001);
	client.setCallback(callback);
 #endif

	myMP3.begin(mySoftwareSerial);

	Wire.begin(APDS9960_SDA,APDS9960_SCL);
  	pinMode(APDS9960_INT, INPUT);
	attachInterrupt(APDS9960_INT, interruptRoutine, FALLING);
  	apds.init();
  	apds.enableGestureSensor(true);
  	ArduinoOTA.onStart([&]() {
		updating = true;
		matrix->clear();
  	});

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    	flashProgress(progress, total);
  });

  	ArduinoOTA.begin();
	#ifndef USB_CONNECTION
		client.publish("control", "Hallo Welt");
	#endif
	digitalWrite(D5,!digitalRead(D5));
	bufferpointer=0;
}



void loop() {
 ArduinoOTA.handle();
 if (!updating) {
	 #ifdef USB_CONNECTION
		//while (Serial.available () > 0) {
			//digitalWrite(D5,!digitalRead(D5));
			//String message= Serial.readStringUntil(':');
			//processing(sizeof(message));
			
			if(Serial.available () > 0){
				//debuggingWithMatrix("Hallo");

				myBytes[bufferpointer] = Serial.read();
				//digitalWrite(D5,!digitalRead(D5));
				if ((myBytes[bufferpointer]==255)&&(myBytes[bufferpointer-1]==255)&&(myBytes[bufferpointer-2]==255)){
					//debuggingWithMatrix("gefunden...");
					uint16_t x_coordinate = int(myBytes[3]<<8)+int(myBytes[4]);
					uint16_t y_coordinate = int(myBytes[5]<<8)+int(myBytes[6]);

					matrix->setCursor(x_coordinate+1, y_coordinate+5);
					matrix->setTextColor(matrix->Color(myBytes[7],myBytes[8],myBytes[9]));
		
					String myText = "";
					char myChar;
					for(int i = 10;i<bufferpointer;i++){
						char c = myBytes[i];
						myText += c;
					}
					//Serial.printf("Text: %s\n",myText.c_str());
					matrix->print(utf8ascii(myText));
					matrix->show();
					bufferpointer=0;
				}
				bufferpointer++;
				if(bufferpointer==1000){
					bufferpointer=0;
				}
			}
			
		//}
	#else
		if (!client.connected())
		{
			reconnect();
		}else{
			client.loop();
		}
	#endif
	if(isr_flag == 1) {
    detachInterrupt(APDS9960_INT);
    handleGesture();
    isr_flag = 0;
    attachInterrupt(APDS9960_INT, interruptRoutine, FALLING);
  }
}
}
