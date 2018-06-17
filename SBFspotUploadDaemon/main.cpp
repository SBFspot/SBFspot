/************************************************************************************************
	SBFspot - Yet another tool to read power production of SMA solar inverters
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

************************************************************************************************/

// This project is based on "A simple C/C++ database daemon" and "Boost.org examples"
// See http://kahimyang.info/kauswagan/code-blogs/1326/a-simple-cc-database-daemon
// and http://www.boost.org/doc/libs/1_48_0/doc/html/boost_asio/example/fork/daemon.cpp

#define VERSION "1.3.0"

// Fixed Issue 93: Add PID-File for SBFspotUploadDaemon (by wpaesen)
// Fixed Issue #GH218: .out file not deleted when daemon stops
// Fixed Issue #GH224: SBFSpotUpload.cfg - log level doesn't work

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

/* write with error checking */
void xwrite(int fd, char *buffer, int count)
{
	while (count > 0)
	{
		int n = write (fd, buffer, count);

		if (n == -1)
		{
			if (errno != EINTR)
			{
				Log("Writing PID file failed", LOG_ERROR_);
				exit(EXIT_FAILURE);
			}

			n = 0;
		}

		buffer += n;
		count -= n;
	}
}

int pidof(const char *ps_name, std::vector<int> &pids)
{
	const int BUFSIZE = 256;
    FILE *fp;

    char iobuffer[BUFSIZE + 7] = "pidof ";

    strncat(iobuffer, ps_name, BUFSIZE);

    if (!(fp = popen(iobuffer, "r")))
        return -1; // Error

    iobuffer[0] = 0;
    fgets(iobuffer, BUFSIZE, fp);

    if (pclose(fp) == -1)
        return -1; // Error

    std::istringstream iss(iobuffer);
    std::string s;

    while (getline(iss, s, ' '))
    {
        pids.push_back(atoi(s.c_str()));
    }

    return pids.size();
}

int is_daemon_running(const char *d_name)
{
    std::vector<int> pids;
    int num_ps = pidof(d_name, pids);
    return (num_ps > 1);
}

int main(int argrc, char *argv[])
{
	int c, pidfd = -1, rc = 0;
	const char *config_file = "";
	const char *pid_file = NULL;

	/* parse commandline */
	while(1)
	{
		static struct option long_options[] =
		{
			{ "config-file", required_argument, 0, 'c' },
			{ "pid-file"   , required_argument, 0, 'p' },
			{ 0, 0, 0, 0 }
		};

		int option_index = 0;
		c = getopt_long (argrc, argv, "c:p:", long_options, &option_index);
 
		if (c == -1) break;

		switch (c)
		{
			case 'c':
				config_file = optarg;
				break;
			case 'p':
				pid_file = optarg;
				break;
			default:
				exit(EXIT_FAILURE);
				break;
		}
	}
	
	if (cfg.readSettings(argv[0], config_file) != Configuration::CFG_OK)
        exit(EXIT_FAILURE);

    // Check if log is writable
    if (Log("Starting SBFspotUploadDaemon Version " + std::string(VERSION), LOG_INFO_) != 0)
	{
        exit(EXIT_FAILURE);
	}

	if (is_daemon_running(argv[0]))
	{
		printf("X:Daemon already running - Exiting now.\n");
		Log("Daemon already running - Exiting now.", LOG_INFO_);
		exit(EXIT_FAILURE);
	}
	
    // Check if DB is accessible
	db_SQL_Base db = db_SQL_Base();
	db.open(cfg.getSqlHostname(), cfg.getSqlUsername(), cfg.getSqlPassword(), cfg.getSqlDatabase());
	if (!db.isopen())
	{
        Log("Unable to open database. Check configuration.", LOG_ERROR_);
        exit(EXIT_FAILURE);
	}

    // Check DB Version
    int schema_version = 0;
    db.get_config(SQL_SCHEMAVERSION, schema_version);
    db.close();

    if (schema_version < SQL_MINIMUM_SCHEMA_VERSION)
	{
	    std::stringstream msg;
	    msg << "Upgrade your database to version " << SQL_MINIMUM_SCHEMA_VERSION;
        Log(msg.str().c_str(), LOG_ERROR_);
        exit(EXIT_FAILURE);
	}

		/* try to open PID file */
		if (pid_file) 
			{
				pidfd = open(pid_file, O_WRONLY | O_CREAT, 0644);
				if (pidfd == -1) 
					{
						std::string logline = "Could not open pid file ";
						logline.append(pid_file);
						logline.append(" : ");
						logline.append(strerror(errno));
						
						Log(logline, LOG_ERROR_);
						exit(EXIT_FAILURE);
					}
			}

    // Create child process
    pid_t pid = fork();

    if (pid < 0)
    {
        Log("Could not create child process.", LOG_ERROR_);
        exit(EXIT_FAILURE);
    }
    // Child process created, exit parent process
    if (pid > 0)
    {
			// write pid file if requested
			if (pidfd != -1) 
				{
					char pidbuf[64];
					snprintf(pidbuf, sizeof(pidbuf), "%d\n", pid);
					xwrite(pidfd, pidbuf, strlen(pidbuf));
					close(pidfd);
				}
			
        exit(EXIT_SUCCESS);
    }

		// pidfd inherited by child process, close it
		if (pidfd != -1) close(pidfd);
		
    // Set file permission for files created by our child process
    umask(0);

    // Change to root directory to prevent mounted filesystem from being unmounted.
    if ((rc = chdir("/")) != 0)
        Log("Could not change directory.", LOG_WARNING_);

    // Create session for our new process
    pid_t sid = setsid();
    if (sid < 0)
    {
        Log("Could not create session for child process.", LOG_ERROR_);
        exit(EXIT_FAILURE);
    }

    // close all standard file descriptors.
    fclose(stdin);
    fclose(stdout);
    fclose(stderr);

    // We don't want the daemon to have any standard input.
    if (open("/dev/null", O_RDONLY) < 0)
    {
        Log("Unable to open /dev/null. See syslog for details.", LOG_ERROR_);
        syslog(LOG_ERR | LOG_USER, "Unable to open /dev/null: %m");
        exit(EXIT_FAILURE);
    }

    // Send standard output to a log file.
    const char* output = "/tmp/SBFspotUploadDaemon.out";
    const int flags = O_WRONLY | O_CREAT | O_APPEND;
    const mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    if (open(output, flags, mode) < 0)
    {
        Log("Unable to open output file. See syslog for details.", LOG_ERROR_);
        syslog(LOG_ERR | LOG_USER, "Unable to open output file %s: %m", output);
        exit(EXIT_FAILURE);
    }

    // Also send standard error to the same log file.
    if (dup(1) < 0)
    {
        Log("Unable to dup output descriptor. See syslog for details.", LOG_ERROR_);
        syslog(LOG_ERR | LOG_USER, "Unable to dup output descriptor: %m");
        exit(EXIT_FAILURE);
    }

    // Install our signal handler.
    // This responds to kill [pid] from the command line
    signal(SIGTERM, handler);

    // Ignore signal when terminal session is closed.
    // This keeps our daemon alive even when user closed terminal session
    signal(SIGHUP, SIG_IGN);

    // Start daemon loop
    pvo_upload();

	// fix #GH218
	try
	{
		remove(output);
	}
	catch (...)
	{
		Log("Unable to delete " + std::string(output), LOG_WARNING_);
	}

    exit(EXIT_SUCCESS);
}

