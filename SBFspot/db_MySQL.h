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

#if defined(USE_MYSQL)

#include "osselect.h"
#include "SBFspot.h"
#include <mysql/mysql.h>

extern int quiet;
extern int verbose;

#define SQL_NEXTSTATUSCHECK		"NextStatusCheck"
#define SQL_SCHEMAVERSION		"SchemaVersion"
#define SQL_BATCH_DATELIMIT		"Batch_DateLimit"
#define SQL_BATCH_STATUSLIMIT	"Batch_StatusLimit"

#define SQL_MINIMUM_SCHEMA_VERSION 1
#define SQL_RECOMMENDED_SCHEMA_VERSION 1

class db_SQL_Base
{
public:
	enum
	{
		SQL_OK = 0,
		SQL_ERROR = 1
	};

private:
    std::string m_errortext;

protected:
	MYSQL *m_dbHandle;
	std::string m_database;

public:
	db_SQL_Base() { m_dbHandle = NULL; }
	~db_SQL_Base() { if (m_dbHandle) close(); }
	int open(std::string server, std::string user, std::string pass, std::string database);
	int close(void);
	int exec_query(std::string qry);
	std::string errortext(void) const { return m_errortext; }
	bool isopen(void) { return (m_dbHandle != NULL); }
	int type_label(InverterData *inverters[]);
	int device_status(InverterData *inverters[], time_t spottime);
	int batch_get_archdaydata(std::string &data, unsigned int Serial, int datelimit, int statuslimit, int& recordcount);
	int batch_set_pvoflag(const std::string &data, unsigned int Serial);
	int set_config(const std::string key, const std::string value);
	int get_config(const std::string key, std::string &value);
	int get_config(const std::string key, int &value);
	std::string intToString(const int i) { return static_cast<std::ostringstream*>( &(std::ostringstream() << i) )->str(); }

protected:
	std::string s_quoted(std::string str) { return "'" + str + "'"; }
	std::string s_quoted(char *str) { return "'" + std::string(str) + "'"; }
	bool isverbose(int level) { return !quiet && (verbose >= level); }
	std::string status_text(int status);
	void print_error(std::string msg) { std::cerr << timestamp() << "Error: " << msg << " : " << (m_dbHandle != NULL ? mysql_error(m_dbHandle) : "null") << std::endl; }
	void print_error(std::string msg, std::string sql) { std::cerr << timestamp() << "Error: " << msg << " : " << (m_dbHandle != NULL ? mysql_error(m_dbHandle) : "null") << "\nExecuted Statement: " << sql << std::endl; }
	std::string strftime_t(time_t utctime) { return static_cast<std::ostringstream*>( &(std::ostringstream() << utctime) )->str(); }
	std::string timestamp(void);
};

#endif //#if defined(USE_MYSQL)
