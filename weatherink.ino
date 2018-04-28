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
#include <ESP8266HTTPClient.h>

#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>

#include <GxEPD.h>
#include <GxGDE0213B1/GxGDE0213B1.cpp>
#include <GxIO/GxIO_SPI/GxIO_SPI.cpp>
#include <GxIO/GxIO.cpp>
#include <Fonts/FreeSans12pt7b.h>

#include <JsonListener.h>
#include <JsonStreamingParser.h>

#include <unordered_map>
#include <vector>
#include <string>

#include "meteocons.h"
#include "credentials.h"

GxIO_Class io(SPI, D8, D3, D4);
GxEPD_Class display(io, D4, D2);

const int CONFIG_PIN = D1;
const int DISPLAY_WIDTH = 250;
const int DISPLAY_HEIGHT = 122;

#define WUNDERGROUND_COUNTRY "FI"
#define WUNDERGROUND_TOWN "Jyvaskyla"

const char WUNDERGROUND_QUERY[] =
	"http://api.wunderground.com/api/"
	WUNDERGROUND_KEY
	"/forecast/q/"
	WUNDERGROUND_COUNTRY
	"/"
	WUNDERGROUND_TOWN
	".json";

const std::unordered_map<std::string, uint8_t> weekday_to_num =
{
	{"Mon", 0},
	{"Tue", 1},
	{"Wed", 2},
	{"Thu", 3},
	{"Fri", 4},
	{"Sat", 5},
	{"Sun", 6}
};

const std::vector<std::string> weekdays_fi =
	{ "ma", "ti", "ke", "to", "pe", "la", "su" };

const std::unordered_map<std::string, char> forecast_to_symbol =
{
  { "chanceflurries", 'X' },
  { "chancerain", 'Q' },
  { "chancesleet", 'W' },
  { "chancesnow", 'V' },
  { "chancetstorms", 'S' },
  { "clear", 'B' },
  { "cloudy", 'Y' },
  { "flurries", 'X' },
  { "fog", 'M' },
  { "hazy", 'E' },
  { "mostlycloudy", 'Y' },
  { "mostlysunny", 'H' },
  { "partlycloudy", 'H' },
  { "partlysunny", 'J' },
  { "sleet", 'W' },
  { "rain", 'R' },
  { "snow", 'W' },
  { "sunny", 'B' },
  { "tstorms", '0' },

  { "nt_chanceflurries", '$' },
  { "nt_chancerain", '7' },
  { "nt_chancesleet", '#' },
  { "nt_chancesnow", '#' },
  { "nt_chancetstorms", '&' },
  { "nt_clear", '2' },
  { "nt_cloudy", 'Y' },
  { "nt_flurries", '$' },
  { "nt_fog", 'M' },
  { "nt_hazy", 'E' },
  { "nt_mostlycloudy", '5' },
  { "nt_mostlysunny", '3' },
  { "nt_partlycloudy", '4' },
  { "nt_partlysunny", '4' },
  { "nt_sleet", '9' },
  { "nt_rain", '7' },
  { "nt_snow", '#' },
  { "nt_sunny", '4' },
  { "nt_tstorms", '&' }
};

template <typename T> T map_at_safe(
	const std::unordered_map<std::string, T> &mymap,
	const std::string &key, const T &def)
{
	const auto it = mymap.find(key);
	if (it == mymap.end()) return def;
	return it->second;
}

struct WeatherInfo
{
	std::string forecast;
	std::string weekday;
	int temp_low;
	int temp_high;
};

class WundergroundListener : public JsonListener
{
public:
	WundergroundListener()
	{
		m_info_vec.reserve(4);
	}

	virtual void whitespace(char) {}
	virtual void startDocument() {}
	virtual void endDocument() {}

	virtual void key(String key)
	{
		m_last_key = key;

		if (key == "high") m_is_high = true;
		else if (key == "low") m_is_high = false;
	}

	virtual void value(String value)
	{
		if (m_last_key == "celsius")
		{
			int val = strtol(value.c_str(), nullptr, 10);
			if (m_is_high) m_info.temp_high = val;
			else m_info.temp_low = val;
		}
		else if (m_last_key == "weekday_short")
			m_info.weekday = value.c_str();
		else if (m_last_key == "icon")
			m_info.forecast = value.c_str();
	}

	virtual void startArray()
	{
		if (m_forecastday_level >= 0) m_forecastday_level++;
		if (m_simpleforecast_level >= 0 && m_last_key == "forecastday")
		{
			m_forecastday_level = 0;
			m_day_level = m_simpleforecast_level;
		}
	}

	virtual void endArray()
	{
		if (m_forecastday_level >= 0) m_forecastday_level--;
	}

