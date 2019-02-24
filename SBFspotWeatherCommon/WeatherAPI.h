/************************************************************************************************
	SBFspot - Yet another tool to read power production of SMA® solar inverters
	(c)2012-2018, SBF

	Latest version found at https://github.com/SBFspot/SBFspot

	License: Attribution-NonCommercial-ShareAlike 3.0 Unported (CC BY-NC-SA 3.0)
	http://creativecommons.org/licenses/by-nc-sa/3.0/

	You are free:
		to Share — to copy, distribute and transmit the work
		to Remix — to adapt the work
	Under the following conditions:
	Attribution:
		You must attribute the work in the manner specified by the author or licensor
		(but not in any way that suggests that they endorse you or your use of the work).
	Noncommercial:
		You may not use this work for commercial purposes.
	Share Alike:
		If you alter, transform, or build upon this work, you may distribute the resulting work
		only under the same or similar license to this one.

DISCLAIMER:
	A user of SBFspot software acknowledges that he or she is receiving this
	software on an "as is" basis and the user is not relying on the accuracy
	or functionality of the software for any purpose. The user further
	acknowledges that any use of this software will be at his own risk
	and the copyright owner accepts no responsibility whatsoever arising from
	the use or application of the software.

	SMA is a registered trademark of SMA Solar Technology AG

************************************************************************************************/

#pragma once

#include "../SBFspot/osselect.h"
#include <curl/curl.h>
#include <string>
#include <iostream>
#include <sstream>
#include <vector>
#include <map>

extern int quiet;
extern int verbose;

class Weather
{
public:
	enum
	{
		HTTP_OK = 200,
		HTTP_FORBIDDEN = 403,
		HTTP_NOT_FOUND = 404,
		// More codes at http://en.wikipedia.org/wiki/List_of_HTTP_status_codes
	};

private:
	CURLcode m_curlres;
	CURL* m_curl;
	unsigned int m_timeout;
	std::string m_url;
	std::string m_key;
	std::string m_buffer;
	curl_slist *m_http_header;
	long m_http_status;

	// Weather Conditions - https://openweathermap.org/weather-conditions
	int          m_weather_id;
	std::string  m_weather_description;
	std::string  m_weather_icon;
	double       m_pressure;
	double       m_humidity;
	double       m_temp_cur;
	double       m_temp_min;
	double       m_temp_max;
	double       m_visibility;
	double       m_wind_speed;
	double       m_wind_direction;
	time_t       m_datetime;
	double       m_loc_longitude;
	double       m_loc_latitude;
	time_t       m_loc_sunrise;
	time_t       m_loc_sunset;
	std::string  m_loc_country;
	std::string  m_loc_city;
	unsigned int m_loc_city_id;

public:
	Weather(std::string Endpoint, std::string Parameters, std::string Key, int timeout);
	~Weather();
	CURLcode downloadURL(std::string URL);
	CURLcode downloadURL(std::string URL, std::string data);
	void clearBuffer() { m_buffer.clear(); }
	long HTTP_status() const { return m_http_status; }
	CURLcode errcode() const { return m_curlres; }
	std::string errtext() const { return std::string(curl_easy_strerror(m_curlres)); }
	CURLcode getData(std::string &response);

	std::string  url() const { return m_url; }
	int          id() const { return m_weather_id; }
	std::string  description() const { return m_weather_description; }
	std::string  icon() const { return m_weather_icon; }
	double       temperature() const { return m_temp_cur; }
	double       temp_min() const { return m_temp_min; }
	double       temp_max() const { return m_temp_max; }
	double       pressure() const { return m_pressure; }
	double       humidity() const { return m_humidity; }
	double       visibility() const { return m_visibility; }
	double       wind_speed() const { return m_wind_speed; }
	double       wind_direction() const { return m_wind_direction; }
	double       longitude() const { return m_loc_longitude; }
	double       latitude() const { return m_loc_latitude; }
	time_t       datetime() const { return m_datetime; }
	unsigned int city_id() const { return m_loc_city_id; }
	std::string  city() const { return m_loc_city; }
	std::string  country() const { return m_loc_country; }

private:
	static void writeCallback(char *ptr, size_t size, size_t nmemb, void *stream);
	size_t writeCallback_impl(char *ptr, size_t size, size_t nmemb);
	bool isverbose(int level) { return !quiet && (verbose >= level); }
};

