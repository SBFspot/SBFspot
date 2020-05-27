/************************************************************************************************
	SBFspot - Yet another tool to read power production of SMA® solar inverters
	(c)2012-2020, SBF

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

#if defined(USE_MYSQL)

//TODO: MAX_INVERTERS is defined twice (Quick but dirty fix)
const int MAX_INVERTERS = 20;

#include "db_MySQL.h"
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

using namespace std;

string db_SQL_Base::status_text(int status)
{
	switch (status)
	{
		//Grid Relay Status
		case 311: return "Open";
		case 51: return "Closed";

		//Device Status
		case 307: return "OK";
		case 455: return "Warning";
		case 35: return "Fault";

		//NaNStt=Information not available
		case 0xFFFFFD: return "N/A";

		default: return "?";
	}
}

int db_SQL_Base::open(string server, string user, string pass, string database)
{
	int result = SQL_OK;

    if (!m_dbHandle)	// Not yet open?
	{
		m_database = database;

		if (database.size() > 0)
		{
			m_dbHandle = mysql_init(NULL);
			if (!mysql_real_connect(m_dbHandle, server.c_str(), user.c_str(), pass.c_str(), database.c_str(), 0, NULL, 0))
			{
			    m_errortext = mysql_error(m_dbHandle);
				result = SQL_ERROR;
			}
		}
		else
			result = SQL_ERROR;

    	if(result != SQL_OK)
		{
			print_error("Can't open MySQL db [" + m_database + "]");
			m_dbHandle = NULL;
		}
	}

	return result;
}

int db_SQL_Base::close(void)
{
	int result = SQL_OK;

	mysql_close(m_dbHandle);
	m_dbHandle = NULL;

	return result;
}

int db_SQL_Base::exec_query(string qry)
{
	//returns 0 if success (SQL_OK)
	return mysql_real_query(m_dbHandle, qry.c_str(), qry.size());
}

int db_SQL_Base::type_label(InverterData *inverters[])
{
	std::stringstream sql;
	int rc = SQL_OK;

	for (int inv=0; inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
	{
		sql.str("");

		// Instead of using REPLACE which is actually a DELETE followed by INSERT,
		// we do an INSERT IGNORE (for new records) followed by UPDATE (for existing records)
		sql << "INSERT IGNORE INTO Inverters VALUES(" <<
			inverters[inv]->Serial << ',' <<
			s_quoted(inverters[inv]->DeviceName) << ',' <<
			s_quoted(inverters[inv]->DeviceType) << ',' <<
			s_quoted(inverters[inv]->SWVersion) << ',' <<
			"0,0,0,0,0,0,'','',0)";

		if ((rc = exec_query(sql.str())) != SQL_OK)
			print_error("exec_query() returned", sql.str());

		sql.str("");

		sql << "UPDATE Inverters SET" <<
			" Name=" << s_quoted(inverters[inv]->DeviceName) <<
			",Type=" << s_quoted(inverters[inv]->DeviceType) <<
			",SW_Version=" << s_quoted(inverters[inv]->SWVersion) <<
			" WHERE Serial=" << inverters[inv]->Serial;

		if ((rc = exec_query(sql.str())) != SQL_OK)
			print_error("exec_query() returned", sql.str());
	}

	return rc;
}

int db_SQL_Base::device_status(InverterData *inverters[], time_t spottime)
{
	std::stringstream sql;
	int rc = SQL_OK;

	// Take time from computer instead of inverter
	//time_t spottime = cfg->SpotTimeSource == 0 ? inverters[0]->InverterDatetime : time(NULL);

	for (int inv=0; inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
	{
		sql.str("");

		sql << "UPDATE Inverters SET" <<
			" TimeStamp=" << strftime_t(spottime) <<
			",TotalPac=" << inverters[inv]->TotalPac <<
			",EToday=" << inverters[inv]->EToday <<	
			",ETotal=" << inverters[inv]->ETotal <<
			",OperatingTime=" << (double)inverters[inv]->OperationTime/3600 <<
			",FeedInTime=" << (double)inverters[inv]->FeedInTime/3600 <<
			",Status=" << s_quoted(status_text(inverters[inv]->DeviceStatus)) <<
			",GridRelay=" << s_quoted(status_text(inverters[inv]->GridRelayStatus)) <<
			",Temperature=" << (float)inverters[inv]->Temperature/100 <<
			" WHERE Serial=" << inverters[inv]->Serial;

		if ((rc = exec_query(sql.str())) != SQL_OK)
			print_error("exec_query() returned", sql.str());
	}

	return rc;
}

int db_SQL_Base::batch_get_archdaydata(std::string &data, unsigned int Serial, int datelimit, int statuslimit, int& recordcount)
{
	std::stringstream sql;
	int rc = SQL_OK;
	recordcount = 0;

	sql << "SELECT DATE_FORMAT(Timestamp,'%Y%m%d,%H:%i'),V1,V2,V3,V4,V5,V6,V7,V8,V9,V10,V11,V12 FROM vwPvoData "
		"WHERE TimeStamp>NOW()-INTERVAL " << datelimit-1 << " DAY "
		"AND PVoutput IS NULL "
		"AND Serial=" << Serial << " "
		"ORDER BY TimeStamp "
		"LIMIT " << statuslimit;

	rc = mysql_query(m_dbHandle, sql.str().c_str());

	if (rc == SQL_OK)
	{
		std::stringstream result;
		MYSQL_RES *sqlResult = mysql_store_result(m_dbHandle);
		MYSQL_ROW sqlRow = mysql_fetch_row(sqlResult);

		while (sqlRow)
		{
			result.str("");

			// from 2nd record, add a record separator
			if (!data.empty()) result << ";";

			// Date
			result << sqlRow[0];

			// Energy Generation, Power Generation, Energy Consumption, Power Consumption, Temperature, Voltage and Extended values
			for (int Vx = 1; Vx <= 12; Vx++)
			{
				result << ",";
				if (sqlRow[Vx] != NULL)
					result << sqlRow[Vx];
			}

			const std::string& str = result.str();
			int end = str.length();

			for (std::string::const_reverse_iterator it=str.rbegin(); it!=str.rend(); ++it, end--)
			{
				if ((*it) != ',')
					break;
			}

			data.append(result.str().substr(0, end));
			recordcount++;

			//Get next record
			sqlRow = mysql_fetch_row(sqlResult);
		}

        if(sqlResult)
        {
            mysql_free_result(sqlResult);
            sqlResult = NULL;
        }
	}
	else
		print_error("mysql_query() returned", sql.str());

	return rc;
}

int db_SQL_Base::batch_set_pvoflag(const std::string &data, unsigned int Serial)
{
	std::stringstream sql;
	int rc = SQL_OK;

	vector<std::string> items;
	boost::split(items, data, boost::is_any_of(";"));

	exec_query("START TRANSACTION");

	sql << "UPDATE DayData "
		"SET PVoutput=1 "
		"WHERE Serial=" << Serial << " "
		"AND DATE_FORMAT(FROM_UNIXTIME(Timestamp),'%Y%m%d,%H:%i') "
		"IN (";

	bool firstitem = true;
	for (vector<std::string>::iterator it=items.begin(); it!=items.end(); ++it)
	{
		if (it->substr(15, 1) == "1")
		{
			if (!firstitem)
				sql << ",";
			else
				firstitem = false;
			sql << s_quoted(it->substr(0, 14));
		}
	}

	sql << ")";

	if ((rc = exec_query(sql.str())) != SQL_OK)
	{
		print_error("exec_query() returned", sql.str());
		exec_query("ROLLBACK");
	}
	else
		exec_query("COMMIT");

	return rc;
}

int db_SQL_Base::set_config(const std::string key, const std::string value)
{
	std::stringstream sql;
	int rc = SQL_OK;

	sql << "INSERT INTO Config(`Key`,`Value`) VALUES('" << key << "','" << value << "') ON DUPLICATE KEY UPDATE `Value`='" << value << "'";

	if ((rc = exec_query(sql.str())) != SQL_OK)
		print_error("exec_query() returned", sql.str());

	return rc;
}

int db_SQL_Base::get_config(const std::string key, std::string &value)
{
	std::stringstream sql;
	int rc = SQL_OK;

	sql << "SELECT `Value` FROM Config WHERE `Key`='" << key << "'";

	rc = mysql_query(m_dbHandle, sql.str().c_str());

	if (rc == SQL_OK)
	{
		MYSQL_RES *sqlResult = mysql_store_result(m_dbHandle);
		MYSQL_ROW sqlRow = mysql_fetch_row(sqlResult);
		while (sqlRow)
		{
			value = (char *)sqlRow[0];
			sqlRow = mysql_fetch_row(sqlResult);
		}

        if(sqlResult)
        {
            mysql_free_result(sqlResult);
            sqlResult = NULL;
        }
	}

	return rc;
}

int db_SQL_Base::get_config(const std::string key, int &value)
{
	int rc = SQL_OK;
	std::string strValue = intToString(value);
	if((rc = get_config(key, strValue)) == SQL_OK)
		value = boost::lexical_cast<int>(strValue);

	return rc;
}

std::string db_SQL_Base::timestamp(void)
{
    char buffer[100];
#if !defined WIN32
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm *tm;
    tm = localtime(&tv.tv_sec);

    snprintf(buffer, sizeof(buffer), "[%04d-%02d-%02d %02d:%02d:%02d.%03d] ",
             tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
             tm->tm_hour, tm->tm_min, tm->tm_sec, (int)tv.tv_usec / 1000);
#else
    SYSTEMTIME time;
    ::GetLocalTime(&time);

    sprintf_s(buffer, sizeof(buffer), "[%04d-%02d-%02d %02d:%02d:%02d.%03d] ", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond, time.wMilliseconds);
#endif

	std::string sTimestamp(buffer);
	return sTimestamp;
}

#endif // #if defined(USE_MYSQL)
