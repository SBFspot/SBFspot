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
#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <sstream>
#include <fstream>

extern int quiet;
extern int verbose;

extern std::string errlevelText[];

typedef enum
{
	LOG_NONE_ = 0,
	LOG_INFO_,
	LOG_WARNING_,
	LOG_ERROR_
} ERRLEVEL;

class Configuration
{
public:
	enum {CFG_OK = 0, CFG_ERROR = -1};

private:
	std::string	m_PrgVersion;
	int			m_Status;
	std::string	m_ConfigFile;			//Fullpath to configuration file
	std::string	m_AppPath;
	std::string m_LogDir;
    std::string m_SqlDatabase;
    std::string m_SqlHostname;
    std::string m_SqlUsername;
    std::string m_SqlUserPassword;
	std::string	m_ApiEndpoint;
	std::string m_ApiParameters;
	std::string m_ApiKey;
	int         m_ApiDelay;

	std::ifstream m_fs;
	
	//Commandline settings
	int		debug;				// -d			Debug level (0-5)
	int		verbose;			// -v			Verbose output level (0-5)

public:
	Configuration(void);
	~Configuration();
	int readSettings(std::string me, std::string filename);
	int readSettings(std::wstring wme, std::wstring wfilename);
	std::string Version() const { return m_PrgVersion; }
	int Status() const { return m_Status; }
	std::string AppPath() const { return m_AppPath; }
	std::string LogDir() const { return m_LogDir; }
	std::string SqlDatabase() const { return m_SqlDatabase; }
	std::string SqlHostname() const { return m_SqlHostname; }
	std::string SqlUsername() const { return m_SqlUsername; }
	std::string SqlPassword() const { return m_SqlUserPassword; }
	std::string ApiEndpoint() const { return m_ApiEndpoint; }
	std::string ApiParameters() const { return m_ApiParameters; }
	std::string ApiKey() const { return m_ApiKey; }
	int ApiDelay() const { return m_ApiDelay; }

private:
	bool isverbose(int level)
	{
		return !quiet && (verbose >= level);
	}
	void print_error(std::string msg)
	{
		std::cerr << "Configuration Error: " << msg << std::endl;
	}
	void print_error(std::string msg, unsigned int line, std::string fpath)
	{
		std::cerr << "Configuration Error: " << msg << " on line " << line << " [" << fpath << "]\n";
	}

};
