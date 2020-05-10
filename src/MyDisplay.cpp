#include <ESP8266WiFi.h>
#include <FastLED_NeoMatrix.h>
#include <Fonts/TomThumb.h>
#include <ArduinoJson.h>
 
#define led 2 //发光二极管连接在8266的GPIO2上
const char *ssid     = "danke";//这里写入网络的ssid
const char *password = "17521659186";//wifi密码
const char *host = "192.168.124.44";//修改为Server服务端的IP，即你电脑的IP，确保在同一网络之下。
 
WiFiClient client;
const int tcpPort = 6666;//修改为你建立的Server服务端的端口号，此端口号是创建服务器时指定的。
CRGB leds[256];
FastLED_NeoMatrix *matrix = new FastLED_NeoMatrix(leds, 32, 8, NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG);

void setup()
{
    FastLED.addLeds<NEOPIXEL, D2>(leds, 256).setCorrection(TypicalLEDStrip);
    Serial.begin(115200);    
    pinMode(led,OUTPUT);
    matrix->begin();
    matrix->setTextWrap(false);
	matrix->setBrightness(40);
    matrix->setFont(&TomThumb);
    matrix->clear();
    delay(10);

    matrix->print("WIFI...");
 
    WiFi.begin(ssid, password);//启动
 
     //在这里检测是否成功连接到目标网络，未连接则阻塞。
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
    }
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

void loop()
{
    while (!client.connected())//若未连接到服务端，则客户端进行连接。
    {
        if (!client.connect(host, tcpPort))//实际上这一步就在连接服务端，如果连接上，该函数返回true
        {
            delay(500);
            digitalWrite(led, HIGH);
            matrix->clear();
            matrix->setCursor(0,6);
            matrix->setBrightness(10);
            matrix->setTextColor(matrix->Color(255, 0, 0));
            matrix->print("Linking..");
            matrix->show();
            delay(500);
            digitalWrite(led, LOW);
        }else{
            matrix->clear();
            matrix->setBrightness(10);
            matrix->setCursor(0,6);
            matrix->setTextColor(matrix->Color(255, 255, 255));
            matrix->print("Linked!");
            matrix->show();
        }
    }
 
    while (client.available())//available()表示是否可以获取到数据
    {
        client.setTimeout(20);
        String val = client.readStringUntil('}') + '}';
        DynamicJsonBuffer jsonBuffer;
        JsonObject &json = jsonBuffer.parseObject(val);
        String type = json["type"];
        if (type.equals("show"))
        {
            matrix->show();
        }
        else if (type.equals("clear"))
        {
            matrix->clear();
        }
        else if (type.equals("drawText"))
        {
            if (json["font"].as<String>().equals("big"))
            {
                matrix->setFont();
                matrix->setCursor(json["x"].as<int16_t>(), json["y"].as<int16_t>() - 1);
            }
            else
            {
                matrix->setFont(&TomThumb);
                matrix->setCursor(json["x"].as<int16_t>(), json["y"].as<int16_t>() + 5);
            }
            matrix->setTextColor(matrix->Color(json["color"][0].as<int16_t>(), json["color"][1].as<int16_t>(), json["color"][2].as<int16_t>()));
            String text = json["text"];
            
            matrix->print(utf8ascii(text));
        }
        else if (type.equals("drawBMP"))
        {
            int16_t h = json["height"].as<int16_t>();
            int16_t w = json["width"].as<int16_t>();
            int16_t x = json["x"].as<int16_t>();
            int16_t y = json["y"].as<int16_t>();

            for (int16_t j = 0; j < h; j++, y++)
            {
                for (int16_t i = 0; i < w; i++)
                {
                    matrix->drawPixel(x + i, y, json["bmp"][j * w + i].as<int16_t>());
                }
            }
        }
        else if (type.equals("drawLine"))
        {
            matrix->drawLine(json["x0"].as<int16_t>(), json["y0"].as<int16_t>(), json["x1"].as<int16_t>(), json["y1"].as<int16_t>(), matrix->Color(json["color"][0].as<int16_t>(), json["color"][1].as<int16_t>(), json["color"][2].as<int16_t>()));
        }
        else if (type.equals("drawCircle"))
        {
            matrix->drawCircle(json["x0"].as<int16_t>(), json["y0"].as<int16_t>(), json["r"].as<int16_t>(), matrix->Color(json["color"][0].as<int16_t>(), json["color"][1].as<int16_t>(), json["color"][2].as<int16_t>()));
        }
        else if (type.equals("drawRect"))
        {
            matrix->drawRect(json["x"].as<int16_t>(), json["y"].as<int16_t>(), json["w"].as<int16_t>(), json["h"].as<int16_t>(), matrix->Color(json["color"][0].as<int16_t>(), json["color"][1].as<int16_t>(), json["color"][2].as<int16_t>()));
        }
            else if (type.equals("fill"))
        {
            matrix->fillScreen(matrix->Color(json["color"][0].as<int16_t>(), json["color"][1].as<int16_t>(), json["color"][2].as<int16_t>()));
        }
        else if (type.equals("drawPixel"))
        {
            matrix->drawPixel(json["x"].as<int16_t>(), json["y"].as<int16_t>(), matrix->Color(json["color"][0].as<int16_t>(), json["color"][1].as<int16_t>(), json["color"][2].as<int16_t>()));
        }
        else if (type.equals("setBrightness"))
        {
            matrix->setBrightness(json["brightness"].as<int16_t>());
        }
        //     else if (type.equals("play"))
        // {
        //     myMP3.volume(json["vol"].as<int8>());
        //     delay(20);
        //     myMP3.playFolder(json["folder"].as<int8>(),json["file"].as<int8>());
        // }
        // else if (type.equals("getMatrixInfo"))
        // {
        //     StaticJsonBuffer<200> jsonBuffer;
        //     JsonObject& root = jsonBuffer.createObject();
        //     root["version"] = version;
        //     root["wifirssi"] = String(WiFi.RSSI());
        //     root["wifiquality"] =GetRSSIasQuality(WiFi.RSSI());
        //     root["wifissid"] =WiFi.SSID();
        //     root["getIP"] =WiFi.localIP().toString();
        //     String JS;
        //     root.printTo(JS);
        //     Serial.println(String(JS));
        // }
        // else if (type.equals("getLUX"))
        // {
        //     StaticJsonBuffer<200> jsonBuffer;
        //     JsonObject& root = jsonBuffer.createObject();
        //     root["LUX"] = photocell.getCurrentLux();
        //     String JS;
        //     root.printTo(JS);
        //     Serial.println(String(JS));
        // }
    }
}
