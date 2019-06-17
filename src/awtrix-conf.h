///////////////////////// AWTRIX CONFIG /////////////////////////

// Wifi Config

typedef struct {
    const char *ssid = "Kindergarten";
    const char *password = "53825382";
    char *awtrix_server = "192.168.178.39";
} configData_t;

configData_t wifiConfig;


bool usbWifi = false; // true = usb...

//#define MATRIX_MODEV2

/// LDR Config
#define LDR_RESISTOR 1000 //ohms
#define LDR_PIN A0
#define LDR_PHOTOCELL LightDependentResistor::GL5516