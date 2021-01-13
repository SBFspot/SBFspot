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

************************************************************************************************/

#include "../SBFspotUploadCommon/CommonServiceCode.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <syslog.h>
#include <getopt.h>
#include <sstream>
#include <vector>
#include <string.h>

int quiet;
int verbose;

bool bStopping = false;
Configuration cfg;

// Signal handler
void handler(int signum)
{
    bStopping = true;
    syslog(LOG_INFO | LOG_USER, "Exit signal received.");
}

void pvo_upload(void)
{
	Log("Starting Daemon...", LOG_INFO_);

	CommonServiceCode();

	Log("Stopping Daemon...", LOG_INFO_);
}

int main(int argc, char *argv[])
{
	int c;
	const char *config_file = "";

	/* parse commandline */
	while (1)
	{
		static struct option long_options[] =
		{
			{ "config-file", required_argument, 0, 'c' },
			{ 0, 0, 0, 0 }
		};

		int option_index = 0;
		c = getopt_long(argc, argv, "c:", long_options, &option_index);

		if (c == -1) break;

		switch (c)
		{
		case 'c':
			config_file = optarg;
			break;
		default:
			return EXIT_FAILURE;
			break;
		}
	}

	if (cfg.readSettings(argv[0], config_file) != Configuration::CFG_OK)
		return EXIT_FAILURE;

	std::clog << "SBFspotUploadDaemon Version " << cfg.getVersion();
	std::clog << "\nLoglevel=" << cfg.getLogLevel();
	std::clog << "\nLogDir=" << cfg.getLogDir() << std::endl;
	
	Log("SBFspotUploadDaemon Version " + cfg.getVersion(), LOG_INFO_);

	// Check if DB is accessible
	db_SQL_Base db;
#if defined(USE_MYSQL)
	db.open(cfg.getSqlHostname(), cfg.getSqlUsername(), cfg.getSqlPassword(), cfg.getSqlDatabase(), cfg.getSqlPort());
#elif defined(USE_SQLITE)
	db.open(cfg.getSqlDatabase());
#endif
	if (!db.isopen())
	{
		std::clog << "Unable to open database. Check configuration." << std::endl;
		return EXIT_FAILURE;
	}

	// Check DB Version
	int schema_version = 0;
	db.get_config(SQL_SCHEMAVERSION, schema_version);
	db.close();

	if (schema_version < SQL_MINIMUM_SCHEMA_VERSION)
	{
		std::clog << "Upgrade your database to version " << SQL_MINIMUM_SCHEMA_VERSION << std::endl;
		return EXIT_FAILURE;
	}

	// Install our signal handler.
	// This responds to the service manager signalling the service to stop.
	signal(SIGTERM, handler);

	// Start daemon loop
	pvo_upload();

	return EXIT_SUCCESS;
}
