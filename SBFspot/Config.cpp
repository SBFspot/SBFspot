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

#include "Config.h"

#include "SBFspot.h"
#include "version.h"

#include <boost/asio/ip/address.hpp>

#define MAX_CFG_AD 300	// Days
#define MAX_CFG_AM 300	// Months
#define MAX_CFG_AE 300	// Months

using namespace std;

int Config::parseCmdline(int argc, char **argv)
{
    this->debug = 0;			// debug level - 0=none, 5=highest
    this->verbose = 0;		// verbose level - 0=none, 5=highest
    this->archDays = 1;		// today only
    this->archMonths = 1;	// this month only
    this->archEventMonths = 1;	// this month only
    this->forceInq = 0;		// Inquire inverter also during the night
    this->userGroup = UG_USER;
    // WebSolarLog support (http://www.websolarlog.com/index.php/tag/sma-spot/)
    // This is an undocumented feature and should only be used for WebSolarLog
    this->wsl = 0;
    this->quiet = 0;
    this->nocsv = 0;
    this->nospot = 0;
    this->nosql = 0;
    // 123Solar Web Solar logger support(http://www.123solar.org/)
    // This is an undocumented feature and should only be used for 123solar
    this->s123 = S123_NOP;
    this->loadlive = 0;	//force settings to prepare for live loading to http://pvoutput.org/loadlive.jsp
    this->startdate = 0;
    this->settime = 0;
    this->mqtt = 0;

    bool help_requested = false;

    //Set quiet/help mode
    for (int i = 1; i < argc; i++)
    {
        if (*argv[i] == '/')
            *argv[i] = '-';

        if (stricmp(argv[i], "-q") == 0)
            this->quiet = 1;

        if (stricmp(argv[i], "-?") == 0)
            help_requested = true;
    }

    // Get path of executable
    // Fix Issue 169 (expand symlinks)
    this->AppPath = realpath(argv[0]);

    size_t pos = this->AppPath.find_last_of("/\\");
    if (pos != std::string::npos)
        this->AppPath.erase(++pos);
    else
        this->AppPath.clear();

    //Build fullpath to config file (SBFspot.cfg should be in same folder as SBFspot.exe)
    this->ConfigFile = this->AppPath + "SBFspot.cfg";

    char *pEnd = NULL;
    long lValue = 0;

    if ((this->quiet == 0) && (!help_requested))
    {
        sayHello(0);
        printf("Commandline Args:");
        for (int i = 1; i < argc; i++)
            printf(" %s", argv[i]);

        printf("\n");
    }

    for (int i = 1; i < argc; i++)
    {
        //Set #days (archived daydata)
        if (strnicmp(argv[i], "-ad", 3) == 0)
        {
            if (strlen(argv[i]) > 6)
            {
                invalidArg(argv[i]);
                return -1;
            }
            lValue = strtol(argv[i]+3, &pEnd, 10);
            if ((lValue < 0) || (lValue > MAX_CFG_AD) || (*pEnd != 0))
            {
                invalidArg(argv[i]);
                return -1;
            }
            else
                this->archDays = (int)lValue;

        }

        //Set #months (archived monthdata)
        else if (strnicmp(argv[i], "-am", 3) == 0)
        {
            if (strlen(argv[i]) > 6)
            {
                invalidArg(argv[i]);
                return -1;
            }
            lValue = strtol(argv[i]+3, &pEnd, 10);
            if ((lValue < 0) || (lValue > MAX_CFG_AM) || (*pEnd != 0))
            {
                invalidArg(argv[i]);
                return -1;
            }
            else
                this->archMonths = (int)lValue;
        }

        //Set #days (archived events)
        else if (strnicmp(argv[i], "-ae", 3) == 0)
        {
            if (strlen(argv[i]) > 6)
            {
                invalidArg(argv[i]);
                return -1;
            }
            lValue = strtol(argv[i]+3, &pEnd, 10);
            if ((lValue < 0) || (lValue > MAX_CFG_AE) || (*pEnd != 0))
            {
                invalidArg(argv[i]);
                return -1;
            }
            else
                this->archEventMonths = (int)lValue;

        }

        //Set debug level
        else if(strnicmp(argv[i], "-d", 2) == 0)
        {
            lValue = strtol(argv[i]+2, &pEnd, 10);
            if (strlen(argv[i]) == 2) lValue = 2;	// only -d sets medium debug level
            if ((lValue < 0) || (lValue > 5) || (*pEnd != 0))
            {
                invalidArg(argv[i]);
                return -1;
            }
            else
                this->debug = (int)lValue;
        }

        //Set verbose level
        else if (strnicmp(argv[i], "-v", 2) == 0)
        {
            lValue = strtol(argv[i]+2, &pEnd, 10);
            if (strlen(argv[i]) == 2) lValue = 2;	// only -v sets medium verbose level
            if ((lValue < 0) || (lValue > 5) || (*pEnd != 0))
            {
                invalidArg(argv[i]);
                return -1;
            }
            else
                this->verbose = (int)lValue;
        }

        //force settings to prepare for live loading to http://pvoutput.org/loadlive.jsp
        else if ((stricmp(argv[i], "-liveload") == 0) || (stricmp(argv[i], "-loadlive") == 0))
            this->loadlive = 1;

        //Set inquiryDark flag
        else if (stricmp(argv[i], "-finq") == 0)
            this->forceInq = 1;

        //Set WebSolarLog flag (Undocumented - For WSL usage only)
        else if (stricmp(argv[i], "-wsl") == 0)
            this->wsl = 1;

        //Set 123Solar command value (Undocumented - For WSL usage only)
        else if (strnicmp(argv[i], "-123s", 5) == 0)
        {
            if (strlen(argv[i]) == 5)
                this->s123 = S123_DATA;
            else if (strnicmp(argv[i]+5, "=DATA", 5) == 0)
                this->s123 = S123_DATA;
            else if (strnicmp(argv[i]+5, "=INFO", 5) == 0)
                this->s123 = S123_INFO;
            else if (strnicmp(argv[i]+5, "=SYNC", 5) == 0)
                this->s123 = S123_SYNC;
            else if (strnicmp(argv[i]+5, "=STATE", 6) == 0)
                this->s123 = S123_STATE;
            else
            {
                invalidArg(argv[i]);
                return -1;
            }
        }

        //Set NoCSV flag (Disable CSV export - Overrules Config setting)
        else if (stricmp(argv[i], "-nocsv") == 0)
            this->nocsv = 1;

        //Set NoSQL flag (Disable SQL export)
        else if (stricmp(argv[i], "-nosql") == 0)
            this->nosql = 1;

        //Set NoSpot flag (Disable Spot CSV export)
        else if (stricmp(argv[i], "-sp0") == 0)
            this->nospot = 1;

        else if (stricmp(argv[i], "-installer") == 0)
            this->userGroup = UG_INSTALLER;

        else if (strnicmp(argv[i], "-password:", 10) == 0)
            if (strlen(argv[i]) == 10)
            {
                invalidArg(argv[i]);
                return -1;
            }
            else
            {
                memset(this->SMA_Password, 0, sizeof(this->SMA_Password));
                strncpy(this->SMA_Password, argv[i] + 10, sizeof(this->SMA_Password) - 1);
            }

        else if (strnicmp(argv[i], "-startdate:", 11) == 0)
        {
            if (strlen(argv[i]) == 11)
            {
                invalidArg(argv[i]);
                return -1;
            }
            else
            {
                std::string dt_start(argv[i] + 11);
                if (dt_start.length() == 8)	//YYYYMMDD
                {
                    time_t start = time(NULL);
                    struct tm tm_start;
                    memcpy(&tm_start, localtime(&start), sizeof(tm_start));
                    tm_start.tm_year = atoi(dt_start.substr(0,4).c_str()) - 1900;
                    tm_start.tm_mon = atoi(dt_start.substr(4,2).c_str()) - 1;
                    tm_start.tm_mday = atoi(dt_start.substr(6,2).c_str());
                    this->startdate = mktime(&tm_start);
                    if (-1 == this->startdate)
                    {
                        invalidArg(argv[i]);
                        return -1;
                    }
                }
                else
                {
                    invalidArg(argv[i]);
                    return -1;
                }
            }
        }

        //look for alternative config file
        else if (strnicmp(argv[i], "-cfg", 4) == 0)
        {
            if (strlen(argv[i]) == 4)
            {
                invalidArg(argv[i]);
                return -1;
            }
            else
            {
                //Fix Issue G90 (code.google.com)
                //If -cfg arg has no '\' it's only a filename and should be in the same folder as SBFspot executable
                this->ConfigFile = argv[i] + 4;
                if (this->ConfigFile.find_first_of("/\\") == std::string::npos)
                    this->ConfigFile = this->AppPath + (argv[i] + 4);
            }
        }

        else if (stricmp(argv[i], "-settime") == 0)
            this->settime = 1;

        //Scan for bluetooth devices
        else if (stricmp(argv[i], "-scan") == 0)
        {
#ifdef WIN32
            bthSearchDevices();
#else
            puts("On LINUX systems, use hcitool scan");
#endif
            return 1;	// Caller should terminate, no error
        }

        else if (stricmp(argv[i], "-mqtt") == 0)
            this->mqtt = 1;

        else if (stricmp(argv[i], "-ble") == 0)
            this->ble = 1;

        else if (stricmp(argv[i], "-loop") == 0)
            this->loop = true;

        //Show Help
        else if (stricmp(argv[i], "-?") == 0)
        {
            sayHello(1);
            return 1;	// Caller should terminate, no error
        }

        else if (this->quiet == 0)
        {
            invalidArg(argv[i]);
            return -1;
        }

    }

    if (this->settime == 1)
    {
        // Verbose output level should be at least = 2 (normal)
        if (this->verbose < 2)
            this->verbose = 2;

        this->forceInq = 1;
    }

    //Disable verbose/debug modes when silent
    if (this->quiet == 1)
    {
        this->verbose = 0;
        this->debug = 0;
    }

    return 0;
}

