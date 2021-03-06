#include <ESP8266WiFi.h>
#include <FastLED_NeoMatrix.h>
#include <Fonts/TomThumb.h>
#include <ArduinoJson.h>
#include <WebSocketClient.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>

#define led 2                     //发光二极管连接在8266的GPIO2上
const char *ssid = "KUBERNETES-ARG"; //这里写入网络的ssid
const char *password = "simba-server";
char *host = "192.168.103.87";//修改为Server服务端的IP，即你电脑的IP，确保在同一网络之下。
// char *host = "192.168.3.209";
const int tcpPort = 10663; //修改为你建立的Server服务端的端口号，此端口号是创建服务器时指定的。

struct WifiConfig
{
    String name;
    String pwd;
    String host;
};

WiFiClient client;
WebSocketClient webSocketClient;

CRGB leds[256];
FastLED_NeoMatrix *matrix = new FastLED_NeoMatrix(leds, 32, 8, NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG);

void printMsg(char *msg)
{
    matrix->clear();
    matrix->setBrightness(10);
    matrix->setCursor(0, 6);
    matrix->setTextColor(matrix->Color(255, 255, 255));
    matrix->print(msg);
    matrix->show();
}

void printStr(String msg)
{
    matrix->clear();
    matrix->setBrightness(10);
    matrix->setCursor(0, 6);
    matrix->setTextColor(matrix->Color(255, 255, 255));
    matrix->print(msg);
    matrix->show();
}

static byte c1; // Last character buffer
byte utf8ascii(byte ascii)
{
    if (ascii < 128) // Standard ASCII-set 0..0x7F handling
    {
        c1 = 0;
        return (ascii);
    }
    // get previous input
    byte last = c1; // get last char
    c1 = ascii;     // remember actual character
    switch (last)   // conversion depending on first UTF8-character
    {
    case 0xC2:
        return (ascii)-34;
        break;
    case 0xC3:
        return (ascii | 0xC0) - 34;
        break;
    case 0x82:
        if (ascii == 0xAC)
            return (0xEA);
    }
    return (0);
}

String utf8ascii(String s)
{
    String r = "";
    char c;
    for (int i = 0; i < s.length(); i++)
    {
        c = utf8ascii(s.charAt(i));
        if (c != 0)
            r += c;
    }
    return r;
}

void setup()
{
    FastLED.addLeds<NEOPIXEL, D2>(leds, 256).setCorrection(TypicalLEDStrip);
    Serial.begin(115200);
    pinMode(led, OUTPUT);
    matrix->begin();
    matrix->setTextWrap(false);
    matrix->setFont(&TomThumb);
    delay(10);

    printMsg("WiFi Conf");

    WiFi.begin(ssid, password);//启动

     //在这里检测是否成功连接到目标网络，未连接则阻塞。
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
    }

    if (client.connect(host, tcpPort))
    {
        printMsg("Connected");
    }
    else
    {
        printMsg("wifi Fail");
    }

    delay(500);

    webSocketClient.path = "/";
    webSocketClient.host = host;
    if (webSocketClient.handshake(client))
    {
        printMsg("handshake");
    }
    else
    {
        printMsg("HSFailed");
        while (1)
        {
            // Hang on failure
        }
    }
}

void loop()
{
    String data;

    if (client.connected())
    {
        webSocketClient.getData(data);
        if (data.length() > 0)
        {
            DynamicJsonBuffer jsonBuffer;
            JsonObject &json = jsonBuffer.parseObject(data);
            String serviceName = json["ServiceName"];
            if (serviceName.equals("show"))
            {
                matrix->show();
            }
            else if (serviceName.equals("clear"))
            {
                matrix->clear();
            }
            else if (serviceName.equals("drawText"))
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
            else if (serviceName.equals("drawBMP"))
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
            else if (serviceName.equals("drawLine"))
            {
                matrix->drawLine(json["x0"].as<int16_t>(), json["y0"].as<int16_t>(), json["x1"].as<int16_t>(), json["y1"].as<int16_t>(), matrix->Color(json["color"][0].as<int16_t>(), json["color"][1].as<int16_t>(), json["color"][2].as<int16_t>()));
            }
            else if (serviceName.equals("drawCircle"))
            {
                matrix->drawCircle(json["x0"].as<int16_t>(), json["y0"].as<int16_t>(), json["r"].as<int16_t>(), matrix->Color(json["color"][0].as<int16_t>(), json["color"][1].as<int16_t>(), json["color"][2].as<int16_t>()));
            }
            else if (serviceName.equals("drawRect"))
            {
                matrix->drawRect(json["x"].as<int16_t>(), json["y"].as<int16_t>(), json["w"].as<int16_t>(), json["h"].as<int16_t>(), matrix->Color(json["color"][0].as<int16_t>(), json["color"][1].as<int16_t>(), json["color"][2].as<int16_t>()));
            }
            else if (serviceName.equals("fill"))
            {
                matrix->fillScreen(matrix->Color(json["color"][0].as<int16_t>(), json["color"][1].as<int16_t>(), json["color"][2].as<int16_t>()));
            }
            else if (serviceName.equals("drawPixel"))
            {
                matrix->drawPixel(json["x"].as<int16_t>(), json["y"].as<int16_t>(), matrix->Color(json["color"][0].as<int16_t>(), json["color"][1].as<int16_t>(), json["color"][2].as<int16_t>()));
            }
            else if (serviceName.equals("setBrightness"))
            {
                matrix->setBrightness(json["brightness"].as<int16_t>());
            }

            delay(30);
        }

        // capture the value of analog 1, send it along
        //pinMode(1, INPUT);
        //data = String(analogRead(1));

        //webSocketClient.sendData(data);
    }
    else
    {
        printStr("webDisconnect");
        while (1)
        {
            // Hang on disconnect.
        }
        // wait to fully let the client disconnect
        delay(3000);
    }
}
