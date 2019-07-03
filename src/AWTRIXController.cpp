#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <FS.h>
#include <AutoConnect.h>

#define PARAM_FILE      "/param.json"
#define AUX_SETTING_URI "/AWTRIX_setting"
#define AUX_SAVE_URI    "/awtrix_save"

typedef ESP8266WebServer  WiFiWebServer;

AutoConnect  portal;
AutoConnectConfig config;
WiFiClient   wifiClient;

String  serverName;
bool  LDR;
bool DFPlayer; 
int  TempSensor; // String "BME280" oder "HTU21D"
int  WIFI; // String "USB" oder "WiFi"


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
	else if (usedTempSensor.value() == "BME280"){
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


void setup() {
  delay(1000);
  Serial.begin(115200);
  Serial.println();
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
}

void loop() {
  portal.handleClient();
}
