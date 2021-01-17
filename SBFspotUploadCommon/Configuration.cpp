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

#include "Configuration.h"
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

using namespace std;

std::string errlevelText[] = {"", "DEBUG", "INFO", "WARNING", "ERROR"};


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
		m_ConfigFile = m_AppPath + "SBFspotUpload.cfg";
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
		boost::trim(line);
		size_t hashpos = line.find_first_of("#\r");
		if (hashpos != string::npos) line.erase(hashpos);

		if (line.size() > 0)
		{
			//Split line key=value
			vector<std::string> lineparts;
			boost::split(lineparts, line, boost::is_any_of("="));
			if (lineparts.size() != 2)
				print_error("Syntax error", lineCnt, m_ConfigFile);
			else
			{
				try
				{
					boost::to_lower(lineparts[0]);
					std::string lcValue = boost::to_lower_copy(lineparts[1]);

					if (lineparts[0] == "logdir")
					{
						// Append terminating (back)slash if needed
						m_LogDir = lineparts[1];
						int len = m_LogDir.length();
						if (len > 0)
						{
							if ((m_LogDir.substr(len-1, 1) != "\\") && (m_LogDir.substr(len-1, 1) != "/"))
								m_LogDir += "/";
						}
					}

					else if (lineparts[0] == "loglevel")
					{
						if (lcValue == "debug") m_LogLevel = LOG_DEBUG_;
						else if (lcValue == "info") m_LogLevel = LOG_INFO_;
						else if (lcValue == "warning") m_LogLevel = LOG_WARNING_;
						else if (lcValue == "error") m_LogLevel = LOG_ERROR_;
						else
						{
							print_error("Syntax error", lineCnt, m_ConfigFile);
							m_Status = CFG_ERROR;
							break;
						}
					}

					else if (lineparts[0] == "pvoutput_sid")
					{
						vector<std::string> systems;
						boost::split(systems, lineparts[1], boost::is_any_of(","));
						{
							for (unsigned int i = 0; i < systems.size(); i++)
							{
								vector<std::string> sys_serial;
								boost::split(sys_serial, systems[i], boost::is_any_of(":"));
								if (sys_serial.size() != 2)
								{
									print_error("Syntax error", lineCnt, m_ConfigFile);
									m_Status = CFG_ERROR;
									break;
								}
								else
								{
									try
									{
										SMASerial Serial = boost::lexical_cast<SMASerial>(sys_serial[0]);
										PVOSystemID SID = boost::lexical_cast<PVOSystemID>(sys_serial[1]);
										m_PvoSIDs[Serial] = SID;
									}
									catch(...)
									{
										print_error("Syntax error", lineCnt, m_ConfigFile);
										m_Status = CFG_ERROR;
										break;
									}
								}
							}
						}
					}

					else if (lineparts[0] == "pvoutput_key")
						m_PvoAPIkey = lineparts[1];

					else if (lineparts[0] == "sql_database")
						m_SqlDatabase = lineparts[1];
#if defined(USE_MYSQL)
					else if (lineparts[0] == "sql_hostname")
						m_SqlHostname = lineparts[1];

					else if (lineparts[0] == "sql_username")
						m_SqlUsername = lineparts[1];

					else if (lineparts[0] == "sql_password")
						m_SqlUserPassword = lineparts[1];
					else if (lineparts[0] == "sql_port")
						try
						{
							m_SqlPort = boost::lexical_cast<unsigned int>(lineparts[1]);
						}
						catch (...)
						{
							print_error("Syntax error", lineCnt, m_ConfigFile);
							m_Status = CFG_ERROR;
							break;
						}
#endif
					else
						cerr << "WARNING: Ignoring '" << lineparts[0] << "'" << endl;
				} // try
				catch (...)
				{
					// Most likely a conversion error
					print_error("Syntax error", lineCnt, m_ConfigFile);
					m_Status = CFG_ERROR;
				}
            }
        }
    } //while

	m_fs.close();

	if (m_Status == CFG_OK)
	{
		// Perform some checks...
		if (m_LogDir.empty()) { print_error("Missing 'LogDir'"); m_Status = CFG_ERROR; }
		else if (m_PvoSIDs.size() == 0) { print_error("Missing 'PVoutput_SID'"); m_Status = CFG_ERROR; }
		else if (m_PvoAPIkey.empty()) { print_error("Missing 'PVoutput_Key'"); m_Status = CFG_ERROR; }
		else if (m_SqlDatabase.empty()) { print_error("Missing 'SQL_Database'"); m_Status = CFG_ERROR; }
	}

	return m_Status;
}

Configuration::~Configuration()
{
	if (m_fs.is_open()) m_fs.close();
}