	virtual void startObject()
	{
		if (m_simpleforecast_level >= 0) m_simpleforecast_level++;
		if (m_last_key == "simpleforecast") m_simpleforecast_level = 0;
	}

	virtual void endObject()
	{
		if (m_simpleforecast_level >= 0) m_simpleforecast_level--;

		if (m_forecastday_level >= 0 && m_simpleforecast_level == m_day_level)
		{
			m_day_index++;
			m_info_vec.push_back(m_info);
		}
	}

	const std::vector<WeatherInfo> &getInfo() { return m_info_vec; }

private:
	String m_last_key;
	int m_simpleforecast_level = -1;
	int m_forecastday_level = -1;
	int m_day_level =  -1;
	int m_day_index = 0;
	bool m_is_high = false;
	WeatherInfo m_info;
	std::vector<WeatherInfo> m_info_vec;
};

void drawString(Adafruit_GFX &gfx, int cx, int cy, const String &s)
{
	int16_t x, y;
	uint16_t w, h;
	gfx.getTextBounds((char *)s.c_str(), 0, 0, &x, &y, &w, &h);
	int dx = x + w / 2;
	int dy = y + h / 2;
	gfx.setCursor(cx - dx, cy - dy);
	gfx.print(s);
}

void setup()
{
	Serial.begin(115200);
	display.init();
	display.setRotation(1);
	display.setTextColor(GxEPD_BLACK);
	display.fillScreen(GxEPD_WHITE);

	pinMode(CONFIG_PIN, INPUT_PULLUP);
	delay(10);
	if (!digitalRead(CONFIG_PIN))
	{
		display.setFont(&FreeSans12pt7b);
		drawString(display, DISPLAY_WIDTH / 2, DISPLAY_HEIGHT / 2 - 30,
			"Connect to SSID:");
		drawString(display, DISPLAY_WIDTH / 2, DISPLAY_HEIGHT / 2,
			"weatherink");
		drawString(display, DISPLAY_WIDTH / 2, DISPLAY_HEIGHT / 2 + 30,
			"to configure");
		display.update();

		WiFiManager wifiManager;
		wifiManager.startConfigPortal("weatherink");
	}

	int tries = 10;
	while (WiFi.status() != WL_CONNECTED && tries-- > 0) delay(1000);

	if (WiFi.status() == WL_CONNECTED)
	{
		HTTPClient client;
		client.setTimeout(5000);
		client.begin(WUNDERGROUND_QUERY);
		int code = client.GET();
		
		if (code == 200)
		{
			JsonStreamingParser parser;
			WundergroundListener listener;
			parser.setListener(&listener);
			WiFiClient *stream = client.getStreamPtr();
			while (client.connected())
			{
				uint8_t buf[128];
				if (stream->available())
				{
					int c = stream->read(buf, sizeof(buf));
					for (int i = 0; i < c; ++i) parser.parse(buf[i]);
				}
				yield();
			}

			int step = DISPLAY_WIDTH / 3;
			int x = DISPLAY_WIDTH / 6;
			int y = DISPLAY_HEIGHT / 2;
			int n = 0;
			int first_wday = -1;
			for (const WeatherInfo &info : listener.getInfo())
			{
				display.setFont(&FreeSans12pt7b);

				int wday = map_at_safe<uint8_t>(weekday_to_num, info.weekday, -1);
				if (first_wday == -1) first_wday = wday;

				{
					std::string wday_str;
					if (wday >= 0) wday_str = weekdays_fi[wday];
 					else wday_str = "err";
					drawString(display, x, y - 50, wday_str.c_str());
				}

				{
					int temp_avg = (info.temp_low + info.temp_high + 1) / 2;
					drawString(display, x, y + 50, String(temp_avg));
				}

				{
					char symbol = map_at_safe<char>(forecast_to_symbol,
						info.forecast, ')');
					display.setFont(&meteocons_webfont36pt7b);
					drawString(display, x, y, String(symbol));
				}

				x += step;
				if (++n == 3) break;
			}

			{
				uint32_t wday_u = (uint32_t)first_wday;
				uint32_t wday_rtc;
				ESP.rtcUserMemoryRead(0, &wday_rtc, sizeof(wday_u));

				if (wday_rtc == wday_u)
				{
					display.updateWindow(0, 0, display.width(), display.height());
				}
				else
				{
					display.update();
				}

				ESP.rtcUserMemoryWrite(0, &wday_u, sizeof(wday_u));
			}
		}
		client.end();
	}

	display.powerDown();
	ESP.deepSleep(5 * 1000000UL);
}

void loop()
{
}
