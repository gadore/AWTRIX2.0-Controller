#include <FS.h>
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <AutoConnect.h>
#include <ESP8266WebServer.h>     // Replace with WebServer.h for ESP32
#include <PubSubClient.h>
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
#include <Wire.h>   
#include <BME280_t.h>

typedef ESP8266WebServer  WiFiWebServer;

AutoConnect  portal;
AutoConnectConfig config;
WiFiClient   wifiClient;
PubSubClient mqttClient(wifiClient);

#define PARAM_FILE      "/param.json"
#define AUX_SETTING_URI "/AWTRIX_setting"
#define AUX_SAVE_URI    "/awtrix_save"

// instantiate BME sensor
BME280<> BMESensor;

bool tempState = false;

String version = "0.9b";

String  serverName;
bool  LDR;
bool DFPlayer; 
int  TempSensor; 
int  WIFI; 

bool firstStart = true;
int myTime;
int myCounter;
int TIME_FOR_SEARCHING_WIFI = 1;

//USB Connection:
byte myBytes[1000];
unsigned int bufferpointer;

#define LDR_RESISTOR 1000 //ohms
#define LDR_PIN A0
#define LDR_PHOTOCELL LightDependentResistor::GL5516

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

SoftwareSerial mySoftwareSerial(D7, D5); // RX, TX

CRGB leds[256];
#ifdef MATRIX_MODEV2
  FastLED_NeoMatrix *matrix = new FastLED_NeoMatrix(leds, 32, 8, NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_ROWS + NEO_MATRIX_ZIGZAG);
#else
  FastLED_NeoMatrix *matrix = new FastLED_NeoMatrix(leds, 32, 8, NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG);
#endif

// JSON definition of AutoConnectAux.
// Multiple AutoConnectAux can be defined in the JSON array.
// In this example, JSON is hard-coded to make it easier to understand
// the AutoConnectAux API. 
static const char AUX_AWTRIX_setting[] PROGMEM = R"raw(
[
  {
    "title": "AWTRIX Setting",
    "uri": "/AWTRIX_setting",
    "menu": true,
    "element": [
      {
        "name": "header",
        "type": "ACText",
        "value": "<h2>AWTRIX controller settings</h2>",
        "style": "text-align:center;color:#2f4f4f;padding:10px;"
      },
	  	      {
      "name": "text1",
      "type": "ACText",
      "value": "Connection",
      "style": "font-family:Arial;font-size:18px;font-weight:bold;color:#191970"
    },
      {
        "name": "ServerIP",
        "type": "ACInput",
        "value": "",
        "label": "Server",
        "pattern": "^(([a-zA-Z0-9]|[a-zA-Z0-9][a-zA-Z0-9\\-]*[a-zA-Z0-9])\\.)*([A-Za-z0-9]|[A-Za-z0-9][A-Za-z0-9\\-]*[A-Za-z0-9])$",
        "placeholder": "AWTRIX Server IP"
      },
	      {
        "name": "newline1",
        "type": "ACElement",
        "value": "<br>"
      },
	      {
        "name": "WIFIorUSB",
        "type": "ACRadio",
        "value": [
          "WiFi",
		  "Serial"
        ],
        "label": "Connection mode",
        "arrange": "vertical",
        "checked": 1
      },     
      {
        "name": "newline2",
        "type": "ACElement",
        "value": "<br>"
      },
	       {
        "name": "newline5",
        "type": "ACElement",
        "value": "<br>"
      },
	      {
      "name": "text2",
      "type": "ACText",
      "value": "Periphery",
      "style": "font-family:Arial;font-size:18px;font-weight:bold;color:#191970"
    },
      {
        "name": "LDR",
        "type": "ACCheckbox",
        "value": "unique",
        "label": "Use LDR",
        "checked": false
      },
	  {
        "name": "DFPLayer",
        "type": "ACCheckbox",
        "value": "unique",
        "label": "Use DFPLayer",
        "checked": false
      },
	      {
        "name": "newline3",
        "type": "ACElement",
        "value": "<br>"
      },

      {
        "name": "usedTempSensor",
        "type": "ACRadio",
        "value": [
		  "None",
          "BME280",
		  "HTU21D"
        ],
        "label": "Temperature Sensor",
        "arrange": "vertical",
        "checked": 1
      },
      {
        "name": "newline4",
        "type": "ACElement",
        "value": "<br>"
      },

      {
        "name": "save",
        "type": "ACSubmit",
        "value": "Save&amp;Start",
        "uri": "/awtrix_save"
      },
      {
        "name": "discard",
        "type": "ACSubmit",
        "value": "Discard",
        "uri": "/"
      }
    ]
  },
  {
    "title": "AWTRIX Setting",
    "uri": "/awtrix_save",
    "menu": false,
    "element": [
      {
        "name": "caption",
        "type": "ACText",
        "value": "<h4>Parameters saved as:</h4>",
        "style": "text-align:center;color:#2f4f4f;padding:10px;"
      },
      {
        "name": "parameters",
        "type": "ACText"
      },
    ]
  }
]
)raw";