int Config::readConfig()
{
    //Initialise config structure and set default values
    strncpy(this->prgVersion, VERSION, sizeof(this->prgVersion));
    memset(this->BT_Address, 0, sizeof(this->BT_Address));
    memset(this->IP_Address, 0, sizeof(this->IP_Address));
    this->outputPath[0] = 0;
    this->outputPath_Events[0] = 0;
    if (this->userGroup == UG_USER) this->SMA_Password[0] = 0;
    this->plantname[0] = 0;

    strcpy(this->DateTimeFormat, "%d/%m/%Y %H:%M:%S");
    strcpy(this->DateFormat, "%d/%m/%Y");
    strcpy(this->TimeFormat, "%H:%M:%S");
    this->synchTime = 1;
    this->CSV_Export = 1;
    this->CSV_ExtendedHeader = 1;
    this->CSV_Header = 1;
    this->CSV_SaveZeroPower = 1;
    this->SunRSOffset = 900;
    this->SpotTimeSource = 0;
    this->SpotWebboxHeader = 0;
    this->MIS_Enabled = 0;
    strcpy(this->locale, "en-US");
    this->synchTimeLow = 1;
    this->synchTimeHigh = 3600;
    // MQTT default values
    this->mqtt_host = "localhost";
    this->mqtt_port = ""; // mosquitto: 1883/8883 for TLS
#if defined(WIN32)
    this->mqtt_publish_exe = "%ProgramFiles%\\mosquitto\\mosquitto_pub.exe";
#else
    this->mqtt_publish_exe = "/usr/bin/mosquitto_pub";
#endif
    this->mqtt_topic = "sbfspot";
    this->mqtt_publish_args = "-h {host} -t {topic} -m \"{{message}}\"";
    this->mqtt_publish_data = "Timestamp,SunRise,SunSet,InvSerial,InvName,InvStatus,EToday,ETotal,PACTot,UDC1,UDC2,IDC1,IDC2,PDC1,PDC2";
    this->mqtt_item_format = "\"{key}\": {value}";

    this->sqlPort = 3306;

    const char *CFG_Boolean = "(0-1)";
    const char *CFG_InvalidValue = "Invalid value for '%s' %s\n";

    FILE *fp;
    if ((fp = fopen(this->ConfigFile.c_str(), "r")) == NULL)
    {
        std::cerr << "Error! Could not open file " << this->ConfigFile << std::endl;
        return -1;
    }

    if (this->verbose >= 2)
        std::cout << "Reading config '" << this->ConfigFile << "'" << std::endl;

    char *pEnd = NULL;
    long lValue = 0;

    // Quick fix #350 Limitation for MQTT keywords?
    // Input buffer increased from 200 to 512 bytes
    // TODO: Rewrite by using std::getline()
    char line[512];
    int rc = 0;

    while ((rc == 0) && (fgets(line, sizeof(line), fp) != NULL))
    {
        if (line[0] != '#' && line[0] != 0 && line[0] != 10)
        {
            char *variable = strtok(line,"=");
            char *value = strtok(NULL,"\n");

            if ((value != NULL) && (*rtrim(value) != 0))
            {
                if (stricmp(variable, "BTaddress") == 0)
                {
                    memset(this->BT_Address, 0, sizeof(this->BT_Address));
                    strncpy(this->BT_Address, value, sizeof(this->BT_Address) - 1);
                }
                else if(strnicmp(variable, "IP_Address", 10) == 0)
                {
                    boost::split(this->ip_addresslist, value, boost::is_any_of(","));
                    for (unsigned int i = 0; i < this->ip_addresslist.size(); i++)
                    {
                        try
                        {
                            boost::asio::ip::address ipv4Addr = boost::asio::ip::address::from_string(this->ip_addresslist[i]);
                            if (!ipv4Addr.is_v4())
                                throw -2;
                        }
                        catch (...)
                        {
                            std::cerr << "Invalid value for '" << variable << "' " << this->ip_addresslist[i] << std::endl;
                            rc = -2;
                            break;
                        }
                    }

                    if (rc == 0)
                    {
                        memset(this->IP_Address, 0, sizeof(this->IP_Address));
                        strncpy(this->IP_Address, this->ip_addresslist[0].c_str(), sizeof(this->IP_Address) - 1);
                    }
                }
                else if(stricmp(variable, "Password") == 0)
                {
                    if (this->userGroup == UG_USER)
                    {
                        memset(this->SMA_Password, 0, sizeof(this->SMA_Password));
                        strncpy(this->SMA_Password, value, sizeof(this->SMA_Password) - 1);
                    }
                }
                else if (stricmp(variable, "OutputPath") == 0)
                {
                    memset(this->outputPath, 0, sizeof(this->outputPath));
                    strncpy(this->outputPath, value, sizeof(this->outputPath) - 1);
                }
                else if (stricmp(variable, "OutputPathEvents") == 0)
                {
                    memset(this->outputPath_Events, 0, sizeof(this->outputPath_Events));
                    strncpy(this->outputPath_Events, value, sizeof(this->outputPath_Events) - 1);
                }
                else if(stricmp(variable, "Latitude") == 0) this->latitude = (float)atof(value);
                else if(stricmp(variable, "Longitude") == 0) this->longitude = (float)atof(value);
                else if(stricmp(variable, "Longitude") == 0) this->longitude = (float)atof(value);
                else if(stricmp(variable, "Longitude") == 0) this->longitude = (float)atof(value);
                else if(stricmp(variable, "Longitude") == 0) this->longitude = (float)atof(value);
                else if(stricmp(variable, "LiveInterval") == 0) this->liveInterval = (uint16_t)atoi(value);
                else if(stricmp(variable, "ArchiveInterval") == 0) this->archiveInterval = (uint16_t)atoi(value);
                else if (stricmp(variable, "Plantname") == 0) this->plantname = value;
                else if(stricmp(variable, "CalculateMissingSpotValues") == 0)
                {
                    lValue = strtol(value, &pEnd, 10);
                    if (((lValue == 0) || (lValue == 1)) && (*pEnd == 0))
                        this->calcMissingSpot = (int)lValue;
                    else
                    {
                        fprintf(stderr, CFG_InvalidValue, variable, CFG_Boolean);
                        rc = -2;
                    }
                }
                else if (stricmp(variable, "DatetimeFormat") == 0)
                {
                    memset(this->DateTimeFormat, 0, sizeof(this->DateTimeFormat));
                    strncpy(this->DateTimeFormat, value, sizeof(this->DateTimeFormat) - 1);
                }
                else if (stricmp(variable, "DateFormat") == 0)
                {
                    memset(this->DateFormat, 0, sizeof(this->DateFormat));
                    strncpy(this->DateFormat, value, sizeof(this->DateFormat)-1);
                }
                else if (stricmp(variable, "TimeFormat") == 0)
                {
                    memset(this->TimeFormat, 0, sizeof(this->TimeFormat));
                    strncpy(this->TimeFormat, value, sizeof(this->TimeFormat) - 1);
                }
                else if(stricmp(variable, "DecimalPoint") == 0)
                {
                    if (stricmp(value, "comma") == 0) this->decimalpoint = ',';
                    else if ((stricmp(value, "dot") == 0) || (stricmp(value, "point") == 0)) this->decimalpoint = '.'; // Fix Issue 84 - 'Point' is accepted for backward compatibility
                    else
                    {
                        fprintf(stderr, CFG_InvalidValue, variable, "(comma|dot)");
                        rc = -2;
                    }
                }
                else if(stricmp(variable, "CSV_Delimiter") == 0)
                {
                    if (stricmp(value, "comma") == 0) this->delimiter = ',';
                    else if (stricmp(value, "semicolon") == 0) this->delimiter = ';';
                    else
                    {
                        fprintf(stderr, CFG_InvalidValue, variable, "(comma|semicolon)");
                        rc = -2;
                    }
                }
                else if(stricmp(variable, "SynchTime") == 0)
                {
                    lValue = strtol(value, &pEnd, 10);
                    if (((lValue >= 0) && (lValue <= 30)) && (*pEnd == 0))
                        this->synchTime = (int)lValue;
                    else
                    {
                        fprintf(stderr, CFG_InvalidValue, variable, "(0-30)");
                        rc = -2;
                    }
                }
                else if(stricmp(variable, "SynchTimeLow") == 0)
                {
                    lValue = strtol(value, &pEnd, 10);
                    if ((lValue >= 1) && (lValue <= 120) && (*pEnd == 0))
                        this->synchTimeLow = (int)lValue;
                    else
                    {
                        fprintf(stderr, CFG_InvalidValue, variable, "(1-120)");
                        rc = -2;
                    }
                }
                else if(stricmp(variable, "SynchTimeHigh") == 0)
                {
                    lValue = strtol(value, &pEnd, 10);
                    if ((lValue >= 1200) && (lValue <= 3600) && (*pEnd == 0))
                        this->synchTimeHigh = (int)lValue;
                    else
                    {
                        fprintf(stderr, CFG_InvalidValue, variable, "(1200-3600)");
                        rc = -2;
                    }
                }
                else if(stricmp(variable, "CSV_Export") == 0)
                {
                    lValue = strtol(value, &pEnd, 10);
                    if (((lValue == 0) || (lValue == 1)) && (*pEnd == 0))
                        this->CSV_Export = (int)lValue;
                    else
                    {
                        fprintf(stderr, CFG_InvalidValue, variable, CFG_Boolean);
                        rc = -2;
                    }
                }
                else if(stricmp(variable, "CSV_ExtendedHeader") == 0)
                {
                    lValue = strtol(value, &pEnd, 10);
                    if (((lValue == 0) || (lValue == 1)) && (*pEnd == 0))
                        this->CSV_ExtendedHeader = (int)lValue;
                    else
                    {
                        fprintf(stderr, CFG_InvalidValue, variable, CFG_Boolean);
                        rc = -2;
                    }
                }
                else if(stricmp(variable, "CSV_Header") == 0)
                {
                    lValue = strtol(value, &pEnd, 10);
                    if (((lValue == 0) || (lValue == 1)) && (*pEnd == 0))
                        this->CSV_Header = (int)lValue;
                    else
                    {
                        fprintf(stderr, CFG_InvalidValue, variable, CFG_Boolean);
                        rc = -2;
                    }
                }
                else if(stricmp(variable, "CSV_SaveZeroPower") == 0)
                {
                    lValue = strtol(value, &pEnd, 10);
                    if (((lValue == 0) || (lValue == 1)) && (*pEnd == 0))
                        this->CSV_SaveZeroPower = (int)lValue;
                    else
                    {
                        fprintf(stderr, CFG_InvalidValue, variable, CFG_Boolean);
                        rc = -2;
                    }
                }
                else if(stricmp(variable, "SunRSOffset") == 0)
                {
                    lValue = strtol(value, &pEnd, 10);
                    if ((lValue >= 0) && (lValue <= 3600) && (*pEnd == 0))
                        this->SunRSOffset = (int)lValue;
                    else
                    {
                        fprintf(stderr, CFG_InvalidValue, variable, "(0-3600)");
                        rc = -2;
                    }
                }
                else if(stricmp(variable, "CSV_Spot_TimeSource") == 0)
                {
                    if (stricmp(value, "Inverter") == 0) this->SpotTimeSource = 0;
                    else if (stricmp(value, "Computer") == 0) this->SpotTimeSource = 1;
                    else
                    {
                        fprintf(stderr, CFG_InvalidValue, variable, "Inverter|Computer");
                        rc = -2;
                    }
                }

                else if(stricmp(variable, "CSV_Spot_WebboxHeader") == 0)
                {
                    lValue = strtol(value, &pEnd, 10);
                    if (((lValue == 0) || (lValue == 1)) && (*pEnd == 0))
                        this->SpotWebboxHeader = (int)lValue;
                    else
                    {
                        fprintf(stderr, CFG_InvalidValue, variable, CFG_Boolean);
                        rc = -2;
                    }
                }
                else if(stricmp(variable, "MIS_Enabled") == 0)
                {
                    lValue = strtol(value, &pEnd, 10);
                    if (((lValue == 0) || (lValue == 1)) && (*pEnd == 0))
                        this->MIS_Enabled = (int)lValue;
                    else
                    {
                        fprintf(stderr, CFG_InvalidValue, variable, CFG_Boolean);
                        rc = -2;
                    }
                }
                else if(stricmp(variable, "Locale") == 0)
                {
                    if ((stricmp(value, "de-DE") == 0) ||
                        (stricmp(value, "en-US") == 0) ||
                        (stricmp(value, "fr-FR") == 0) ||
                        (stricmp(value, "nl-NL") == 0) ||
                        (stricmp(value, "it-IT") == 0) ||
                        (stricmp(value, "es-ES") == 0)
                        )
                        strcpy(this->locale, value);
                    else
                    {
                        fprintf(stderr, CFG_InvalidValue, variable, "de-DE|en-US|fr-FR|nl-NL|it-IT|es-ES");
                        rc = -2;
                    }
                }
                else if(stricmp(variable, "BTConnectRetries") == 0)
                {
                    lValue = strtol(value, &pEnd, 10);
                    if ((lValue >= 0) && (lValue <= 15) && (*pEnd == 0))
                        this->BT_ConnectRetries = (int)lValue;
                    else
                    {
                        fprintf(stderr, CFG_InvalidValue, variable, "(1-15)");
                        rc = -2;
                    }
                }
                else if(stricmp(variable, "Timezone") == 0)
                {
                    this->timezone = value;
                    boost::local_time::tz_database tzDB;
                    string tzdbPath = this->AppPath + "date_time_zonespec.csv";
                    // load the time zone database which comes with boost
                    // file must be UNIX file format (line delimiter=CR)
                    // if not: bad lexical cast: source type value could not be interpreted as target
                    try
                    {
                        tzDB.load_from_file(tzdbPath);
                    }
                    catch (std::exception const&  e)
                    {
                        std::cout << e.what() << std::endl;
                        return -2;
                    }

                    // Get timezone info from the database
                    this->tz = tzDB.time_zone_from_region(value);
                    if (!this->tz)
                    {
                        cout << "Invalid timezone specified: " << value << endl;
                        return -2;
                    }
                }

                else if(stricmp(variable, "SQL_Database") == 0)
                    this->sqlDatabase = value;
#if defined(USE_MYSQL)
                else if(stricmp(variable, "SQL_Hostname") == 0)
                    this->sqlHostname = value;
                else if(stricmp(variable, "SQL_Username") == 0)
                    this->sqlUsername = value;
                else if(stricmp(variable, "SQL_Password") == 0)
                    this->sqlUserPassword = value;
                else if (stricmp(variable, "SQL_Port") == 0)
                    try
                        {
                        this->sqlPort = boost::lexical_cast<unsigned int>(value);
                        }
                        catch (...)
                        {
                            fprintf(stderr, CFG_InvalidValue, variable, "");
                            rc = -2;
                            break;
                        }
#endif
                else if (stricmp(variable, "MQTT_Host") == 0)
                    this->mqtt_host = value;
                else if (stricmp(variable, "MQTT_Port") == 0)
                    this->mqtt_port = value;
                else if (stricmp(variable, "MQTT_Publisher") == 0)
                    this->mqtt_publish_exe = value;
                else if (stricmp(variable, "MQTT_PublisherArgs") == 0)
                    this->mqtt_publish_args = value;
                else if (stricmp(variable, "MQTT_Topic") == 0)
                    this->mqtt_topic = value;
                else if (stricmp(variable, "MQTT_ItemFormat") == 0)
                    this->mqtt_item_format = value;
                else if (stricmp(variable, "MQTT_Data") == 0)
                    this->mqtt_publish_data = value;
                else if (stricmp(variable, "MQTT_ItemDelimiter") == 0)
                {
                    if (stricmp(value, "comma") == 0) this->mqtt_item_delimiter = ",";
                    else if (stricmp(value, "semicolon") == 0) this->mqtt_item_delimiter = ";";
                    else if (stricmp(value, "blank") == 0) this->mqtt_item_delimiter = " ";
                    else if (stricmp(value, "none") == 0) this->mqtt_item_delimiter = "";
                    else
                    {
                        fprintf(stderr, CFG_InvalidValue, variable, "(none|blank|comma|semicolon)");
                        rc = -2;
                    }
                }

                // Add more config keys here

                else
                    fprintf(stderr, "Warning: Ignoring keyword '%s'\n", variable);
            }
        }
    }
    fclose(fp);

    if (rc != 0) return rc;

    if (strlen(this->BT_Address) > 0)
        this->ConnectionType = CT_BLUETOOTH;
    else
    {
        this->ConnectionType = CT_ETHERNET;
        this->IP_Port = 9522;
    }

    if (strlen(this->SMA_Password) == 0)
    {
        fprintf(stderr, "Missing USER Password.\n");
        rc = -2;
    }

    if (this->decimalpoint == this->delimiter)
    {
        fprintf(stderr, "'CSV_Delimiter' and 'DecimalPoint' must be different character.\n");
        rc = -2;
    }

    //Overrule CSV_Export from config with Commandline setting -nocsv
    if (this->nocsv == 1)
        this->CSV_Export = 0;

    //Silently enable CSV_Header when CSV_ExtendedHeader is enabled
    if (this->CSV_ExtendedHeader == 1)
        this->CSV_Header = 1;

    if (strlen(this->outputPath) == 0)
    {
        fprintf(stderr, "Missing OutputPath.\n");
        rc = -2;
    }

    //If OutputPathEvents is omitted, use OutputPath
    if (strlen(this->outputPath_Events) == 0)
        strcpy(this->outputPath_Events, this->outputPath);

    if (plantname.empty())
    {
        plantname = "MyPlant";
    }

    if (this->timezone.empty())
    {
        cout << "Missing timezone.\n";
        rc = -2;
    }

    //force settings to prepare for live loading to http://pvoutput.org/loadlive.jsp
    if (this->loadlive == 1)
    {
        strncat(this->outputPath, "/LoadLive", sizeof(this->outputPath));
        strcpy(this->DateTimeFormat, "%H:%M");
        this->CSV_Export = 1;
        this->decimalpoint = '.';
        this->CSV_Header = 0;
        this->CSV_ExtendedHeader = 0;
        this->CSV_SaveZeroPower = 0;
        this->delimiter = ';';
//		this->PVoutput = 0;
        this->archEventMonths = 0;
        this->archMonths = 0;
        this->nospot = 1;
    }

    // If 1st day of the month and -am1 specified, force to -am2 to get last day of prev month
    if (this->archMonths == 1)
    {
        time_t now = time(NULL);
        struct tm *tm_now = localtime(&now);
        if (tm_now->tm_mday == 1)
            this->archMonths++;
    }

    if (this->verbose > 2) showConfig();

    return rc;
}

