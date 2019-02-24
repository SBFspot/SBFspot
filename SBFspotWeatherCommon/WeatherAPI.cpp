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

/***********************************************************************************************
 **                                 _   _ ____  _
 **                             ___| | | |  _ \| |
 **                            / __| | | | |_) | |
 **                           | (__| |_| |  _ <| |___
 **                            \___|\___/|_| \_\_____|
 **
 ** Curl is a command line tool for transferring data specified with URL
 ** syntax. Find out how to use curl by reading the curl.1 man page or the
 ** MANUAL document. Find out how to install Curl by reading the INSTALL
 ** document.
 **
 ** COPYRIGHT AND PERMISSION NOTICE
 **
 ** Copyright (c) 1996 - 2008, Daniel Stenberg, <daniel@haxx.se>.
 **
 ** All rights reserved.
 **
 ** Permission to use, copy, modify, and distribute this software for any purpose
 ** with or without fee is hereby granted, provided that the above copyright
 ** notice and this permission notice appear in all copies.
 **
 ** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 ** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 ** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS. IN
 ** NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 ** DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 ** OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
 ** OR OTHER DEALINGS IN THE SOFTWARE.
 **
 ** Except as contained in this notice, the name of a copyright holder shall not
 ** be used in advertising or otherwise to promote the sale, use or other dealings
 ** in this Software without prior written authorization of the copyright holder.
 **
 ***********************************************************************************************/

#include "WeatherAPI.h"
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include "json.h"

using namespace std;
using namespace boost;
using namespace boost::algorithm;

Weather::Weather(std::string Endpoint, std::string Parameters, std::string Key, int timeout)
{
	m_url = Endpoint + "?" + Parameters;
	m_key = Key;
	m_timeout = timeout;
	m_curlres = CURLE_OK;

	/* In windows, this will init the winsock stuff */
	curl_global_init(CURL_GLOBAL_ALL);

	m_curl = curl_easy_init();

	if (!m_curl)
		m_curlres = CURLE_FAILED_INIT;
}

Weather::~Weather()
{
	curl_easy_cleanup(m_curl);
	curl_global_cleanup();
}

// Static Callback member function
// Implemented in Weather_x.cpp
// void Weather::writeCallback(char *ptr, size_t size, size_t nmemb, void *f)

// Callback implementation
size_t Weather::writeCallback_impl(char *ptr, size_t size, size_t nmemb)
{
	m_buffer.append(ptr, size * nmemb);
	return size * nmemb;
}

CURLcode Weather::downloadURL(string URL)
{
	m_curlres = CURLE_FAILED_INIT;
	m_http_status = HTTP_OK;

	if (m_curl)
	{
		curl_easy_setopt(m_curl, CURLOPT_URL, URL.c_str());
		curl_easy_setopt(m_curl, CURLOPT_TIMEOUT, m_timeout);
		curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, Weather::writeCallback);
        curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, this);
		clearBuffer();
		m_curlres = curl_easy_perform(m_curl);
        curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &m_http_status);
	}

	return m_curlres;
}

CURLcode Weather::downloadURL(string URL, string data)
{
	m_curlres = CURLE_FAILED_INIT;
	m_http_status = HTTP_OK;

	if (m_curl)
	{
		curl_easy_setopt(m_curl, CURLOPT_URL, URL.c_str());
		curl_easy_setopt(m_curl, CURLOPT_POST, 1);
	    curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, data.c_str());
        curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, m_http_header);
		curl_easy_setopt(m_curl, CURLOPT_TIMEOUT, m_timeout);
		curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, Weather::writeCallback);
        curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, this);
		clearBuffer();
		m_curlres = curl_easy_perform(m_curl);
        curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &m_http_status);
	}

	return m_curlres;
}

CURLcode Weather::getData(std::string &response)
{
	m_curlres = CURLE_FAILED_INIT;

	if (m_curl)
	{
		if ((m_curlres = downloadURL(m_url + "&appid=" + m_key)) == CURLE_OK)
		{
			response = m_buffer;

			// Parse weather data
			Json::Value weather;
			Json::CharReaderBuilder builder;
			Json::CharReader *reader = builder.newCharReader();
			std::string parsingerrors;

			bool parsingOK = reader->parse(response.c_str(), response.c_str() + response.size(), &weather, &parsingerrors);
			delete reader;

			if (parsingOK)
			{
				m_loc_city_id = weather["id"].asUInt();
				m_loc_city = weather["name"].asString();
				m_loc_country = weather["sys"]["country"].asString();
				m_loc_longitude = weather["coord"]["lon"].asDouble();
				m_loc_latitude = weather["coord"]["lat"].asDouble();
				m_temp_cur = weather["main"]["temp"].asDouble();
				m_temp_min = weather["main"]["temp_min"].asDouble();
				m_temp_max = weather["main"]["temp_max"].asDouble();
				m_pressure = weather["main"]["pressure"].asDouble();
				m_humidity = weather["main"]["humidity"].asDouble();
				m_weather_id = weather["weather"][0]["id"].asInt();
				m_weather_description = weather["weather"][0]["description"].asString();
				m_weather_icon = weather["weather"][0]["icon"].asString();
				m_visibility = weather["visibility"].asDouble();
				m_wind_speed = weather["wind"]["speed"].asDouble();
				m_wind_direction = weather["wind"]["deg"].asDouble();
				m_datetime = weather["dt"].asInt();
			}
			else
			{
				response = parsingerrors;
			}
		}
		else
			response = errtext();
	}

	return m_curlres;
}