void getParams(AutoConnectAux& aux) {
  serverName = aux["ServerIP"].value;
  serverName.trim();

  AutoConnectRadio& usedTempSensor = aux["usedTempSensor"].as<AutoConnectRadio>();
	if (usedTempSensor.value() == "BME280"){
		TempSensor = 1;
	}
	else if (usedTempSensor.value() == "HTU21D"){
		TempSensor = 2;
	}
	else{
		TempSensor = 0;
	}

  AutoConnectRadio& WIFIorUSB = aux["WIFIorUSB"].as<AutoConnectRadio>();
  	if (WIFIorUSB.value() == "WiFi"){
		WIFI = 0;
	}
	else {
		WIFI = 1;
	}

  LDR = aux["LDR"].as<AutoConnectCheckbox>().checked;
  DFPlayer = aux["DFPlayer"].as<AutoConnectCheckbox>().checked;
}

// Load parameters saved with  saveParams from SPIFFS into the
// elements defined in /AWTRIX_setting JSON.
String loadParams(AutoConnectAux& aux, PageArgument& args) {
  (void)(args);
  File param = SPIFFS.open(PARAM_FILE, "r");
  if (param) {
    if (aux.loadElement(param)) {
      getParams(aux);
      Serial.println(PARAM_FILE " loaded");
    }
    else
      Serial.println(PARAM_FILE " failed to load");
    param.close();
  }
  else {
    Serial.println(PARAM_FILE " open failed");
#ifdef ARDUINO_ARCH_ESP32
    Serial.println("If you get error as 'SPIFFS: mount failed, -10025', Please modify with 'SPIFFS.begin(true)'.");
#endif
  }
  return String("");
}

// Save the value of each element entered by '/AWTRIX_setting' to the
// parameter file. The saveParams as below is a callback function of
// /awtrix_save. When invoking this handler, the input value of each
// element is already stored in '/AWTRIX_setting'.
// In Sketch, you can output to stream its elements specified by name.
String saveParams(AutoConnectAux& aux, PageArgument& args) {
  // The 'where()' function returns the AutoConnectAux that caused
  // the transition to this page.
  AutoConnectAux&   AWTRIX_setting = *portal.aux(portal.where());
  getParams(AWTRIX_setting);
  AutoConnectInput& ServerIP = AWTRIX_setting["ServerIP"].as<AutoConnectInput>();

  // The entered value is owned by AutoConnectAux of /AWTRIX_setting.
  // To retrieve the elements of /AWTRIX_setting, it is necessary to get
  // the AutoConnectAux object of /AWTRIX_setting.
  File param = SPIFFS.open(PARAM_FILE, "w");
  AWTRIX_setting.saveElement(param, { "ServerIP", "LDR","DFPlayer", "usedTempSensor", "WIFIorUSB" "hostname"});
  param.close();

  // Echo back saved parameters to AutoConnectAux page.
  AutoConnectText&  echo = aux["parameters"].as<AutoConnectText>();
  echo.value = "Server: " + serverName;
  echo.value += ServerIP.isValid() ? String(" (OK)<br>") : String(" (ERR)<br>") ;
  echo.value += "Temperatur Sensor: " + String(TempSensor)+ "<br>";
  echo.value += "Connection via: " + String(WIFI)+ "<br>";
  echo.value += "Use LDR: " + String(LDR == true ? "true" : "false") + "<br>";
  echo.value += "Use DFPlayer: " + String(DFPlayer == true ? "true" : "false") + "<br>";


  return String("");
}

void handleRoot() {
  String  content =
    "<html>"
    "<head>"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
    "</head>"
    "<body>"
    "<iframe width=\"450\" height=\"260\" style=\"transform:scale(0.79);-o-transform:scale(0.79);-webkit-transform:scale(0.79);-moz-transform:scale(0.79);-ms-transform:scale(0.79);transform-origin:0 0;-o-transform-origin:0 0;-webkit-transform-origin:0 0;-moz-transform-origin:0 0;-ms-transform-origin:0 0;border: 1px solid #cccccc;\" src=\"https://thingspeak.com/channels/454951/charts/1?bgcolor=%23ffffff&color=%23d62020&dynamic=true&type=line\"></iframe>"
    "<p style=\"padding-top:5px;text-align:center\">" AUTOCONNECT_LINK(COG_24) "</p>"
    "</body>"
    "</html>";

  WiFiWebServer&  webServer = portal.host();
  webServer.send(200, "text/html", content);
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
  for (int i = 0; i < s.length(); i++){
    c = utf8ascii(s.charAt(i));
    if (c != 0) r += c;
  }
  return r;
}

