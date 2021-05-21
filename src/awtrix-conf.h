///////////////////////// AWTRIX CONFIG /////////////////////////

// Wifi Config
const char *ssid = "A12";
const char *password = "simba-server";
char *awtrix_server = "192.168.50.121";

//#define USB_CONNECTION
//#define MATRIX_MODEV2

/// LDR Config
#define LDR_RESISTOR 1000 //ohms
#define LDR_PIN A0
#define LDR_PHOTOCELL LightDependentResistor::GL5516