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

#if defined(USE_SQLITE)

#include "db_SQLite_Weather.h"
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

int db_SQL_Weather::insert_data(Weather &weather)
{
	const char *sql = "INSERT INTO WeatherData(UTC,WeatherID,Temperature,Pressure,Humidity,Visibility,WindSpeed,WindDirection,PVoutput) VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9)";
	int rc = SQLITE_OK;

	sqlite3_stmt* pStmt;
	if ((rc = sqlite3_prepare_v2(m_dbHandle, sql, strlen(sql), &pStmt, NULL)) == SQLITE_OK)
	{
		exec_query("BEGIN IMMEDIATE TRANSACTION");

		sqlite3_bind_int(pStmt, 1, weather.datetime());
		sqlite3_bind_int(pStmt, 2, weather.id());
		sqlite3_bind_double(pStmt, 3, weather.temperature());
		sqlite3_bind_double(pStmt, 4, weather.pressure());
		sqlite3_bind_double(pStmt, 5, weather.humidity());
		sqlite3_bind_double(pStmt, 6, weather.visibility());
		sqlite3_bind_double(pStmt, 7, weather.wind_speed());
		sqlite3_bind_double(pStmt, 8, weather.wind_direction());
		sqlite3_bind_null(pStmt, 9); // PVoutput

		rc = sqlite3_step(pStmt);
		if ((rc != SQLITE_DONE) && (rc != SQLITE_CONSTRAINT))
		{
			print_error("[day_data]sqlite3_step() returned");
		}
		else
		{
			sqlite3_clear_bindings(pStmt);
			sqlite3_reset(pStmt);
			rc = SQLITE_OK;

			sqlite3_finalize(pStmt);

			if (rc == SQLITE_OK)
				exec_query("COMMIT");
			else
			{
				print_error("[weather_data]Transaction failed. Rolling back now...");
				exec_query("ROLLBACK");
			}
		}
	}

	return rc;
}

int db_SQL_Weather::set_pvoflag(const std::string &data)
{
	std::stringstream sql;
	int rc = SQLITE_OK;

	std::vector<std::string> items;
	boost::split(items, data, boost::is_any_of(";"));

	sql << "UPDATE OR ROLLBACK WeatherData "
		"SET PVoutput=1 "
		"WHERE strftime('%Y%m%d,%H:%M',datetime(TimeStamp, 'unixepoch', 'localtime')) "
		"IN (";

	bool firstitem = true;
	for (std::vector<std::string>::iterator it = items.begin(); it != items.end(); ++it)
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

	if ((rc = exec_query(sql.str())) != SQLITE_OK)
		print_error("exec_query() returned", sql.str());

	return rc;
}

int db_SQL_Weather::get_temperature(std::string &data, int datelimit, int statuslimit, int& recordcount)
{
	std::stringstream sql;
	int rc = SQLITE_OK;
	recordcount = 0;

	sqlite3_stmt *pStmt = NULL;

	sql << "SELECT strftime('%Y%m%d,%H:%M',TimeStamp),V1,V2,V3,V4,V5,V6,V7,V8,V9,V10,V11,V12 FROM [vwPvoData] WHERE "
		"TimeStamp>DATE(DATE(),'" << -(datelimit - 2) << " day') "
		"AND PVoutput IS NULL "
		"ORDER BY TimeStamp "
		"LIMIT " << statuslimit;

	rc = sqlite3_prepare_v2(m_dbHandle, sql.str().c_str(), -1, &pStmt, NULL);

	if (pStmt != NULL)
	{
		std::stringstream result;
		while (sqlite3_step(pStmt) == SQLITE_ROW)
		{
			result.str("");

			std::string dt = (char *)sqlite3_column_text(pStmt, 0);
			// Energy Generation
			int64_t V1 = sqlite3_column_int64(pStmt, 1);
			// Power Generation
			int64_t V2 = sqlite3_column_int64(pStmt, 2);

			// from 2nd record, add a record separator
			if (!data.empty()) result << ";";

			// Mandatory values
			result << dt << "," << V1 << "," << V2;

			result << ",";
			// Energy Consumption
			if (sqlite3_column_type(pStmt, 3) != SQLITE_NULL)
				result << sqlite3_column_int64(pStmt, 3);

			result << ",";
			// Power Consumption
			if (sqlite3_column_type(pStmt, 4) != SQLITE_NULL)
				result << sqlite3_column_int64(pStmt, 4);

			result << ",";
			// Temperature
			if (sqlite3_column_type(pStmt, 5) != SQLITE_NULL)
				result << sqlite3_column_double(pStmt, 5);

			result << ",";
			// Voltage
			if (sqlite3_column_type(pStmt, 6) != SQLITE_NULL)
				result << sqlite3_column_double(pStmt, 6);

			// Extended values
			for (int extval = 7; extval <= 12; extval++)
			{
				result << ",";
				if (sqlite3_column_type(pStmt, extval) != SQLITE_NULL)
					result << sqlite3_column_double(pStmt, extval);
			}

			const std::string& str = result.str();
			int end = str.length();

			for (std::string::const_reverse_iterator it = str.rbegin(); it != str.rend(); ++it, end--)
			{
				if ((*it) != ',')
					break;
			}

			data.append(result.str().substr(0, end));
			recordcount++;
		}

		sqlite3_finalize(pStmt);
	}

	return rc;
}

#endif // #if defined(USE_SQLITE)
