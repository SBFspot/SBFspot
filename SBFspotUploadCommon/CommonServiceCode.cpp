/************************************************************************************************
	SBFspot - Yet another tool to read power production of SMA solar inverters
	(c)2012-2021, SBF

	Latest version found at https://github.com/SBFspot/SBFspot

	License: Attribution-NonCommercial-ShareAlike 3.0 Unported (CC BY-NC-SA 3.0)
	http://creativecommons.org/licenses/by-nc-sa/3.0/

	You are free:
		to Share - to copy, distribute and transmit the work
		to Remix - to adapt the work
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

const uint32_t MAX_INVERTERS = 20;

void CommonServiceCode(void)
{
	CURLcode rc_curl = CURLE_OK;
	int rc_db = 0;
	int batch_datelimit = 0, batch_statuslimit = 0;
	std::stringstream msg;

	int nextStatusCheck = 0;
	const int timeBetweenChecks = 2 * 60 * 60;	// every 2 hours

	db_SQL_Base db;

    // Periodically check if the service is stopping.
    while (!bStopping)
    {
        msg.str("");
#if defined(USE_MYSQL)
		db.open(cfg.getSqlHostname(), cfg.getSqlUsername(), cfg.getSqlPassword(), cfg.getSqlDatabase(), cfg.getSqlPort());
#elif defined(USE_SQLITE)
		db.open(cfg.getSqlDatabase());
#endif

		if (db.isopen())
		{
			for (std::map<SMASerial, PVOSystemID>::const_iterator it=cfg.getPvoSIDs().begin(); !bStopping && it!=cfg.getPvoSIDs().end(); ++it)
			{
				PVOutput PVO(it->second, cfg.getPvoApiKey(), 30);
				int now = time(NULL);
				db.get_config(SQL_NEXTSTATUSCHECK, nextStatusCheck);
				if ((nextStatusCheck - now) < 0)
				{
					PVO.getSystemData();

					if (PVO.HTTP_status() == PVOutput::HTTP_OK)
					{
                        batch_datelimit = PVO.batch_datelimit();
                        batch_statuslimit = PVO.batch_statuslimit();
                        nextStatusCheck = now + timeBetweenChecks;
                        db.set_config(SQL_BATCH_DATELIMIT, db.intToString(batch_datelimit));
                        db.set_config(SQL_BATCH_STATUSLIMIT, db.intToString(batch_statuslimit));
                        db.set_config(SQL_NEXTSTATUSCHECK, db.intToString(nextStatusCheck));

                        if (!PVO.isTeamMember())
                        {
                            Log(PVO.SystemName() + " is not yet member of SBFspot Team. Consider joining at http://pvoutput.org/listteam.jsp?tid=613", LOG_WARNING_);
                        }
					}
				}

				if (batch_datelimit == 0) batch_datelimit = PVO.batch_datelimit();
				if (batch_statuslimit == 0) batch_statuslimit = PVO.batch_statuslimit();

				std::string data;

				int datapoints = 0;
				if((rc_db = db.batch_get_archdaydata(data, it->first/*Serial*/, batch_datelimit, batch_statuslimit, datapoints)) == db.SQL_OK)
				{
					if (!data.empty())
					{
						std::stringstream msg;

						if (datapoints == 1)
							msg << "Uploading datapoint: " << data;
						else
						{
							if (VERBOSE_HIGH)
								msg << "Uploading " << datapoints << " datapoints " << data;
							else
							{
								size_t pos = data.find_first_of(";");
								if (pos == std::string::npos) pos = data.length();
								msg << "Uploading " << datapoints << " datapoints, starting with " << data.substr(0, pos);
							}
						}

						std::string response;
						if ((rc_curl = PVO.addBatchStatus(data, response)) == CURLE_OK)
						{
							if (PVO.HTTP_status() == PVOutput::HTTP_OK)
							{
								msg << " => OK (200)";
								Log(msg.str(), LOG_INFO_);
								rc_db = db.batch_set_pvoflag(response, it->first/*Serial*/);
								if (rc_db != db.SQL_OK)
									Log("batch_set_pvoflag() returned " + db.errortext(), LOG_ERROR_);
							}
							else
							{
								msg << " " << response;
								Log(msg.str(), LOG_ERROR_);
							}
						}
						else
							Log("addBatchStatus() returned " + PVO.errtext(), LOG_ERROR_);
					}
				}
			}

			db.close();

			// Wait for next run; 30 seconds after every 1 minute (08:00:30 - 08:01:30 - 08:02:30 - ...)
			for (int countdown = 90 - (time(NULL) % 60); !bStopping && countdown > 0; countdown--)
				sleep(1);
		}
		else
		{
		    std::string errortext = db.errortext();
			msg << "Failed to open database: " << errortext;
			Log(msg.str(),LOG_ERROR_);
			bStopping = true;
		}
    }
}

int Log(std::string txt, ERRLEVEL level)
{
	int rc = 0;
	if (level >= cfg.getLogLevel())
	{
		char buff[32];
		time_t now = time(NULL);
		strftime(buff, sizeof(buff), "SBFspotUpload%Y%m%d.log", localtime(&now));
		std::string fullpath(cfg.getLogDir() + buff);

		std::ofstream fs_log;
		fs_log.open(fullpath.c_str(), std::ios::app | std::ios::out);

		if (fs_log.is_open())
		{
			strftime(buff, sizeof(buff), "[%H:%M:%S] ", localtime(&now));
			fs_log << buff << errlevelText[level] << ": " << txt << std::endl;
			fs_log.close();
			rc = 0;
		}
		else
		{
			std::cerr << "Unable to write to logfile [" << fullpath << "]" << std::endl;
			rc = 1;
		}
	}

	return rc;
}
