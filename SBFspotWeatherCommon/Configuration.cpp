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

#define VERSION "1.0.0"

#include "Configuration.h"
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

using namespace std;

std::string errlevelText[] = {"", "INFO", "WARNING", "ERROR"};

Configuration::Configuration()
{
	m_PrgVersion = VERSION;

	// Default delay between 2 requests (seconds)
	m_ApiDelay = 600;
}

int Configuration::readSettings(std::wstring wme, std::wstring wfilename)
{
	std::string me(wme.begin(), wme.end());
	std::string filename(wfilename.begin(), wfilename.end());
	return readSettings(me,filename);
}

int Configuration::readSettings(std::string me, std::string filename)
{
	// Get path of executable
	m_AppPath = me;
	size_t pos = m_AppPath.find_last_of("/\\");
	if (pos != string::npos)
		//SBFspot started from absolute or relative path
		m_AppPath.erase(++pos);
	else
		//SBFspot started from curdir
		m_AppPath.clear();

	if (filename.empty())
		m_ConfigFile = m_AppPath + "SBFspotWeather.cfg";
	else
		m_ConfigFile = filename;

	m_fs.open(m_ConfigFile.c_str(), std::ifstream::in);

	if (!m_fs.is_open())
    {
		print_error("Could not open file " + m_ConfigFile);
		m_Status = CFG_ERROR;
		return m_Status;
	}

	if (verbose >= 2)
		cout << "Reading config '" << m_ConfigFile << "'\n";

	std::string line;
	unsigned int lineCnt = 0;

	m_Status = CFG_OK;
	while((m_Status == CFG_OK) && getline(m_fs, line))
	{
		lineCnt++;

		//Get rid of comments and empty lines
		size_t hashpos = line.find_first_of("#\r");
		if (hashpos != string::npos) line.erase(hashpos);

		if (line.size() > 0)
		{
			//Split line key=value
			std::string key;
			std::string value;

			const size_t equals_idx = line.find_first_of('=');
			if (std::string::npos != equals_idx)
			{
				key = line.substr(0, equals_idx);
				value = line.substr(equals_idx + 1);
			}
			else
			{
				print_error("Syntax error", lineCnt, m_ConfigFile);
			}

			try
			{
				boost::to_lower(key);

				if (key == "logdir")
				{
					// Append terminating (back)slash if needed
					m_LogDir = value;
					int len = m_LogDir.length();
					if (len > 0)
					{
						if ((m_LogDir.substr(len - 1, 1) != "\\") && (m_LogDir.substr(len - 1, 1) != "/"))
							m_LogDir += "/";
					}
				}

				else if (key == "verbose_level")
				{
					verbose = boost::lexical_cast<int>(value);
					if (verbose < 0) verbose = 0;
					if (verbose > 5) verbose = 5;
				}

				else if (key == "debug_level")
				{
					debug = boost::lexical_cast<int>(value);
					if (debug < 0) debug = 0;
					if (debug > 5) debug = 5;
				}

				else if (key == "apiendpoint")
					m_ApiEndpoint = value;

				else if (key == "apiparameters")
					m_ApiParameters = value;

				else if (key == "apikey")
					m_ApiKey = value;

				else if (key == "apidelay")
					m_ApiDelay = boost::lexical_cast<int>(value);

				else if (key == "sql_database")
					m_SqlDatabase = value;
#if defined(USE_MYSQL)
				else if (key == "sql_hostname")
					m_SqlHostname = value;

				else if (key == "sql_username")
					m_SqlUsername = value;

				else if (key == "sql_password")
					m_SqlUserPassword = value;
#endif
				else
					cerr << "WARNING: Ignoring '" << key << "'" << endl;
			} // try
			catch (...)
			{
				// Most likely a conversion error
				print_error("Syntax error", lineCnt, m_ConfigFile);
				m_Status = CFG_ERROR;
			}
        }
    } //while

	m_fs.close();

	if (m_Status == CFG_OK)
	{
		// Perform some checks...
		if (m_LogDir.empty()) { print_error("Missing 'LogDir'"); m_Status = CFG_ERROR; }
		else if (m_ApiEndpoint.empty()) { print_error("Missing 'ApiEndPoint'"); m_Status = CFG_ERROR; }
		else if (m_ApiParameters.empty()) { print_error("Missing 'ApiParameters'"); m_Status = CFG_ERROR; }
		else if (m_ApiKey.empty()) { print_error("Missing 'ApiKey'"); m_Status = CFG_ERROR; }
		else if (m_SqlDatabase.empty()) { print_error("Missing 'SQL_Database'"); m_Status = CFG_ERROR; }
	}

	return m_Status;
}

Configuration::~Configuration()
{
	if (m_fs.is_open()) m_fs.close();
}