void hardwareAnimatedCheck(int typ,int x,int y){
	int wifiCheckTime = millis();
	int wifiCheckPoints = 0;
	while(millis()-wifiCheckTime<2000){
		while(wifiCheckPoints<7){
			matrix->clear();
			
			if(typ==0){
				matrix->setCursor(7, 6);
				matrix->print("WiFi");
			} else if(typ==1){
				matrix->setCursor(1, 6);
				matrix->print("Server");
			}
			switch(wifiCheckPoints){
				case 6:
					matrix->drawPixel(x,y,0x07E0);
				case 5:
					matrix->drawPixel(x-1,y+1,0x07E0);
				case 4:
					matrix->drawPixel(x-2,y+2,0x07E0);
				case 3:
					matrix->drawPixel(x-3,y+3,0x07E0);
				case 2:
					matrix->drawPixel(x-4,y+4,0x07E0);
				case 1:
					matrix->drawPixel(x-5,y+3,0x07E0);
				case 0:
					matrix->drawPixel(x-6,y+2,0x07E0);
				break;
				}
			wifiCheckPoints++;
			matrix->show();
			delay(100);
		}
	}
}

void hardwareAnimatedSearchFast(int rounds,int x,int y){
	matrix->clear();
	matrix->setTextColor(0xFFFF);
	matrix->setCursor(1, 6);
	matrix->print("Server");

	switch(rounds){
		case 3:
			matrix->drawPixel(x,y,0xFFFF);
			matrix->drawPixel(x+1,y+1,0xFFFF);
			matrix->drawPixel(x+2,y+2,0xFFFF);
			matrix->drawPixel(x+3,y+3,0xFFFF);
			matrix->drawPixel(x+2,y+4,0xFFFF);
			matrix->drawPixel(x+1,y+5,0xFFFF);
			matrix->drawPixel(x,y+6,0xFFFF);
		case 2:
			matrix->drawPixel(x-1,y+2,0xFFFF);
			matrix->drawPixel(x,y+3,0xFFFF);
			matrix->drawPixel(x-1,y+4,0xFFFF);
		case 1: 
			matrix->drawPixel(x-3,y+3,0xFFFF);
		case 0: 
		break;	
	}	
	matrix->show();
}

