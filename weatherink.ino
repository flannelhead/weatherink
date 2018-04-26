#include <ESP8266WiFi.h>

#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>

#include <GxEPD.h>
#include <GxGDE0213B1/GxGDE0213B1.cpp>
#include <GxIO/GxIO_SPI/GxIO_SPI.cpp>
#include <GxIO/GxIO.cpp>

GxIO_Class io(SPI, D8, D3, D4); // arbitrary selection of D3(=0), D4(=2), selected for default of GxEPD_Class
GxEPD_Class display(io, D4, D2);

const int CONFIG_PIN = D1;

void setup()
{
  display.init();
  /*
	pinMode(CONFIG_PIN, INPUT_PULLUP);

	if (digitalRead(CONFIG_PIN))
	{
		WiFiManager wifiManager;
		wifiManager.startConfigPortal("weatherink");
	}
 */
}

void loop()
{

}