void Config::showConfig()
{
    std::cout << "Configuration settings:";
    if (strlen(this->IP_Address) == 0)	// No IP address -> Show BT address
        std::cout << "\nBTAddress=" << this->BT_Address;
    if (strlen(this->BT_Address) == 0)	// No BT address -> Show IP address
        std::cout << "\nIP_Address=" << this->IP_Address;
    std::cout << "\nPassword=<undisclosed>" << \
        "\nMIS_Enabled=" << this->MIS_Enabled << \
        "\nPlantname=" << this->plantname << \
        "\nOutputPath=" << this->outputPath << \
        "\nOutputPathEvents=" << this->outputPath_Events << \
        "\nLatitude=" << this->latitude << \
        "\nLongitude=" << this->longitude << \
        "\nTimezone=" << this->timezone << \
        "\nCalculateMissingSpotValues=" << this->calcMissingSpot << \
        "\nDateTimeFormat=" << this->DateTimeFormat << \
        "\nDateFormat=" << this->DateFormat << \
        "\nTimeFormat=" << this->TimeFormat << \
        "\nSynchTime=" << this->synchTime << \
        "\nSynchTimeLow=" << this->synchTimeLow << \
        "\nSynchTimeHigh=" << this->synchTimeHigh << \
        "\nSunRSOffset=" << this->SunRSOffset << \
        "\nDecimalPoint=" << dp2txt(this->decimalpoint) << \
        "\nCSV_Delimiter=" << delim2txt(this->delimiter) << \
        "\nPrecision=" << this->precision << \
        "\nCSV_Export=" << this->CSV_Export << \
        "\nCSV_ExtendedHeader=" << this->CSV_ExtendedHeader << \
        "\nCSV_Header=" << this->CSV_Header << \
        "\nCSV_SaveZeroPower=" << this->CSV_SaveZeroPower << \
        "\nCSV_Spot_TimeSource=" << this->SpotTimeSource << \
        "\nCSV_Spot_WebboxHeader=" << this->SpotWebboxHeader << \
        "\nLocale=" << this->locale << \
        "\nBTConnectRetries=" << this->BT_ConnectRetries << std::endl;

#if defined(USE_MYSQL) || defined(USE_SQLITE)
    std::cout << "SQL_Database=" << this->sqlDatabase << std::endl;
#endif

#if defined(USE_MYSQL)
    std::cout << "SQL_Hostname=" << this->sqlHostname << \
        "\nSQL_Username=" << this->sqlUsername << \
        "\nSQL_Password=<undisclosed>" << std::endl;
#endif

    if (this->mqtt == 1)
    {
        std::cout << "MQTT_Host=" << this->mqtt_host << \
            "\nMQTT_Port=" << this->mqtt_port << \
            "\nMQTT_Topic=" << this->mqtt_topic << \
            "\nMQTT_Publisher=" << this->mqtt_publish_exe << \
            "\nMQTT_PublisherArgs=" << this->mqtt_publish_args << \
            "\nMQTT_Data=" << this->mqtt_publish_data << \
            "\nMQTT_ItemFormat=" << this->mqtt_item_format << std::endl;
    }

    std::cout << "### End of Config ###" << std::endl;
}

