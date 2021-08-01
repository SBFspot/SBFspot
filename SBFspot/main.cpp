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

#include "Inverter.h"
#include "sunrise_sunset.h"

const uint32_t MAX_INVERTERS = 20;

using namespace boost;

int main(int argc, char **argv)
{
#if defined(_WIN32)
	// On Windows, switch console to UTF-8
	SetConsoleOutputCP(CP_UTF8);
#endif

    int rc = 0;

    Config cfg;

    //Read the command line and store settings in config struct
    rc = parseCmdline(argc, argv, &cfg);
    if (rc == -1) return 1;	//Invalid commandline - Quit, error
    if (rc == 1) return 0;	//Nothing to do - Quit, no error

    //Read config file and store settings in config struct
    rc = GetConfig(&cfg);	//Config struct contains fullpath to config file
    if (rc != 0) return rc;

    //Copy some config settings to public variables
    debug = cfg.debug;
    verbose = cfg.verbose;
    quiet = cfg.quiet;
    ConnType = cfg.ConnectionType;

    if ((ConnType != CT_BLUETOOTH) && (cfg.settime == 1))
    {
        std::cout << "-settime is only supported for Bluetooth devices" << std::endl;
        return 0;
    }

    strncpy(DateTimeFormat, cfg.DateTimeFormat, sizeof(DateTimeFormat));
    strncpy(DateFormat, cfg.DateFormat, sizeof(DateFormat));

    if (VERBOSE_NORMAL) print_error(stdout, PROC_INFO, "Starting...\n");

    // If co-ordinates provided, calculate sunrise & sunset times
    // for this location
    if ((cfg.latitude != 0) || (cfg.longitude != 0))
    {
        cfg.isLight = sunrise_sunset(cfg.latitude, cfg.longitude, &cfg.sunrise, &cfg.sunset, (float)cfg.SunRSOffset / 3600);

        if (VERBOSE_NORMAL)
        {
            printf("sunrise: %02d:%02d\n", (int)cfg.sunrise, (int)((cfg.sunrise - (int)cfg.sunrise) * 60));
            printf("sunset : %02d:%02d\n", (int)cfg.sunset, (int)((cfg.sunset - (int)cfg.sunset) * 60));
        }

        if ((cfg.forceInq == 0) && (cfg.isLight == 0))
        {
            if (quiet == 0) puts("Nothing to do... it's dark. Use -finq to force inquiry.");
            return 0;
        }
    }

    int status = tagdefs.readall(cfg.AppPath, cfg.locale);
    if (status != TagDefs::READ_OK)
    {
        printf("Error reading tags\n");
        return(2);
    }

    Inverter inverter(cfg);
    rc = inverter.process();

    if (VERBOSE_NORMAL) print_error(stdout, PROC_INFO, "Done.\n");

    return rc;
}