void hardwareAnimatedSearch(int typ,int x,int y){
	for(int i=0;i<4;i++){
		matrix->clear();
		matrix->setTextColor(0xFFFF);
		if(typ==0){
			matrix->setCursor(7, 6);
			matrix->print("WiFi");
		} else if(typ==1){
			matrix->setCursor(1, 6);
			matrix->print("Server");
		}
		switch(i){
			case 3:
				matrix->drawPixel(x,y,0xFFFF);
				matrix->drawPixel(x+1,y+1,0xFFFF);
				matrix->drawPixel(x+2,y+2,0xFFFF);
				matrix->drawPixel(x+3,y+3,0xFFFF);
				matrix->drawPixel(x+2,y+4,0xFFFF);
				matrix->drawPixel(x+1,y+5,0xFFFF);
				matrix->drawPixel(x,y+6,0xFFFF);
			case 2: 
				matrix->drawPixel(x-1,y+2,0xFFFF);
				matrix->drawPixel(x,y+3,0xFFFF);
				matrix->drawPixel(x-1,y+4,0xFFFF);
			case 1: 
				matrix->drawPixel(x-3,y+3,0xFFFF);
			case 0: 
			break;	
		}	
		matrix->show();
		delay(500);	
	}
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

int GetRSSIasQuality(int rssi){
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

void updateMatrix(byte payload[],int length){
	int y_offset = 5;

	if(firstStart){
		hardwareAnimatedCheck(1,30,2);
		firstStart=false;
	}
	Serial.println(payload[0]);
	switch(payload[0]){
		case 0:{
			//Command 0: DrawText

			//Prepare the coordinates
			uint16_t x_coordinate = int(payload[1]<<8)+int(payload[2]);
			uint16_t y_coordinate = int(payload[3]<<8)+int(payload[4]);

			matrix->setCursor(x_coordinate+1, y_coordinate+y_offset);
			matrix->setTextColor(matrix->Color(payload[5],payload[6],payload[7]));
		
			String myText = "";
			for(int i = 8;i<length;i++){
				char c = payload[i];
				myText += c;
			}
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
		case 12:{
			//Command 12: GetMatrixInfo
			StaticJsonBuffer<200> jsonBuffer;
			JsonObject& root = jsonBuffer.createObject();
			root["type"] = "MatrixInfo";
			root["version"] = version;
			root["wifirssi"] = String(WiFi.RSSI());
			root["wifiquality"] =GetRSSIasQuality(WiFi.RSSI());
			root["wifissid"] =WiFi.SSID();
			root["getIP"] =WiFi.localIP().toString();
			String JS;
			root.printTo(JS);
			if (WIFI==0){
				mqttClient.publish("matrixClient", JS.c_str());
			} else {
				Serial.println(String(JS));
			}
			break;
		}
		case 13:{
  			matrix->setBrightness(payload[1]);
			break;
  		}
	}
}

void callback(char *topic, byte *payload, unsigned int length){
	updateMatrix(payload,length);
}

void reconnect(){
	if(WIFI==0){
		while (!mqttClient.connected()){
			String clientId = "AWTRIXController-";
			clientId += String(random(0xffff), HEX);
			hardwareAnimatedSearch(1,28,0);
			if (mqttClient.connect(clientId.c_str())){
				mqttClient.subscribe("awtrixmatrix/#");
				mqttClient.publish("matrixClient", "connected");
			}
		}
	}
}

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
			StaticJsonBuffer<200> jsonBuffer;
			JsonObject& root = jsonBuffer.createObject();
			root["type"] = "Gesture";
			root["gesture"] = control;
			String JS;
			root.printTo(JS);
		if (WIFI==0){
			mqttClient.publish("matrixClient", JS.c_str());
		}else{	
			Serial.println(String(JS));
		}		
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

bool startCP(IPAddress ip) {
	matrix->clear();
	matrix->setCursor(3, 6);
	matrix->setTextColor(matrix->Color(0, 255, 0));
	matrix->print("Hotspot");
	matrix->show();
  return true;
}

void setup(){
	delay(1000);
	FastLED.addLeds<NEOPIXEL, D2>(leds, 256).setCorrection(TypicalLEDStrip);
	matrix->begin();
	matrix->setTextWrap(false);
	matrix->setBrightness(80);
	matrix->setFont(&TomThumb);
	Serial.setRxBufferSize(1024);
	Serial.begin(115200);
	config.title= "AWTRIX Controller";
	config.apid= "AWTRIX Controller";
	config.apip=IPAddress(172,168,244,1);
	config.gateway=IPAddress(172,168,244,1);
 	portal.onDetect(startCP);
	SPIFFS.begin();
  if (portal.load(FPSTR(AUX_AWTRIX_setting))) {
    AutoConnectAux& AWTRIX_setting = *portal.aux(AUX_SETTING_URI);
    PageArgument  args;
    loadParams(AWTRIX_setting, args);
    config.bootUri = AC_ONBOOTURI_HOME;
    config.homeUri = "/";
    portal.config(config);
    portal.on(AUX_SETTING_URI, loadParams);
    portal.on(AUX_SAVE_URI, saveParams);
  }
  else
    Serial.println("load error");

  Serial.print("WiFi ");
  if (portal.begin()) {
    Serial.println("connected:" + WiFi.SSID());
    Serial.println("IP:" + WiFi.localIP().toString());
	
  }
  else {
    Serial.println("connection failed:" + String(WiFi.status()));
    Serial.println("Needs WiFi connection to start publishing messages");
  }

 	WiFiWebServer&  webServer = portal.host();
 	webServer.on("/", handleRoot);
	mySoftwareSerial.begin(9600);
	hardwareAnimatedCheck(0,27,2);
	mqttClient.setServer(serverName.c_str(), 7001);
	mqttClient.setCallback(callback);
	photocell.setPhotocellPositionOnGround(false);
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
	matrix->clear();
	matrix->setCursor(7,6);

	bufferpointer=0;
}

void loop() {
  ArduinoOTA.handle();
  portal.handleClient();

	if(firstStart){
		if(millis()-myTime>500){
			hardwareAnimatedSearchFast(myCounter,28,0);
			myCounter++;
			if(myCounter==4){
				myCounter=0;
			}
			myTime = millis();
		}
		firstStart=false;
	}

 	if (!updating) {
	 	if(WIFI==1){
			while(Serial.available () > 0){
				myBytes[bufferpointer] = Serial.read();
				if ((myBytes[bufferpointer]==255)&&(myBytes[bufferpointer-1]==255)&&(myBytes[bufferpointer-2]==255)){
					updateMatrix(myBytes, bufferpointer);
					for(int i =0;i<bufferpointer;i++){
						myBytes[i]=0;
					}
					bufferpointer=0;
					break;
				} else {
					bufferpointer++;
				}
				if(bufferpointer==1000){
					bufferpointer=0;
				}
			}
		}
		else {
			if (!mqttClient.connected()){
				reconnect();
			}else{
				mqttClient.loop();
			}
		}
	if(isr_flag == 1) {
    detachInterrupt(APDS9960_INT);
    handleGesture();
    isr_flag = 0;
    attachInterrupt(APDS9960_INT, interruptRoutine, FALLING);
  }
}


}
