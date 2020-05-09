#include <ESP8266WiFi.h>
#include <FastLED_NeoMatrix.h>
#include <Fonts/TomThumb.h>
 
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
            matrix->setTextColor(matrix->Color(0, 255, 0));
            matrix->print("Connecting");
            matrix->show();
            delay(500);
            digitalWrite(led, LOW);
        }
    }
 
    while (client.available())//available()表示是否可以获取到数据
    {
        client.setTimeout(20);
        String val = client.readString();
        matrix->clear();
        matrix->setCursor(0,6);
        matrix->setBrightness(10);
        matrix->setFont(&TomThumb);
        matrix->setTextColor(matrix->Color(255, 255, 255));
	    matrix->print(val);
	    matrix->show();
    }
}