/*
E-ink weather display implemented on ESP8266
Copyright (C) 2018  Sakari Kapanen

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <ESP8266WiFi.h>

#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>

#include <GxEPD.h>
#include <GxGDE0213B1/GxGDE0213B1.cpp>
#include <GxIO/GxIO_SPI/GxIO_SPI.cpp>
#include <GxIO/GxIO.cpp>

#include "meteocons.h"

GxIO_Class io(SPI, D8, D3, D4); // arbitrary selection of D3(=0), D4(=2), selected for default of GxEPD_Class
GxEPD_Class display(io, D4, D2);

const int CONFIG_PIN = D1;
const int DISPLAY_WIDTH = 250;
const int DISPLAY_HEIGHT = 122;


void drawChar(Adafruit_GFX &gfx, int cx, int cy, char c)
{
	int16_t x, y;
	uint16_t w, h;
	gfx.getTextBounds(&c, 0, 0, &x, &y, &w, &h);
	int dx = x + w/2;
	int dy = y + h/2;
	gfx.drawChar(cx - dx, cy - dy, c, GxEPD_BLACK, GxEPD_WHITE, 1);
}

void setup()
{
	Serial.begin(115200);

	/*
	pinMode(CONFIG_PIN, INPUT_PULLUP);

	if (digitalRead(CONFIG_PIN))
	{
		WiFiManager wifiManager;
		wifiManager.startConfigPortal("weatherink");
	}
	*/

	display.init();
	display.setRotation(1);
	display.fillScreen(GxEPD_WHITE);
	display.setFont(&meteocons_webfont48pt7b);
	drawChar(display, DISPLAY_WIDTH / 2, DISPLAY_HEIGHT / 2, 'A');
	display.update();
}

void loop()
{

}
