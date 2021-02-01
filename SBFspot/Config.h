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

#pragma once

#include "osselect.h"
#include "Types.h"

#include <map>
#include <boost/date_time/local_time/local_time.hpp>

struct ArrayConfig
{
    float azimuth = 180.0f;
    float elevation = 30.0f;
    float peakPower = 10000.0f;
};

struct Config
{
    int parseCmdline(int argc, char **argv);
    int readConfig();
    void showConfig();
    void sayHello(int ShowHelp);
    void invalidArg(char *arg);

    std::string	ConfigFile;			//Fullpath to configuration file
    std::string	AppPath;
    char	BT_Address[18];			//Inverter bluetooth address 12:34:56:78:9A:BC
    char	IP_Address[16];			//Inverter IP address 192.168.178.123 (for Speedwirecommunication)
    std::vector<std::string> ip_addresslist; //List of Inverter IP addresses (for Speedwirecommunication )
    int		BT_Timeout = 5;
    int		BT_ConnectRetries = 10;
    short   IP_Port;
    CONNECTIONTYPE ConnectionType;     // CT_BLUETOOTH | CT_ETHERNET
    char	SMA_Password[13];
    float	latitude = 0.0f;
    float	longitude = 0.0f;
    std::map<uint32_t, std::vector<ArrayConfig>> arrays;    // Module array configuration per inverter serial
    uint16_t liveInterval = 60;
    uint16_t archiveInterval = 300;
    time_t	archdata_from = 0;
    time_t	archdata_to = 0;
    char	delimiter = ';';    // CSV field delimiter
    int		precision = 3;      // CSV value precision
    char	decimalpoint = ','; // CSV decimal point
    char	outputPath[MAX_PATH];
    char	outputPath_Events[MAX_PATH];
    std::string	plantname;
    std::string sqlDatabase;
    std::string sqlHostname;
    std::string sqlUsername;
    std::string sqlUserPassword;
    unsigned int sqlPort;
    int		synchTime;				// 1=Synch inverter time with computer time (default=0)
    float	sunrise;
    float	sunset;
    int		calcMissingSpot = 0;    // 0-1
    char	DateTimeFormat[32];
    char	DateFormat[32];
    char	TimeFormat[32];
    int		CSV_Export;
    int		CSV_Header;
    int		CSV_ExtendedHeader;
    int		CSV_SaveZeroPower;
    int		SunRSOffset;			// Offset to start before sunrise and end after sunset
    int		userGroup;				// USER|INSTALLER
    char	prgVersion[16];
    int		SpotTimeSource;			// 0=Use inverter time; 1=Use PC time in Spot CSV
    int		SpotWebboxHeader;		// 0=Use standard Spot CSV hdr; 1=Webbox style hdr
    char	locale[6];				// default en-US
    int		MIS_Enabled;			// Multi Inverter Support
    std::string	timezone;
    boost::local_time::time_zone_ptr tz;
    int		synchTimeLow;			// settime low limit
    int		synchTimeHigh;			// settime high limit

    // MQTT Stuff -- Using mosquitto (https://mosquitto.org/)
    std::string mqtt_publish_exe;	// default /usr/bin/mosquitto_pub ("%ProgramFiles%\mosquitto\mosquitto_pub.exe" on Windows)
    std::string mqtt_host;			// default localhost
    std::string mqtt_port;			// default 1883 (8883 for MQTT over TLS)
    std::string mqtt_topic;			// default sbfspot
    std::string mqtt_publish_args;	// default and optional arguments for mosquitto_pub (-d for debug messages)
    std::string mqtt_publish_data;	// comma delimited list of spot data to publish (Timestamp,Serial,MeteringDyWhOut,GridMsTotW,...)
    std::string mqtt_item_format;   // default "{key}": {value}
    std::string mqtt_item_delimiter;// default comma

    //Commandline settings
    int		debug;				// -d			Debug level (0-5)
    int		verbose;			// -v			Verbose output level (0-5)
    int		archDays;			// -ad			Number of days back to get Archived DayData (0=disabled, 1=today, ...)
    int		archMonths;			// -am			Number of months back to get Archived MonthData (0=disabled, 1=this month, ...)
    int		archEventMonths;	// -ae			Number of months back to get Archived Events (0=disabled, 1=this month, ...)
    int		forceInq;			// -finq		Inquire inverter also during the night
    int		wsl;				// -wsl			WebSolarLog support (http://www.websolarlog.com/index.php/tag/sma-spot/)
    int		quiet;				// -q			Silent operation (No output except for -wsl)
    int		nocsv;				// -nocsv		Disables CSV export (Overrules CSV_Export in config)
    int		nospot;				// -sp0			Disables Spot CSV export
    int		nosql;				// -nosql		Disables SQL export
    int		loadlive;			// -loadlive	Force settings to prepare for live loading to http://pvoutput.org/loadlive.jsp
    time_t	startdate;			// -startdate	Start reading of historic data at the given date (YYYYMMDD)
    S123_COMMAND	s123;		// -123s		123Solar Web Solar logger support(http://www.123solar.org/)
    int		settime;			// -settime		Set plant time
    int		mqtt;				// -mqtt		Publish spot data to mqtt broker
    bool	ble = false;		// -ble			Publish spot data via Bluetooth LE
    bool	loop = false;		// -loop		Run SBF spot in a loop (daemon mode)
};
