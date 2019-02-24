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

#include "CommonServiceCode.h"
#include "..\SBFspot\misc.h"
#include <chrono>
#include <thread>

void CommonServiceCode(void)
{
	CURLcode rc_curl = CURLE_OK;
	int rc_db = 0;
	std::stringstream msg;

	msg << "Starting Weather Daemon. Delay between requests is " << cfg.ApiDelay() << " seconds";
	Log(msg.str(), LOG_INFO_);

	db_SQL_Weather db = db_SQL_Weather();

    // Periodically check if the service is stopping.
    while (!bStopping)
    {
		msg.str("");
		db.open(cfg.SqlHostname(), cfg.SqlUsername(), cfg.SqlPassword(), cfg.SqlDatabase());

		if (db.isopen())
		{
			Weather WAPI(cfg.ApiEndpoint(), cfg.ApiParameters(), cfg.ApiKey(), 30);

			Log("Requesting weather data from " + WAPI.url() + "&appid=****", LOG_INFO_);

			std::string response;
			if ((rc_curl = WAPI.getData(response)) == CURLE_OK)
			{
				if (WAPI.HTTP_status() == Weather::HTTP_OK)
				{
					Log("OK (200) " + response, LOG_INFO_);
					
					msg << "date=" << strftime_t("%d/%m/%Y %H:%M:%S", WAPI.datetime()) << " temp=" << WAPI.temperature();
					Log(msg.str(), LOG_INFO_);

					db.insert_data(WAPI);
				}
				else
				{
					Log(response, LOG_ERROR_);
				}
			}
			else
				Log("Weather::getData() returned " + WAPI.errtext(), LOG_ERROR_);

			db.close();
		}
		else
		{
		    std::string errortext = db.errortext();
			msg << "Failed to open database: " << errortext;
			Log(msg.str(),LOG_ERROR_);
		}

		// Wait for next run
		for (int countdown = cfg.ApiDelay(); !bStopping && countdown > 0; countdown--)
			//std::this_thread::sleep_for(std::chrono::seconds(1));
			sleep(1);
    }
}

int Log(std::string txt, ERRLEVEL level)
{
	char buff[32];
	time_t now = time(NULL);
	strftime(buff, sizeof(buff), "SBFspotWeather%Y%m%d.log", localtime(&now));
	std::string fullpath(cfg.LogDir() + buff);

	std::ofstream fs_log;
	fs_log.open(fullpath.c_str(), std::ios::app | std::ios::out);

    if (fs_log.is_open())
    {
        strftime(buff, sizeof(buff), "[%H:%M:%S] ", localtime(&now));
        fs_log << buff << errlevelText[level] << ": " << txt << std::endl;
        fs_log.close();
        return 0;
    }
    else
    {
        std::cerr << "Unable to write to logfile [" << fullpath << "]" << std::endl;
        return 1;
    }
}