void Config::sayHello(int ShowHelp)
{
#if __BYTE_ORDER == __BIG_ENDIAN
#define BYTEORDER "BE"
#else
#define BYTEORDER "LE"
#endif
    std::cout << "SBFspot V" << VERSION << "\n";
    std::cout << "Yet another tool to read power production of SMA solar inverters\n";
    std::cout << "(c) 2012-2021, SBF (https://github.com/SBFspot/SBFspot)\n";
    std::cout << "Compiled for " << OS << " (" << BYTEORDER << ") " << sizeof(long) * 8 << " bit";
#if defined(USE_SQLITE)
    std::cout << " with SQLite support" << std::endl;
#endif
#if defined(USE_MYSQL)
    std::cout << " with MySQL support" << std::endl;
#endif
    if (ShowHelp != 0)
    {
        std::cout << "SBFspot [-options]" << endl;
        std::cout << " -scan               Scan for bluetooth enabled SMA inverters.\n";
        std::cout << " -d#                 Set debug level: 0-5 (0=none, default=2)\n";
        std::cout << " -v#                 Set verbose output level: 0-5 (0=none, default=2)\n";
        std::cout << " -ad#                Set #days for archived daydata: 0-" << MAX_CFG_AD << "\n";
        std::cout << "                     0=disabled, 1=today (default), ...\n";
        std::cout << " -am#                Set #months for archived monthdata: 0-" << MAX_CFG_AM << "\n";
        std::cout << "                     0=disabled, 1=current month (default), ...\n";
        std::cout << " -ae#                Set #months for archived events: 0-" << MAX_CFG_AE << "\n";
        std::cout << "                     0=disabled, 1=current month (default), ...\n";
        std::cout << " -cfgX.Y             Set alternative config file to X.Y (multiple inverters)\n";
        std::cout << " -finq               Force Inquiry (Inquire inverter also during the night)\n";
        std::cout << " -q                  Quiet (No output)\n";
        std::cout << " -nocsv              Disables CSV export (Overrules CSV_Export in config)\n";
        std::cout << " -nosql              Disables SQL export\n";
        std::cout << " -sp0                Disables Spot.csv export\n";
        std::cout << " -installer          Login as installer\n";
        std::cout << " -password:xxxx      Installer password\n";
        std::cout << " -loadlive           Use predefined settings for manual upload to pvoutput.org\n";
        std::cout << " -startdate:YYYYMMDD Set start date for historic data retrieval\n";
        std::cout << " -settime            Sync inverter time with host time\n";
        std::cout << " -mqtt               Publish spot data to MQTT broker\n";
        //std::cout << " -ble                Publish spot data via Bluetooth LE\n" << std::endl;
        std::cout << " -loop               Run SBF spot in a loop (daemon mode)\n" << std::endl;

        std::cout << "Libraries used:\n";
#if defined(USE_SQLITE)
        std::cout << "\tSQLite V" << sqlite3_libversion() << std::endl;
#endif
#if defined(USE_MYSQL)
        unsigned long mysql_version = mysql_get_client_version();
        std::cout << "\tMySQL V"
            << mysql_version / 10000     << "."	// major
            << mysql_version / 100 % 100 << "." // minor
            << mysql_version % 100	     << " (Client)" // build
            << std::endl;
#endif
        std::cout << "\tBOOST V"
            << BOOST_VERSION / 100000     << "."  // major
            << BOOST_VERSION / 100 % 1000 << "."  // minor
            << BOOST_VERSION % 100                // build
            << std::endl;
    }
}

void Config::invalidArg(char *arg)
{
    std::cout << "Invalid argument: " << arg << "\nUse -? for help" << std::endl;
}
