/************************************************************************************************
                               ____  ____  _____                _
                              / ___|| __ )|  ___|__ _ __   ___ | |_
                              \___ \|  _ \| |_ / __| '_ \ / _ \| __|
                               ___) | |_) |  _|\__ \ |_) | (_) | |_
                              |____/|____/|_|  |___/ .__/ \___/ \__|
                                                   |_|

	SBFspot - Yet another tool to read power production of SMA® solar/battery inverters
	(c)2012-2020, SBF

	Latest version can be found at https://github.com/SBFspot/SBFspot

	Special Thanks to:
	S. Pittaway: Author of "NANODE SMA PV MONITOR" on which this project is based.
	W. Simons  : Early adopter, main tester and SMAdata2® Protocol analyzer
	G. Schnuff : SMAdata2® Protocol analyzer
	T. Frank   : Speedwire® support
	Snowmiss   : User manual
	All other users for their contribution to the success of this project

	The Windows version of this project is developed using Visual C++ 2010 Express
		=> Use SBFspot.sln / SBFspot.vcxproj
	For Linux, project can be built using Code::Blocks or Make tool
		=> Use SBFspot.cbp for Code::Blocks
		=> Use makefile for make tool (converted from .cbp project file)

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

	SMA, Speedwire and SMAdata2 are registered trademarks of SMA Solar Technology AG

************************************************************************************************/
#include "version.h"
#include "osselect.h"
#include "endianness.h"
#include "SBFspot.h"
#include "misc.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <time.h>
#include "bluetooth.h"
#include "Ethernet.h"
#include "SBFNet.h"
#include "sunrise_sunset.h"
#include "CSVexport.h"
#include "EventData.h"
#include "ArchData.h"
#include "SQLselect.h"
#include <boost/algorithm/string.hpp>
#include <boost/asio/ip/address.hpp>
#include "mqtt.h"

using namespace std;
using namespace boost;
using namespace boost::date_time;
using namespace boost::posix_time;
using namespace boost::gregorian;

int MAX_CommBuf = 0;
int MAX_pcktBuf = 0;

const int MAX_INVERTERS = 20;

//Public vars
int debug = 0;
int verbose = 0;
int quiet = 0;
char DateTimeFormat[32];
char DateFormat[32];
CONNECTIONTYPE ConnType = CT_NONE;
TagDefs tagdefs = TagDefs();
bool hasBatteryDevice = false;	// Plant has 1 or more battery device(s)

int main(int argc, char **argv)
{
    char msg[80];

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

    //Allocate array to hold InverterData structs
    InverterData *Inverters[MAX_INVERTERS];
    for (int i=0; i<MAX_INVERTERS; Inverters[i++]=NULL);

    if (ConnType == CT_BLUETOOTH)
    {
        int attempts = 1;
        do
        {
            if (attempts != 1) sleep(1);
            {
                if (VERBOSE_NORMAL) printf("Connecting to %s (%d/%d)\n", cfg.BT_Address, attempts, cfg.BT_ConnectRetries);
                rc = bthConnect(cfg.BT_Address);
            }
            attempts++;
        }
        while ((attempts <= cfg.BT_ConnectRetries) && (rc != 0));


        if (rc != 0)
        {
            snprintf(msg, sizeof(msg), "bthConnect() returned %d\n", rc);
            print_error(stdout, PROC_CRITICAL, msg);
            return rc;
        }

		rc = initialiseSMAConnection(cfg.BT_Address, Inverters, cfg.MIS_Enabled);

        if (rc != E_OK)
        {
            print_error(stdout, PROC_CRITICAL, "Failed to initialize communication with inverter.\n");
            freemem(Inverters);
            bthClose();
            return rc;
        }

        rc = getBT_SignalStrength(Inverters[0]);
        if (VERBOSE_NORMAL) printf("BT Signal=%0.1f%%\n", Inverters[0]->BT_Signal);

    }
    else // CT_ETHERNET
    {
		if (VERBOSE_NORMAL) printf("Connecting to Local Network...\n");
		rc = ethConnect(cfg.IP_Port);
		if (rc != 0)
		{
			print_error(stdout, PROC_CRITICAL, "Failed to set up socket connection.");
			return rc;
		}

		if (cfg.ip_addresslist.size() > 1)
			// New method for multiple inverters with fixed IP
			rc = ethInitConnectionMulti(Inverters, cfg.ip_addresslist);
		else
			// Old method for one inverter (fixed IP or broadcast)
			rc = ethInitConnection(Inverters, cfg.IP_Address);

		if (rc != E_OK)
		{
			print_error(stdout, PROC_CRITICAL, "Failed to initialize Speedwire connection.");
			ethClose();
			return rc;
		}
    }

    if (logonSMAInverter(Inverters, cfg.userGroup, cfg.SMA_Password) != E_OK)
    {
        snprintf(msg, sizeof(msg), "Logon failed. Check '%s' Password\n", cfg.userGroup == UG_USER? "USER":"INSTALLER");
        print_error(stdout, PROC_CRITICAL, msg);
        freemem(Inverters);
        bthClose();
        return 1;
    }

    /*************************************************
     * At this point we are logged on to the inverter
     *************************************************/

    if (VERBOSE_NORMAL) puts("Logon OK");

	// If SBFspot is executed with -settime argument
	if (cfg.settime == 1)
	{
		rc = SetPlantTime(0, 0, 0);	// Set time ignoring limits
		logoffSMAInverter(Inverters[0]);

	    freemem(Inverters);
		bthClose();	// Close socket

		return rc;
	}

	// Synchronize plant time with system time
    // Only BT connected devices and if enabled in config _or_ requested by 123Solar
	// Most probably Speedwire devices get their time from the local IP network
    if ((ConnType == CT_BLUETOOTH) && (cfg.synchTime > 0 || cfg.s123 == S123_SYNC ))
		if ((rc = SetPlantTime(cfg.synchTime, cfg.synchTimeLow, cfg.synchTimeHigh)) != E_OK)
			std::cerr << "SetPlantTime returned an error: " << rc << std::endl;

	if ((rc = getInverterData(Inverters, sbftest)) != 0)
        std::cerr << "getInverterData(sbftest) returned an error: " << rc << std::endl;

	if ((rc = getInverterData(Inverters, SoftwareVersion)) != 0)
        std::cerr << "getSoftwareVersion returned an error: " << rc << std::endl;

    if ((rc = getInverterData(Inverters, TypeLabel)) != 0)
        std::cerr << "getTypeLabel returned an error: " << rc << std::endl;
    else
    {
        for (int inv=0; Inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
        {
			if ((Inverters[inv]->DevClass == BatteryInverter) || (Inverters[inv]->SUSyID == 292))	//SB 3600-SE (Smart Energy)
				hasBatteryDevice = Inverters[inv]->hasBattery = true;
			else
				Inverters[inv]->hasBattery = false;

            if (VERBOSE_NORMAL)
            {
                printf("SUSyID: %d - SN: %lu\n", Inverters[inv]->SUSyID, Inverters[inv]->Serial);
                printf("Device Name:      %s\n", Inverters[inv]->DeviceName);
                printf("Device Class:     %s%s\n", Inverters[inv]->DeviceClass, (Inverters[inv]->SUSyID == 292) ? " (with battery)":"");
                printf("Device Type:      %s\n", Inverters[inv]->DeviceType);
                printf("Software Version: %s\n", Inverters[inv]->SWVersion);
                printf("Serial number:    %lu\n", Inverters[inv]->Serial);
            }
        }
    }

	// Check for Multigate and get connected devices
    for (int inv=0; Inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
	{
		if ((Inverters[inv]->DevClass == CommunicationProduct) && (Inverters[inv]->SUSyID == SID_MULTIGATE))
		{
			if (VERBOSE_HIGH)
				std::cout << "Multigate found. Looking for connected devices..." << std::endl;

			// multigate has its own ID
			Inverters[inv]->multigateID = inv;

			if ((rc = getDeviceList(Inverters, inv)) != 0)
				std::cout << "getDeviceList returned an error: " << rc << std::endl;
			else
			{
				if (VERBOSE_HIGH)
				{
					std::cout << "Found these devices:" << std::endl;
					for (int ii=0; Inverters[ii]!=NULL && ii<MAX_INVERTERS; ii++)
					{
						std::cout << "ID:" << ii << " S/N:" << Inverters[ii]->SUSyID << "-" << Inverters[ii]->Serial << " IP:" << Inverters[ii]->IPAddress << std::endl;
					}
				}
			
				if (logonSMAInverter(Inverters, cfg.userGroup, cfg.SMA_Password) != E_OK)
				{
					snprintf(msg, sizeof(msg), "Logon failed. Check '%s' Password\n", cfg.userGroup == UG_USER? "USER":"INSTALLER");
					print_error(stdout, PROC_CRITICAL, msg);
					freemem(Inverters);
					ethClose();
					return 1;
				}

				if ((rc = getInverterData(Inverters, SoftwareVersion)) != 0)
					printf("getSoftwareVersion returned an error: %d\n", rc);

				if ((rc = getInverterData(Inverters, TypeLabel)) != 0)
					printf("getTypeLabel returned an error: %d\n", rc);
				else
				{
					for (int ii=0; Inverters[ii]!=NULL && ii<MAX_INVERTERS; ii++)
					{
						if (VERBOSE_NORMAL)
						{
							printf("SUSyID: %d - SN: %lu\n", Inverters[ii]->SUSyID, Inverters[ii]->Serial);
							printf("Device Name:      %s\n", Inverters[ii]->DeviceName);
							printf("Device Class:     %s\n", Inverters[ii]->DeviceClass);
							printf("Device Type:      %s\n", Inverters[ii]->DeviceType);
							printf("Software Version: %s\n", Inverters[ii]->SWVersion);
							printf("Serial number:    %lu\n", Inverters[ii]->Serial);
						}
					}
				}
			}
		}
	}

	if (hasBatteryDevice)
	{
		if ((rc = getInverterData(Inverters, BatteryChargeStatus)) != 0)
	        std::cerr << "getBatteryChargeStatus returned an error: " << rc << std::endl;
		else
		{
			for (int inv=0; Inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
			{
				if ((Inverters[inv]->DevClass == BatteryInverter) || (Inverters[inv]->hasBattery))
				{
					if (VERBOSE_NORMAL)
					{
						printf("SUSyID: %d - SN: %lu\n", Inverters[inv]->SUSyID, Inverters[inv]->Serial);
						printf("Batt. Charging Status: %lu%%\n", Inverters[inv]->BatChaStt);
					}
				}
			}
		}

		if ((rc = getInverterData(Inverters, BatteryInfo)) != 0)
	        std::cerr << "getBatteryInfo returned an error: " << rc << std::endl;
		else
		{
			for (int inv=0; Inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
			{
				if ((Inverters[inv]->DevClass == BatteryInverter) || (Inverters[inv]->hasBattery))
				{
					if (VERBOSE_NORMAL)
					{
						printf("SUSyID: %d - SN: %lu\n", Inverters[inv]->SUSyID, Inverters[inv]->Serial);
						printf("Batt. Temperature: %3.1f%sC\n", (float)(Inverters[inv]->BatTmpVal / 10), SYM_DEGREE); // degree symbol is different on windows/linux
						printf("Batt. Voltage    : %3.2fV\n", toVolt(Inverters[inv]->BatVol));
						printf("Batt. Current    : %2.3fA\n", toAmp(Inverters[inv]->BatAmp));
					}
				}
			}
		}

		if ((rc = getInverterData(Inverters, MeteringGridMsTotW)) != 0)
	        std::cerr << "getMeteringGridInfo returned an error: " << rc << std::endl;
		else
		{
			for (int inv=0; Inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
			{
				if ((Inverters[inv]->DevClass == BatteryInverter) || (Inverters[inv]->hasBattery))
				{
					if (VERBOSE_NORMAL)
					{
						printf("SUSyID: %d - SN: %lu\n", Inverters[inv]->SUSyID, Inverters[inv]->Serial);
						printf("Grid Power Out : %dW\n", Inverters[inv]->MeteringGridMsTotWOut);
						printf("Grid Power In  : %dW\n", Inverters[inv]->MeteringGridMsTotWIn);
					}
				}
			}
		}
	}

    if ((rc = getInverterData(Inverters, DeviceStatus)) != 0)
        std::cerr << "getDeviceStatus returned an error: " << rc << std::endl;
    else
    {
        for (int inv=0; Inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
        {
            if (VERBOSE_NORMAL)
            {
                printf("SUSyID: %d - SN: %lu\n", Inverters[inv]->SUSyID, Inverters[inv]->Serial);
				printf("Device Status:      %s\n", tagdefs.getDesc(Inverters[inv]->DeviceStatus, "?").c_str());
            }
        }
    }

	if ((rc = getInverterData(Inverters, InverterTemperature)) != 0)
        std::cerr << "getInverterTemperature returned an error: " << rc << std::endl;
    else
    {
        for (int inv=0; Inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
        {
            if (VERBOSE_NORMAL)
            {
                printf("SUSyID: %d - SN: %lu\n", Inverters[inv]->SUSyID, Inverters[inv]->Serial);
				printf("Device Temperature: %3.1f%sC\n", ((float)Inverters[inv]->Temperature / 100), SYM_DEGREE); // degree symbol is different on windows/linux
            }
        }
    }

	if (Inverters[0]->DevClass == SolarInverter)
    {
        if ((rc = getInverterData(Inverters, GridRelayStatus)) != 0)
	        std::cerr << "getGridRelayStatus returned an error: " << rc << std::endl;
        else
        {
            for (int inv=0; Inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
            {
                if (Inverters[inv]->DevClass == SolarInverter)
                {
                    if (VERBOSE_NORMAL)
                    {
                        printf("SUSyID: %d - SN: %lu\n", Inverters[inv]->SUSyID, Inverters[inv]->Serial);
						printf("GridRelay Status:      %s\n", tagdefs.getDesc(Inverters[inv]->GridRelayStatus, "?").c_str());
                    }
                }
            }
        }
    }

    if ((rc = getInverterData(Inverters, MaxACPower)) != 0)
        std::cerr << "getMaxACPower returned an error: " << rc << std::endl;
    else
    {
        //TODO: REVIEW THIS PART (getMaxACPower & getMaxACPower2 should be 1 function)
        if ((Inverters[0]->Pmax1 == 0) && (rc = getInverterData(Inverters, MaxACPower2)) != 0)
	        std::cerr << "getMaxACPower2 returned an error: " << rc << std::endl;
        else
        {
            for (int inv=0; Inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
            {
                if (VERBOSE_NORMAL)
                {
                    printf("SUSyID: %d - SN: %lu\n", Inverters[inv]->SUSyID, Inverters[inv]->Serial);
                    printf("Pac max phase 1: %luW\n", Inverters[inv]->Pmax1);
                    printf("Pac max phase 2: %luW\n", Inverters[inv]->Pmax2);
                    printf("Pac max phase 3: %luW\n", Inverters[inv]->Pmax3);
                }
            }
        }
    }

    if ((rc = getInverterData(Inverters, EnergyProduction)) != 0)
        std::cerr << "getEnergyProduction returned an error: " << rc << std::endl;

    if ((rc = getInverterData(Inverters, OperationTime)) != 0)
        std::cerr << "getOperationTime returned an error: " << rc << std::endl;
    else
    {
        for (int inv=0; Inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
        {
            if (VERBOSE_NORMAL)
            {
                printf("SUSyID: %d - SN: %lu\n", Inverters[inv]->SUSyID, Inverters[inv]->Serial);
                puts("Energy Production:");
                printf("\tEToday: %.3fkWh\n", tokWh(Inverters[inv]->EToday));
                printf("\tETotal: %.3fkWh\n", tokWh(Inverters[inv]->ETotal));
                printf("\tOperation Time: %.2fh\n", toHour(Inverters[inv]->OperationTime));
                printf("\tFeed-In Time  : %.2fh\n", toHour(Inverters[inv]->FeedInTime));
            }
        }
    }

    if ((rc = getInverterData(Inverters, SpotDCPower)) != 0)
        std::cerr << "getSpotDCPower returned an error: " << rc << std::endl;

    if ((rc = getInverterData(Inverters, SpotDCVoltage)) != 0)
        std::cerr << "getSpotDCVoltage returned an error: " << rc << std::endl;

    //Calculate missing DC Spot Values
    if (cfg.calcMissingSpot == 1)
        CalcMissingSpot(Inverters[0]);

    for (int inv=0; Inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
    {
		Inverters[inv]->calPdcTot = Inverters[inv]->Pdc1 + Inverters[inv]->Pdc2;
        if (VERBOSE_NORMAL)
        {
            printf("SUSyID: %d - SN: %lu\n", Inverters[inv]->SUSyID, Inverters[inv]->Serial);
            puts("DC Spot Data:");
            printf("\tString 1 Pdc: %7.3fkW - Udc: %6.2fV - Idc: %6.3fA\n", tokW(Inverters[inv]->Pdc1), toVolt(Inverters[inv]->Udc1), toAmp(Inverters[inv]->Idc1));
            printf("\tString 2 Pdc: %7.3fkW - Udc: %6.2fV - Idc: %6.3fA\n", tokW(Inverters[inv]->Pdc2), toVolt(Inverters[inv]->Udc2), toAmp(Inverters[inv]->Idc2));
            printf("\tCalculated Total Pdc: %7.3fkW\n", tokW(Inverters[inv]->calPdcTot));
        }
    }

    if ((rc = getInverterData(Inverters, SpotACPower)) != 0)
        std::cerr << "getSpotACPower returned an error: " << rc << std::endl;

    if ((rc = getInverterData(Inverters, SpotACVoltage)) != 0)
        std::cerr << "getSpotACVoltage returned an error: " << rc << std::endl;

    if ((rc = getInverterData(Inverters, SpotACTotalPower)) != 0)
        std::cerr << "getSpotACTotalPower returned an error: " << rc << std::endl;

    //Calculate missing AC Spot Values
    if (cfg.calcMissingSpot == 1)
        CalcMissingSpot(Inverters[0]);

    for (int inv=0; Inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
    {
        Inverters[inv]->calPacTot = Inverters[inv]->Pac1 + Inverters[inv]->Pac2 + Inverters[inv]->Pac3;
        //Calculated Inverter Efficiency
        Inverters[inv]->calEfficiency = Inverters[inv]->calPdcTot == 0 ? 0.0f : 100.0f * (float)Inverters[inv]->calPacTot / (float)Inverters[inv]->calPdcTot;
        if (VERBOSE_NORMAL)
        {
            printf("SUSyID: %d - SN: %lu\n", Inverters[inv]->SUSyID, Inverters[inv]->Serial);
            puts("AC Spot Data:");
            printf("\tPhase 1 Pac : %7.3fkW - Uac: %6.2fV - Iac: %6.3fA\n", tokW(Inverters[inv]->Pac1), toVolt(Inverters[inv]->Uac1), toAmp(Inverters[inv]->Iac1));
            printf("\tPhase 2 Pac : %7.3fkW - Uac: %6.2fV - Iac: %6.3fA\n", tokW(Inverters[inv]->Pac2), toVolt(Inverters[inv]->Uac2), toAmp(Inverters[inv]->Iac2));
            printf("\tPhase 3 Pac : %7.3fkW - Uac: %6.2fV - Iac: %6.3fA\n", tokW(Inverters[inv]->Pac3), toVolt(Inverters[inv]->Uac3), toAmp(Inverters[inv]->Iac3));
            printf("\tTotal Pac   : %7.3fkW - Calculated Pac: %7.3fkW\n", tokW(Inverters[inv]->TotalPac), tokW(Inverters[inv]->calPacTot));
            printf("\tEfficiency  : %7.2f%%\n", Inverters[inv]->calEfficiency);
        }
    }

    if ((rc = getInverterData(Inverters, SpotGridFrequency)) != 0)
        std::cerr << "getSpotGridFrequency returned an error: " << rc << std::endl;
    else
    {
        for (int inv=0; Inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
        {
            if (VERBOSE_NORMAL)
            {
                printf("SUSyID: %d - SN: %lu\n", Inverters[inv]->SUSyID, Inverters[inv]->Serial);
                printf("Grid Freq. : %.2fHz\n", toHz(Inverters[inv]->GridFreq));
            }
        }
    }

    if (Inverters[0]->DevClass == SolarInverter)
	{
		for (int inv=0; Inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
		{
			if (VERBOSE_NORMAL)
			{
				printf("SUSyID: %d - SN: %lu\n", Inverters[inv]->SUSyID, Inverters[inv]->Serial);
				if (Inverters[inv]->InverterDatetime > 0)
					printf("Current Inverter Time: %s\n", strftime_t(cfg.DateTimeFormat, Inverters[inv]->InverterDatetime));

				if (Inverters[inv]->WakeupTime > 0)
					printf("Inverter Wake-Up Time: %s\n", strftime_t(cfg.DateTimeFormat, Inverters[inv]->WakeupTime));

				if (Inverters[inv]->SleepTime > 0)
					printf("Inverter Sleep Time  : %s\n", strftime_t(cfg.DateTimeFormat, Inverters[inv]->SleepTime));
			}
		}
	}

	if (Inverters[0]->DevClass == SolarInverter)
	{
		if ((cfg.CSV_Export == 1) && (cfg.nospot == 0))
			ExportSpotDataToCSV(&cfg, Inverters);

		if (cfg.wsl == 1)
			ExportSpotDataToWSL(&cfg, Inverters);

		if (cfg.s123 == S123_DATA)
			ExportSpotDataTo123s(&cfg, Inverters);
		if (cfg.s123 == S123_INFO)
			ExportInformationDataTo123s(&cfg, Inverters);
		if (cfg.s123 == S123_STATE)
			ExportStateDataTo123s(&cfg, Inverters);
	}

	if (hasBatteryDevice && (cfg.CSV_Export == 1) && (cfg.nospot == 0))
		ExportBatteryDataToCSV(&cfg, Inverters);

	#if defined(USE_SQLITE) || defined(USE_MYSQL)
	db_SQL_Export db = db_SQL_Export();
	if (!cfg.nosql)
	{
		db.open(cfg.sqlHostname, cfg.sqlUsername, cfg.sqlUserPassword, cfg.sqlDatabase);
		if (db.isopen())
		{
			time_t spottime = time(NULL);
			db.type_label(Inverters);
			db.device_status(Inverters, spottime);
			db.spot_data(Inverters, spottime);
			if (hasBatteryDevice) 
				db.battery_data(Inverters, spottime);
		}
	}
	#endif

	/*******
	* MQTT *
	********/
	if (cfg.mqtt == 1) // MQTT enabled
	{
		rc = mqtt_publish(&cfg, Inverters);
		if (rc != 0)
		{
			std::cout << "Error " << rc << " while publishing to MQTT Broker" << std::endl;
		}
	}

	//SolarInverter -> Continue to get archive data
	unsigned int idx;

    /***************
    * Get Day Data *
    ****************/
    time_t arch_time = (0 == cfg.startdate) ? time(NULL) : cfg.startdate;

    for (int count=0; count<cfg.archDays; count++)
    {
        if ((rc = ArchiveDayData(Inverters, arch_time)) != E_OK)
        {
            if (rc != E_ARCHNODATA)
		        std::cerr << "ArchiveDayData returned an error: " << rc << std::endl;
        }
        else
        {
            if (VERBOSE_HIGH)
            {
                for (int inv=0; Inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
                {
                    printf("SUSyID: %d - SN: %lu\n", Inverters[inv]->SUSyID, Inverters[inv]->Serial);
                    for (idx=0; idx<sizeof(Inverters[inv]->dayData)/sizeof(DayData); idx++)
                        if (Inverters[inv]->dayData[idx].datetime > 0)
						{
                            printf("%s : %.3fkWh - %3.3fW\n", strftime_t(cfg.DateTimeFormat, Inverters[inv]->dayData[idx].datetime), (double)Inverters[inv]->dayData[idx].totalWh/1000, (double)Inverters[inv]->dayData[idx].watt);
						    fflush(stdout);
						}
                    puts("======");
                }
            }

            if (cfg.CSV_Export == 1)
                ExportDayDataToCSV(&cfg, Inverters);

			#if defined(USE_SQLITE) || defined(USE_MYSQL)
			if ((!cfg.nosql) && db.isopen())
				db.day_data(Inverters);
			#endif
        }

        //Goto previous day
        arch_time -= 86400;
    }


    /*****************
    * Get Month Data *
    ******************/
	if (cfg.archMonths > 0)
	{
		getMonthDataOffset(Inverters); //Issues 115/130
		arch_time = (0 == cfg.startdate) ? time(NULL) : cfg.startdate;
		struct tm arch_tm;
		memcpy(&arch_tm, gmtime(&arch_time), sizeof(arch_tm));

		for (int count=0; count<cfg.archMonths; count++)
		{
			ArchiveMonthData(Inverters, &arch_tm);

			if (VERBOSE_HIGH)
			{
				for (int inv = 0; Inverters[inv] != NULL && inv<MAX_INVERTERS; inv++)
				{
					printf("SUSyID: %d - SN: %lu\n", Inverters[inv]->SUSyID, Inverters[inv]->Serial);
					for (unsigned int ii = 0; ii < sizeof(Inverters[inv]->monthData) / sizeof(MonthData); ii++)
						if (Inverters[inv]->monthData[ii].datetime > 0)
							printf("%s : %.3fkWh - %3.3fkWh\n", strfgmtime_t(cfg.DateFormat, Inverters[inv]->monthData[ii].datetime), (double)Inverters[inv]->monthData[ii].totalWh / 1000, (double)Inverters[inv]->monthData[ii].dayWh / 1000);
					puts("======");
				}
			}

			if (cfg.CSV_Export == 1)
				ExportMonthDataToCSV(&cfg, Inverters);

			#if defined(USE_SQLITE) || defined(USE_MYSQL)
			if ((!cfg.nosql) && db.isopen())
				db.month_data(Inverters);
			#endif

			//Go to previous month
			if (--arch_tm.tm_mon < 0)
			{
				arch_tm.tm_mon = 11;
				arch_tm.tm_year--;
			}
		}
	}

    /*****************
    * Get Event Data *
    ******************/
	posix_time::ptime tm_utc(posix_time::from_time_t((0 == cfg.startdate) ? time(NULL) : cfg.startdate));
	//ptime tm_utc(posix_time::second_clock::universal_time());
	gregorian::date dt_utc(tm_utc.date().year(), tm_utc.date().month(), 1);
	std::string dt_range_csv = str(format("%d%02d") % dt_utc.year() % static_cast<short>(dt_utc.month()));

	for (int m = 0; m < cfg.archEventMonths; m++)
	{
		if (VERBOSE_LOW) cout << "Reading events: " << to_simple_string(dt_utc) << endl;
		//Get user level events
		rc = ArchiveEventData(Inverters, dt_utc, UG_USER);
		if (rc == E_EOF) break; // No more data (first event reached)
		else if (rc != E_OK) std::cerr << "ArchiveEventData(user) returned an error: " << rc << endl;

		//When logged in as installer, get installer level events
		if (cfg.userGroup == UG_INSTALLER)
		{
			rc = ArchiveEventData(Inverters, dt_utc, UG_INSTALLER);
			if (rc == E_EOF) break; // No more data (first event reached)
			else if (rc != E_OK) std::cerr << "ArchiveEventData(installer) returned an error: " << rc << endl;
		}

		//Move to previous month
		if (dt_utc.month() == 1)
			dt_utc = gregorian::date(dt_utc.year() - 1, 12, 1);
		else
			dt_utc = gregorian::date(dt_utc.year(), dt_utc.month() - 1, 1);

	}

	if (rc == E_OK)
	{
		//Adjust start of range with 1 month
		if (dt_utc.month() == 12)
			dt_utc = gregorian::date(dt_utc.year() + 1, 1, 1);
		else
			dt_utc = gregorian::date(dt_utc.year(), dt_utc.month() + 1, 1);
	}

	if ((rc == E_OK) || (rc == E_EOF))
	{
		dt_range_csv = str(format("%d%02d-%s") % dt_utc.year() % static_cast<short>(dt_utc.month()) % dt_range_csv);

		if ((cfg.CSV_Export == 1) && (cfg.archEventMonths > 0))
			ExportEventsToCSV(&cfg, Inverters, dt_range_csv);

	#if defined(USE_SQLITE) || defined(USE_MYSQL)
	if ((!cfg.nosql) && db.isopen())
		db.event_data(Inverters, tagdefs);
	#endif
	}

	if (cfg.ConnectionType == CT_BLUETOOTH)
		logoffSMAInverter(Inverters[0]);
	else
	{
		logoffMultigateDevices(Inverters);
		for (int inv=0; Inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
			logoffSMAInverter(Inverters[inv]);
	}

    freemem(Inverters);
    bthClose();

	#if defined(USE_SQLITE) || defined(USE_MYSQL)
	if ((!cfg.nosql) && db.isopen())
		db.close();
	#endif


    if (VERBOSE_NORMAL) print_error(stdout, PROC_INFO, "Done.\n");

    return 0;
}

//Free memory allocated by initialiseSMAConnection()
void freemem(InverterData *inverters[])
{
    for (int i=0; i<MAX_INVERTERS; i++)
        if (inverters[i] != NULL)
        {
            delete inverters[i];
            inverters[i] = NULL;
        }
}

E_SBFSPOT getPacket(unsigned char senderaddr[6], int wait4Command)
{
    if (DEBUG_NORMAL) printf("getPacket(%d)\n", wait4Command);
    int index = 0;
    int hasL2pckt  = 0;
    E_SBFSPOT rc = E_OK;
    pkHeader *pkHdr = (pkHeader *)CommBuf;
    do
    {
        int bib = bthRead(CommBuf, sizeof(pkHeader));
        if (bib <= 0)
        {
            if (DEBUG_NORMAL) printf("No data!\n");
            return E_NODATA;
        }

        //More data after header?
        if (btohs(pkHdr->pkLength) > sizeof(pkHeader))
        {
            bib += bthRead(CommBuf + sizeof(pkHeader), btohs(pkHdr->pkLength) - sizeof(pkHeader));

            if (DEBUG_HIGH) HexDump(CommBuf, bib, 10);

            //Check if data is coming from the right inverter
            if (isValidSender(senderaddr, pkHdr->SourceAddr) == 1)
            {
                rc = E_OK;
                if (DEBUG_NORMAL) printf("cmd=%d\n", btohs(pkHdr->command));

                if ((hasL2pckt == 0) && (CommBuf[18] == 0x7E) && (get_long(CommBuf+19) == 0x656003FF))
                {
                    hasL2pckt = 1;
                }

                if (hasL2pckt == 1)
                {
                    //Copy CommBuf to packetbuffer
                    int bufptr = sizeof(pkHeader);
                    bool escNext = false;

                    if (DEBUG_NORMAL) printf("PacketLength=%d\n", btohs(pkHdr->pkLength));

                    for (int i = sizeof(pkHeader); i < btohs(pkHdr->pkLength); i++)
                    {
                        pcktBuf[index] = CommBuf[bufptr++];
                        //Keep 1st byte raw unescaped 0x7E
                        if (escNext == true)
                        {
                            pcktBuf[index] ^= 0x20;
                            escNext = false;
                            index++;
                        }
                        else
                        {
                            if (pcktBuf[index] == 0x7D)
                                escNext = true; //Throw away the 0x7d byte
                            else
                                index++;
                        }
                        if (index >= maxpcktBufsize)
                        {
                            printf("Warning: pcktBuf buffer overflow! (%d)\n", index);
                            return E_BUFOVRFLW;
                        }
                    }
                    packetposition = index;
                }
                else
                {
                    memcpy(pcktBuf, CommBuf, bib);
                    packetposition = bib;
                }
            } // isValidSender()
            else
            {
                rc = E_RETRY;
                if (DEBUG_NORMAL)
                    printf("Wrong sender: %02X:%02X:%02X:%02X:%02X:%02X\n",
                           pkHdr->SourceAddr[5],
                           pkHdr->SourceAddr[4],
                           pkHdr->SourceAddr[3],
                           pkHdr->SourceAddr[2],
                           pkHdr->SourceAddr[1],
                           pkHdr->SourceAddr[0]);
            }
        }
        else
        {
            if (DEBUG_HIGH) HexDump(CommBuf, bib, 10);
            //Check if data is coming from the right inverter
            if (isValidSender(senderaddr, pkHdr->SourceAddr) == 1)
            {
                rc = E_OK;
                if (DEBUG_NORMAL) printf("cmd=%d\n", btohs(pkHdr->command));

                memcpy(pcktBuf, CommBuf, bib);
                packetposition = bib;
            } // isValidSender()
            else
            {
                rc = E_RETRY;
                if (DEBUG_NORMAL)
                    printf("Wrong sender: %02X:%02X:%02X:%02X:%02X:%02X\n",
                           pkHdr->SourceAddr[5],
                           pkHdr->SourceAddr[4],
                           pkHdr->SourceAddr[3],
                           pkHdr->SourceAddr[2],
                           pkHdr->SourceAddr[1],
                           pkHdr->SourceAddr[0]);
            }
        }
    }
    // changed to have "any" wait4Command (0xFF) - if you have different order of commands
    while (((btohs(pkHdr->command) != wait4Command) || (rc == E_RETRY)) && (0xFF != wait4Command));

    if ((rc == E_OK) && (DEBUG_HIGH))
    {
        printf("<<<====== Content of pcktBuf =======>>>\n");
        HexDump(pcktBuf, packetposition, 10);
        printf("<<<=================================>>>\n");
    }

    if (packetposition > MAX_pcktBuf)
    {
        MAX_pcktBuf = packetposition;
        if (DEBUG_HIGH)
        {
            printf("MAX_pcktBuf is now %d bytes\n", MAX_pcktBuf);
        }
    }

    return rc;
}

int getInverterIndexBySerial(InverterData *inverters[], unsigned short SUSyID, uint32_t Serial)
{
	if (DEBUG_HIGHEST)
	{
		printf("getInverterIndexBySerial()\n");
		printf("Looking up %d:%lu\n", SUSyID, (unsigned long)Serial);
	}

    for (int inv=0; inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
    {
		if (DEBUG_HIGHEST)
			printf("Inverter[%d] %d:%lu\n", inv, inverters[inv]->SUSyID, inverters[inv]->Serial);

		if ((inverters[inv]->SUSyID == SUSyID) && inverters[inv]->Serial == Serial)
            return inv;
    }

	if (DEBUG_HIGHEST)
		printf("Serial Not Found!\n");
	
	return -1;
}

int getInverterIndexBySerial(InverterData *inverters[], uint32_t Serial)
{
    for (int inv=0; inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
    {
        if (inverters[inv]->Serial == Serial)
            return inv;
    }

    return -1;
}

int getInverterIndexByAddress(InverterData *inverters[], unsigned char bt_addr[6])
{
    for (int inv=0; inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
    {
        int byt;
        for (byt=0; byt<6; byt++)
        {
            if (inverters[inv]->BTAddress[byt] != bt_addr[byt])
                break;
        }

        if (byt == 6)
            return inv;
    }

    return -1;	//No inverter found
}

E_SBFSPOT ethGetPacket(void)
{
    if (DEBUG_NORMAL) printf("ethGetPacket()\n");
    E_SBFSPOT rc = E_OK;

    ethPacketHeaderL1L2 *pkHdr = (ethPacketHeaderL1L2 *)CommBuf;

    do
    {
        int bib = ethRead(CommBuf, sizeof(CommBuf));

        if (bib <= 0)
        {
            if (DEBUG_NORMAL) printf("No data!\n");
            rc = E_NODATA;
        }
        else
        {
            unsigned short pkLen = (pkHdr->pcktHdrL1.hiPacketLen << 8) + pkHdr->pcktHdrL1.loPacketLen;

            //More data after header?
            if (pkLen > 0)
            {
	            if (DEBUG_HIGH) HexDump(CommBuf, bib, 10);
                if (btohl(pkHdr->pcktHdrL2.MagicNumber) == ETH_L2SIGNATURE)
                {
                    // Copy CommBuf to packetbuffer
                    // Dummy byte to align with BTH (7E)
                    pcktBuf[0]= 0;
                    // We need last 6 bytes of ethPacketHeader too
                    memcpy(pcktBuf+1, CommBuf + sizeof(ethPacketHeaderL1), bib - sizeof(ethPacketHeaderL1));
                    // Point packetposition at last byte in our buffer
					// This is different from BTH
                    packetposition = bib - sizeof(ethPacketHeaderL1);

                    if (DEBUG_HIGH)
                    {
                        printf("<<<====== Content of pcktBuf =======>>>\n");
                        HexDump(pcktBuf, packetposition, 10);
                        printf("<<<=================================>>>\n");
                    }
                    
                    rc = E_OK;
                }
                else
                {
                    if (DEBUG_NORMAL) printf("L2 header not found.\n");
                    rc = E_RETRY;
                }
            }
            else
                rc = E_NODATA;
        }
    } while (rc == E_RETRY);

    return rc;
}

E_SBFSPOT ethInitConnection(InverterData *inverters[], char *IP_Address)
{
    if (VERBOSE_NORMAL) puts("Initializing...");

    //Generate a Serial Number for application
    AppSUSyID = 125;
    srand(time(NULL));
    AppSerial = 900000000 + ((rand() << 16) + rand()) % 100000000;
	// Fix Issue 103: Eleminate confusion: apply name: session-id iso SN
    if (VERBOSE_NORMAL) printf("SUSyID: %d - SessionID: %lu (0x%08lX)\n", AppSUSyID, AppSerial, AppSerial);

    E_SBFSPOT rc = E_OK;

    if (strlen(IP_Address) < 8) // len less than 0.0.0.0 or len of no string ==> use broadcast to detect inverters
    {
	    // Start with UDP broadcast to check for SMA devices on the LAN
    	writeLong(pcktBuf, 0x00414D53);  //Start of SMA header
    	writeLong(pcktBuf, 0xA0020400);  //Unknown
    	writeLong(pcktBuf, 0xFFFFFFFF);  //Unknown
    	writeLong(pcktBuf, 0x20000000);  //Unknown
    	writeLong(pcktBuf, 0x00000000);  //Unknown

    	ethSend(pcktBuf, IP_Broadcast);

    	//SMA inverter announces it´s presence in response to the discovery request packet
    	int bytesRead = ethRead(CommBuf, sizeof(CommBuf));

		// if bytesRead < 0, a timeout has occurred
		// if bytesRead == 0, no data was received
		if (bytesRead <= 0)
		{
			cerr << "ERROR: No inverter responded to identification broadcast.\n";
			cerr <<	"Try to set IP_Address in SBFspot.cfg!\n";
			return E_INIT;
		}

    	if (DEBUG_NORMAL) HexDump(CommBuf, bytesRead, 10);
    }

    int devcount = 0;
    //for_each inverter found
    //{
        inverters[devcount] = new InverterData;
		resetInverterData(inverters[devcount]);
        // Store received IP address as readable text to InverterData struct
        // IP address is found at pos 38 in the buffer
        // Don't know yet for multiple inverter plants
		// Len bigger than 0.0.0.0 or len of no string ==> use IP_Adress from config
        if (strlen(IP_Address) > 7) // Fix CP181
        {
            sprintf(inverters[devcount]->IPAddress, "%s", IP_Address);
            if (quiet == 0) printf("Inverter IP address: %s from SBFspot.cfg\n", inverters[devcount]->IPAddress);
        }
		else	// use IP from broadcast-detection of inverter
        {
        	sprintf(inverters[devcount]->IPAddress, "%d.%d.%d.%d", CommBuf[38], CommBuf[39], CommBuf[40], CommBuf[41]);
            if (quiet == 0) printf("Inverter IP address: %s found via broadcastidentification\n", inverters[devcount]->IPAddress);
        }
        devcount++;
    //}


    writePacketHeader(pcktBuf, 0, NULL);
    writePacket(pcktBuf, 0x09, 0xA0, 0, anySUSyID, anySerial);
    writeLong(pcktBuf, 0x00000200);
    writeLong(pcktBuf, 0);
    writeLong(pcktBuf, 0);
    writeLong(pcktBuf, 0);
    writePacketLength(pcktBuf);

    //Send packet to first inverter
    ethSend(pcktBuf, inverters[0]->IPAddress);

    if ((rc = ethGetPacket()) == E_OK)
    {
        ethPacket *pckt = (ethPacket *)pcktBuf;
        inverters[0]->SUSyID = btohs(pckt->Source.SUSyID);	// Fix Issue 98
        inverters[0]->Serial = btohl(pckt->Source.Serial);	// Fix Issue 98
    }
	else
	{
		cerr << "ERROR: Connection to inverter failed!\n";
		cerr << "Is " << inverters[0]->IPAddress << " the correct IP?\n";
		cerr << "Please check IP_Address in SBFspot.cfg!\n";
		return E_INIT;
	}

	logoffSMAInverter(inverters[0]);

    return rc;
}

// Initialise multiple ethernet connected inverters
E_SBFSPOT ethInitConnectionMulti(InverterData *inverters[], std::vector<std::string> IPaddresslist)
{
    if (VERBOSE_NORMAL) puts("Initializing...");

    //Generate a Serial Number for application
    AppSUSyID = 125;
    srand(time(NULL));
    AppSerial = 900000000 + ((rand() << 16) + rand()) % 100000000;

	if (VERBOSE_NORMAL) printf("SUSyID: %d - SessionID: %lu\n", AppSUSyID, AppSerial);

    E_SBFSPOT rc = E_OK;

    for (unsigned int devcount = 0; devcount < IPaddresslist.size(); devcount++)
	{
		inverters[devcount] = new InverterData;
		resetInverterData(inverters[devcount]);
		strcpy(inverters[devcount]->IPAddress, IPaddresslist[devcount].c_str());
        if (quiet == 0) printf("Inverter IP address: %s from SBFspot.cfg\n", inverters[devcount]->IPAddress);

		writePacketHeader(pcktBuf, 0, NULL);
		writePacket(pcktBuf, 0x09, 0xA0, 0, anySUSyID, anySerial);
		writeLong(pcktBuf, 0x00000200);
		writeLong(pcktBuf, 0);
		writeLong(pcktBuf, 0);
		writeLong(pcktBuf, 0);
		writePacketLength(pcktBuf);

		ethSend(pcktBuf, inverters[devcount]->IPAddress);

		if ((rc = ethGetPacket()) == E_OK)
		{
			ethPacket *pckt = (ethPacket *)pcktBuf;
			inverters[devcount]->SUSyID = btohs(pckt->Source.SUSyID);
			inverters[devcount]->Serial = btohl(pckt->Source.Serial);
			if (VERBOSE_NORMAL) printf("Inverter replied: %s SUSyID: %d - Serial: %lu\n", inverters[devcount]->IPAddress, inverters[devcount]->SUSyID, inverters[devcount]->Serial);
		}
		else
		{
			std::cerr << "ERROR: Connection to inverter failed!" << std::endl;
			std::cerr << "Is " << inverters[devcount]->IPAddress << " the correct IP?" << std::endl;
			std::cerr << "Please check IP_Address in SBFspot.cfg!" << std::endl;
			return E_INIT;
		}

		logoffSMAInverter(inverters[devcount]);
	}

    return rc;
}

E_SBFSPOT initialiseSMAConnection(const char *BTAddress, InverterData *inverters[], int MIS)
{
    if (VERBOSE_NORMAL) puts("Initializing...");

    //Generate a Serial Number for application
    AppSUSyID = 125;
    srand(time(NULL));
    AppSerial = 900000000 + ((rand() << 16) + rand()) % 100000000;
	// Fix Issue 103: Eleminate confusion: apply name: session-id iso SN
    if (VERBOSE_NORMAL) printf("SUSyID: %d - SessionID: %lu (0x%08lX)\n", AppSUSyID, AppSerial, AppSerial);

    //Convert BT_Address '00:00:00:00:00:00' to BTAddress[6]
    //scanf reads %02X as int, but we need unsigned char
    unsigned int tmp[6];
    sscanf(BTAddress, "%02X:%02X:%02X:%02X:%02X:%02X", &tmp[5], &tmp[4], &tmp[3], &tmp[2], &tmp[1], &tmp[0]);

	// Multiple Inverter Support disabled
	// Connect to 1 and only 1 device (V2.0.6 compatibility mode)
	if (MIS == 0)
	{
		// Allocate memory for inverter data struct
		inverters[0] = new InverterData;
		resetInverterData(inverters[0]);

		// Copy previously converted BT address
		for (int i=0; i<6; i++)
			inverters[0]->BTAddress[i] = (unsigned char)tmp[i];

		// Call 2.0.6 init function
		return initialiseSMAConnection(inverters[0]);
	}

    for (int i=0; i<6; i++)
        RootDeviceAddress[i] = (unsigned char)tmp[i];

    //Init Inverter
    unsigned char version[6] = {1,0,0,0,0,0};
    writePacketHeader(pcktBuf, 0x0201, version);
    writeByte(pcktBuf, 'v');
    writeByte(pcktBuf, 'e');
    writeByte(pcktBuf, 'r');
    writeByte(pcktBuf, 13);	//CR
    writeByte(pcktBuf, 10);	//LF
    writePacketLength(pcktBuf);
    bthSend(pcktBuf);

    // This can take up to 3 seconds!
    if (getPacket(RootDeviceAddress, 0x02) != E_OK)
        return E_INIT;

    //Search devices
    unsigned char NetID = pcktBuf[22];
    if (VERBOSE_NORMAL) printf("SMA netID=%02X\n", NetID);

    writePacketHeader(pcktBuf, 0x02, RootDeviceAddress);
    writeLong(pcktBuf, 0x00700400);
    writeByte(pcktBuf, NetID);
    writeLong(pcktBuf, 0);
    writeLong(pcktBuf, 1);
    writePacketLength(pcktBuf);
    bthSend(pcktBuf);

    //Connection to Root Device
    if (getPacket(RootDeviceAddress, 0x0A) != E_OK)
        return E_INIT;

    //If Root Device has changed, copy the new address
    if (pcktBuf[24] == 2)
    {
        for (int i=0; i<6; i++)
            RootDeviceAddress[i] = pcktBuf[18+i];
        //Get local BT address
        for (int i=0; i<6; i++)
            LocalBTAddress[i] = pcktBuf[25+i];
    }

    if (DEBUG_NORMAL)
        printf("Root device address: %02X:%02X:%02X:%02X:%02X:%02X\n",
               RootDeviceAddress[5], RootDeviceAddress[4], RootDeviceAddress[3],
               RootDeviceAddress[2], RootDeviceAddress[1], RootDeviceAddress[0]);

    //Get local BT address
    for (int i=0; i<6; i++)
        LocalBTAddress[i] = pcktBuf[25+i];

    if (DEBUG_NORMAL)
        printf("Local BT address: %02X:%02X:%02X:%02X:%02X:%02X\n",
               LocalBTAddress[5], LocalBTAddress[4], LocalBTAddress[3],
               LocalBTAddress[2], LocalBTAddress[1], LocalBTAddress[0]);

    if (getPacket(RootDeviceAddress, 0x05) != E_OK)
        return E_INIT;

    //Get network topology
    int pcktsize = get_short(pcktBuf+1);
    int devcount = 0;
    for (int ptr=18; ptr <= pcktsize-8; ptr+=8)
    {
        if (DEBUG_NORMAL)
            printf("Device %d: %02X:%02X:%02X:%02X:%02X:%02X -> ", devcount,
                   pcktBuf[ptr+5], pcktBuf[ptr+4], pcktBuf[ptr+3], pcktBuf[ptr+2], pcktBuf[ptr+1], pcktBuf[ptr+0]);

        if (get_short(pcktBuf+ptr+6) == 0x0101) // Inverters only - Ignore other devices
        {
            if (DEBUG_NORMAL) printf("Inverter\n");
            if (devcount < MAX_INVERTERS)
            {
                inverters[devcount] = new InverterData;
				resetInverterData(inverters[devcount]);

                for (int i=0; i<6; i++)
                    inverters[devcount]->BTAddress[i] = pcktBuf[ptr+i];

                inverters[devcount]->NetID = NetID;
                devcount++;
            }
            else if (DEBUG_HIGHEST) printf("MAX_INVERTERS limit (%d) reached.\n", MAX_INVERTERS);

        }
        else if (DEBUG_NORMAL) printf(memcmp((unsigned char *)pcktBuf+ptr, LocalBTAddress, sizeof(LocalBTAddress)) == 0 ? "Local BT Address\n" : "Another device?\n");
    }

    /***********************************************************************
    	This part is only needed if you have more than one inverter
    	The purpose is to (re)build the network when we have found only 1
    ************************************************************************/
    if(/*(MIS == 1) && */(devcount == 1) && (NetID > 1))
    {
        // We need more handshake 03/04 commands to initialise network connection between inverters
        writePacketHeader(pcktBuf, 0x03, RootDeviceAddress);
        writeShort(pcktBuf, 0x000A);
        writeByte(pcktBuf, 0xAC);

        writePacketLength(pcktBuf);
        bthSend(pcktBuf);

        if (getPacket(RootDeviceAddress, 0x04) != E_OK)
            return E_INIT;

        writePacketHeader(pcktBuf, 0x03, RootDeviceAddress);
        writeShort(pcktBuf, 0x0002);

        writePacketLength(pcktBuf);
        bthSend(pcktBuf);

        if (getPacket(RootDeviceAddress, 0x04) != E_OK)
            return E_INIT;

        writePacketHeader(pcktBuf, 0x03, RootDeviceAddress);
        writeShort(pcktBuf, 0x0001);
        writeByte(pcktBuf, 0x01);

        writePacketLength(pcktBuf);
        bthSend(pcktBuf);

        if (getPacket(RootDeviceAddress, 0x04) != E_OK)
            return E_INIT;

        /******************************************************************
        	Read the network topology
        	Waiting for a max of 60 sec - 6 times 'timeout' of recv()
        	Should be enough for small networks (2-3 inverters)
        *******************************************************************/

        if (VERBOSE_NORMAL)
            puts ("Waiting for network to be built...");

        E_SBFSPOT rc = E_OK;
        unsigned short PacketType = 0;

        for (int i=0; i<6; i++)
        {
            // Get any packet - should be 0x0005 or 0x1001, but 0x0006 is allowed
            rc = getPacket(RootDeviceAddress, 0xFF);
            if (rc == E_OK)
            {
                PacketType = get_short(pcktBuf + 16);
                break;
            }
        }

        if (rc != E_OK)
		{
			if ((VERBOSE_NORMAL) && (NetID > 1))
		        puts ("In case of single inverter system set MIS_Enabled=0 in config file.");
            return E_INIT;	// Something went wrong... Failed to initialize
		}

        if (0x1001 == PacketType)
        {
            PacketType = 0;	//reset it
            rc = getPacket(RootDeviceAddress, 0x05);
            if (rc == E_OK)
                PacketType = get_short(pcktBuf + 16);
        }

        if (DEBUG_HIGHEST)
            printf("PacketType (0x%04X)\n", PacketType);

        if (0x0005 == PacketType)
        {
            /*
            Get network topology
            Overwrite all found inverters starting at index 1
            */
            pcktsize = get_short(pcktBuf+1);
            devcount = 1;

            for (int ptr=18; ptr<=pcktsize-8; ptr+=8)
            {
                if (DEBUG_NORMAL)
                    printf("Device %d: %02X:%02X:%02X:%02X:%02X:%02X -> ", devcount,
                           pcktBuf[ptr+5], pcktBuf[ptr+4], pcktBuf[ptr+3], pcktBuf[ptr+2], pcktBuf[ptr+1], pcktBuf[ptr+0]);

                if (get_short(pcktBuf+ptr+6) == 0x0101) // Inverters only - Ignore other devices
                {
                    if (DEBUG_NORMAL) printf("Inverter\n");
                    if (devcount < MAX_INVERTERS)
                    {
                        if (!inverters[devcount]) // If not yet allocated, do it now
						{
                            inverters[devcount] = new InverterData;
							resetInverterData(inverters[devcount]);
						}

                        for (int i=0; i<6; i++)
                            inverters[devcount]->BTAddress[i] = pcktBuf[ptr+i];

                        inverters[devcount]->NetID = NetID;
                        devcount++;
                    }
                    else if (DEBUG_HIGHEST) printf("MAX_INVERTERS limit (%d) reached.\n", MAX_INVERTERS);

                }
                else if (DEBUG_NORMAL) printf(memcmp((unsigned char *)pcktBuf+ptr, LocalBTAddress, sizeof(LocalBTAddress)) == 0 ? "Local BT Address\n" : "Another device?\n");
            }
        }

        /*
        At this point our netwerk should be ready!
        In some cases 0x0005 and 0x1001 are missing and we have already received 0x0006 (NETWORK IS READY")
        If not, just wait for it and ignore any error
        */
        if (0x06 != PacketType)
            getPacket(RootDeviceAddress, 0x06);
    }

    //Send broadcast request for identification
    do
    {
        pcktID++;
        writePacketHeader(pcktBuf, 0x01, addr_unknown);
        writePacket(pcktBuf, 0x09, 0xA0, 0, anySUSyID, anySerial);
        writeLong(pcktBuf, 0x00000200);
        writeLong(pcktBuf, 0);
        writeLong(pcktBuf, 0);
        writePacketTrailer(pcktBuf);
        writePacketLength(pcktBuf);
    }
    while (!isCrcValid(pcktBuf[packetposition-3], pcktBuf[packetposition-2]));

    bthSend(pcktBuf);

    //All inverters *should* reply with their SUSyID & SerialNr (and some other unknown info)
    for (int idx=0; inverters[idx]!=NULL && idx<MAX_INVERTERS; idx++)
    {
        if (getPacket(addr_unknown, 0x01) != E_OK)
            return E_INIT;

        if (!validateChecksum())
            return E_CHKSUM;

        int invindex = getInverterIndexByAddress(inverters, CommBuf + 4);

        if (invindex >= 0)
        {
            inverters[invindex]->SUSyID = get_short(pcktBuf + 55);
            inverters[invindex]->Serial = get_long(pcktBuf + 57);
            if (VERBOSE_NORMAL) printf("SUSyID: %d - SN: %lu\n", inverters[invindex]->SUSyID, inverters[invindex]->Serial);
        }
        else if (DEBUG_NORMAL)
            printf("Unexpected response from %02X:%02X:%02X:%02X:%02X:%02X -> ", CommBuf[9], CommBuf[8], CommBuf[7], CommBuf[6], CommBuf[5], CommBuf[4]);

    }

    logoffSMAInverter(inverters[0]);

    return E_OK;
}

// Init function used in SBFspot 2.0.6
// Called when MIS_Enabled=0
E_SBFSPOT initialiseSMAConnection(InverterData *invData)
{
	//Wait for announcement/broadcast message from PV inverter
	if (getPacket(invData->BTAddress, 2) != E_OK)
		return E_INIT;

	invData->NetID = pcktBuf[22];
	if (VERBOSE_NORMAL) printf("SMA netID=%02X\n", invData->NetID);

    writePacketHeader(pcktBuf, 0x02, invData->BTAddress);
	writeLong(pcktBuf, 0x00700400);
    writeByte(pcktBuf, invData->NetID);
    writeLong(pcktBuf, 0);
	writeLong(pcktBuf, 1);
    writePacketLength(pcktBuf);
    bthSend(pcktBuf);

	if (getPacket(invData->BTAddress, 5) != E_OK)
		return E_INIT;

	//Get local BT address - Added V3.1.5 (SetPlantTime)
    for (int i=0; i<6; i++)
        LocalBTAddress[i] = pcktBuf[26+i];

    if (DEBUG_NORMAL)
    {
        printf("Local BT address: %02X:%02X:%02X:%02X:%02X:%02X\n",
               LocalBTAddress[5], LocalBTAddress[4], LocalBTAddress[3],
               LocalBTAddress[2], LocalBTAddress[1], LocalBTAddress[0]);
    }

	do
    {
	    pcktID++;
        writePacketHeader(pcktBuf, 0x01, addr_unknown);
		writePacket(pcktBuf, 0x09, 0xA0, 0, anySUSyID, anySerial);
		writeLong(pcktBuf, 0x00000200);
        writeLong(pcktBuf, 0);
        writeLong(pcktBuf, 0);
        writePacketTrailer(pcktBuf);
        writePacketLength(pcktBuf);
	} while (!isCrcValid(pcktBuf[packetposition-3], pcktBuf[packetposition-2]));

	bthSend(pcktBuf);

	if (getPacket(invData->BTAddress, 1) != E_OK)
		return E_INIT;

	if (!validateChecksum())
		return E_CHKSUM;

	invData->Serial = get_long(pcktBuf + 57);
	if (VERBOSE_NORMAL) printf("Serial Nr: %08lX (%lu)\n", invData->Serial, invData->Serial);

	logoffSMAInverter(invData);

    return E_OK;
}

E_SBFSPOT logonSMAInverter(InverterData *inverters[], long userGroup, char *password)
{
#define MAX_PWLENGTH 12
    unsigned char pw[MAX_PWLENGTH] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    if (DEBUG_NORMAL) puts("logonSMAInverter()");

    char encChar = (userGroup == UG_USER)? 0x88:0xBB;
    //Encode password
    unsigned int idx;
    for (idx = 0; (password[idx] != 0) && (idx < sizeof(pw)); idx++)
        pw[idx] = password[idx] + encChar;
    for (; idx < MAX_PWLENGTH; idx++)
        pw[idx] = encChar;

    E_SBFSPOT rc = E_OK;
    int validPcktID = 0;

    time_t now;

    if (ConnType == CT_BLUETOOTH)
    {
	    do
		{
			pcktID++;
			now = time(NULL);
			writePacketHeader(pcktBuf, 0x01, addr_unknown);
			writePacket(pcktBuf, 0x0E, 0xA0, 0x0100, anySUSyID, anySerial);
			writeLong(pcktBuf, 0xFFFD040C);
			writeLong(pcktBuf, userGroup);	// User / Installer
			writeLong(pcktBuf, 0x00000384); // Timeout = 900sec ?
			writeLong(pcktBuf, now);
			writeLong(pcktBuf, 0);
			writeArray(pcktBuf, pw, sizeof(pw));
			writePacketTrailer(pcktBuf);
			writePacketLength(pcktBuf);
		}
		while (!isCrcValid(pcktBuf[packetposition-3], pcktBuf[packetposition-2]));

		bthSend(pcktBuf);

        do	//while (validPcktID == 0);
        {
            // In a multi inverter plant we get a reply from all inverters
            for (int i=0; inverters[i]!=NULL && i<MAX_INVERTERS; i++)
            {
                if ((rc  = getPacket(addr_unknown, 1)) != E_OK)
                    return rc;

                if (!validateChecksum())
                    return E_CHKSUM;
                else
                {
	                unsigned short rcvpcktID = get_short(pcktBuf+27) & 0x7FFF;
                    if ((pcktID == rcvpcktID) && (get_long(pcktBuf + 41) == now))
                    {
                        int ii = getInverterIndexByAddress(inverters, CommBuf + 4);
                        if (ii >= 0 )
                        {
                            inverters[ii]->SUSyID = get_short(pcktBuf + 15);
                            inverters[ii]->Serial = get_long(pcktBuf + 17);
                            validPcktID = 1;
							unsigned short retcode = get_short(pcktBuf + 23);
							switch (retcode)
							{
								case 0: rc = E_OK; break;
								case 0x0100: rc = E_INVPASSW; break;
								default: rc = E_LOGONFAILED; break;
							}
                        }
                        else if (DEBUG_NORMAL) printf("Unexpected response from %02X:%02X:%02X:%02X:%02X:%02X -> ", CommBuf[9], CommBuf[8], CommBuf[7], CommBuf[6], CommBuf[5], CommBuf[4]);
                    }
                    else
                    {
                        if (DEBUG_HIGHEST) puts("Unexpected response from inverter. Let's retry...");
                    }
                }
            }
        }
        while (validPcktID == 0);
    }
    else    // CT_ETHERNET
    {
		for (int inv=0; inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
		{
			do
			{
				pcktID++;
				now = time(NULL);
				writePacketHeader(pcktBuf, 0x01, addr_unknown);
				if (inverters[inv]->SUSyID != SID_SB240)
					writePacket(pcktBuf, 0x0E, 0xA0, 0x0100, inverters[inv]->SUSyID, inverters[inv]->Serial);
				else
					writePacket(pcktBuf, 0x0E, 0xE0, 0x0100, inverters[inv]->SUSyID, inverters[inv]->Serial);

				writeLong(pcktBuf, 0xFFFD040C);
				writeLong(pcktBuf, userGroup);	// User / Installer
				writeLong(pcktBuf, 0x00000384); // Timeout = 900sec ?
				writeLong(pcktBuf, now);
				writeLong(pcktBuf, 0);
				writeArray(pcktBuf, pw, sizeof(pw));
				writePacketTrailer(pcktBuf);
				writePacketLength(pcktBuf);
			}
			while (!isCrcValid(pcktBuf[packetposition-3], pcktBuf[packetposition-2]));

			ethSend(pcktBuf, inverters[inv]->IPAddress);

			validPcktID = 0;
			do
			{
				if ((rc = ethGetPacket()) == E_OK)
				{
					ethPacket *pckt = (ethPacket *)pcktBuf;
					if (pcktID == (btohs(pckt->PacketID) & 0x7FFF))   // Valid Packet ID
					{
						validPcktID = 1;
						unsigned short retcode = btohs(pckt->ErrorCode);
						switch (retcode)
						{
							case 0: rc = E_OK; break;
							case 0x0100: rc = E_INVPASSW; break;
							default: rc = E_LOGONFAILED; break;
						}
 					}
					else
						if (DEBUG_HIGHEST) printf("Packet ID mismatch. Expected %d, received %d\n", pcktID, (btohs(pckt->PacketID) & 0x7FFF));
				}
			} while ((validPcktID == 0) && (rc == E_OK)); // Fix Issue 167
		}
	}

    return rc;
}

E_SBFSPOT logoffSMAInverter(InverterData *inverter)
{
    if (DEBUG_NORMAL) puts("logoffSMAInverter()");
    do
    {
        pcktID++;
        writePacketHeader(pcktBuf, 0x01, addr_unknown);
        writePacket(pcktBuf, 0x08, 0xA0, 0x0300, anySUSyID, anySerial);
        writeLong(pcktBuf, 0xFFFD010E);
        writeLong(pcktBuf, 0xFFFFFFFF);
        writePacketTrailer(pcktBuf);
        writePacketLength(pcktBuf);
    }
    while (!isCrcValid(pcktBuf[packetposition-3], pcktBuf[packetposition-2]));

    if(ConnType == CT_BLUETOOTH)
        bthSend(pcktBuf);
    else
        ethSend(pcktBuf, inverter->IPAddress);

    return E_OK;
}

E_SBFSPOT SetPlantTime(time_t ndays, time_t lowerlimit, time_t upperlimit)
{
	// If not a Bluetooth connection, just quit
	if (ConnType != CT_BLUETOOTH)
		return E_OK;

    if (DEBUG_NORMAL)
		std::cout <<"SetPlantTime()" << std::endl;

    do
    {
        pcktID++;
        writePacketHeader(pcktBuf, 0x01, addr_unknown);
        writePacket(pcktBuf, 0x10, 0xA0, 0, anySUSyID, anySerial);
        writeLong(pcktBuf, 0xF000020A);
        writeLong(pcktBuf, 0x00236D00);
        writeLong(pcktBuf, 0x00236D00);
        writeLong(pcktBuf, 0x00236D00);
        writeLong(pcktBuf, 0);
        writeLong(pcktBuf, 0);
        writeLong(pcktBuf, 0);
        writeLong(pcktBuf, 0);
        writeLong(pcktBuf, 1);
        writeLong(pcktBuf, 1);
        writePacketTrailer(pcktBuf);
        writePacketLength(pcktBuf);
    }
    while (!isCrcValid(pcktBuf[packetposition-3], pcktBuf[packetposition-2]));

    bthSend(pcktBuf);

	time_t hosttime = time(NULL);

	// Inverter returns UTC time, TZ offset and DST
	// Packet ID is mismatched
	E_SBFSPOT rc = getPacket(addr_unknown, 1);

	if ((rc == E_OK) && (packetposition == 72))
	{
		if (get_long(pcktBuf + 41) != 0x00236D00)
		{
		    if (DEBUG_NORMAL) std::cout << "Unexpected packet received!" << std::endl;
			return E_COMM;
		}
		time_t invCurrTime = get_long(pcktBuf + 45);
		time_t invLastTimeSet = get_long(pcktBuf + 49);
		int tz = get_long(pcktBuf + 57) & 0xFFFFFFFE;
		int dst = get_long(pcktBuf + 57) & 0x00000001;
		int magic = get_long(pcktBuf + 61); // What's this?

		time_t timediff = invCurrTime - hosttime;

		if (VERBOSE_NORMAL)
		{
			std::cout << "Local Host Time: " << strftime_t(DateTimeFormat, hosttime) << std::endl;
			std::cout << "Plant Time     : " << strftime_t(DateTimeFormat, invCurrTime) << " (" << (timediff>0 ? "+":"") << timediff << " sec)" << std::endl;
			std::cout << "TZ offset      : " << tz << " sec - DST: " << (dst == 1? "On":"Off") << std::endl;
			std::cout << "Last Time Set  : " << strftime_t(DateTimeFormat, invLastTimeSet) << std::endl;
		}

		// Time difference between plant and host should be in the range of 1-3600 seconds
		// This is to avoid the plant time is wrongly set when host time is not OK
		// This check can be overruled by setting lower and upper limits = 0 (SBFspot -settime)
		timediff = abs(timediff);

		if ((lowerlimit == 0) && (upperlimit == 0)) // Ignore limits? (-settime command line argument)
		{
			// Plant time is OK - nothing to do
			if (timediff == 0) return E_OK;
		}
		else
		{
			// Time difference is too big - nothing to do
			if (timediff > upperlimit)
			{
				if (VERBOSE_NORMAL)
				{
					std::cout << "The time difference between inverter/host is more than " << upperlimit << " seconds.\n";
					std::cout << "As a precaution the plant time won't be adjusted.\n";
					std::cout << "To overrule this behaviour, execute SBFspot -settime" << std::endl;
				}

				return E_OK;
			}

			// Time difference is too small - nothing to do
			if (timediff < lowerlimit) return E_OK;

			// Calculate #days of last time set
			time_t daysago = ((hosttime - hosttime % 86400) - (invLastTimeSet - invLastTimeSet % 86400)) / 86400;

			if (daysago < ndays)
			{
				if (VERBOSE_NORMAL)
				{
					std::cout << "Time was already adjusted ";
					if (ndays == 1)
						std::cout << "today" << std::endl;
					else
						std::cout << "in last " << ndays << " days" << std::endl;
				}

				return E_OK;
			}
		}

		// All checks passed - OK to set the time

		if (VERBOSE_NORMAL)
		{
			std::cout << "Adjusting plant time..." << std:: endl;
		}

		do
		{
			pcktID++;
			writePacketHeader(pcktBuf, 0x01, addr_unknown);
			writePacket(pcktBuf, 0x10, 0xA0, 0, anySUSyID, anySerial);
			writeLong(pcktBuf, 0xF000020A);
			writeLong(pcktBuf, 0x00236D00);
			writeLong(pcktBuf, 0x00236D00);
			writeLong(pcktBuf, 0x00236D00);
			// Get new host time
			hosttime = time(NULL);
			writeLong(pcktBuf, hosttime);
			writeLong(pcktBuf, hosttime);
			writeLong(pcktBuf, hosttime);
			writeLong(pcktBuf, tz | dst);
			writeLong(pcktBuf, ++magic);
			writeLong(pcktBuf, 1);
			writePacketTrailer(pcktBuf);
			writePacketLength(pcktBuf);
		}
		while (!isCrcValid(pcktBuf[packetposition-3], pcktBuf[packetposition-2]));

		bthSend(pcktBuf);
		// No response expected

		std::cout << "New plant time is now " << strftime_t(DateTimeFormat, hosttime) << std::endl;
	}
	else
		if (VERBOSE_NORMAL)
		{
			std::cout << "Failed to get current plant time (" << rc << ")" << std::endl;
		}

	return rc;
}


#define MAX_CFG_AD 300	// Days
#define MAX_CFG_AM 300	// Months
#define MAX_CFG_AE 300	// Months
int parseCmdline(int argc, char **argv, Config *cfg)
{
    cfg->debug = 0;			// debug level - 0=none, 5=highest
    cfg->verbose = 0;		// verbose level - 0=none, 5=highest
    cfg->archDays = 1;		// today only
    cfg->archMonths = 1;	// this month only
	cfg->archEventMonths = 1;	// this month only
    cfg->forceInq = 0;		// Inquire inverter also during the night
    cfg->userGroup = UG_USER;
    // WebSolarLog support (http://www.websolarlog.com/index.php/tag/sma-spot/)
    // This is an undocumented feature and should only be used for WebSolarLog
    cfg->wsl = 0;
    cfg->quiet = 0;
    cfg->nocsv = 0;
    cfg->nospot = 0;
	cfg->nosql = 0;
    // 123Solar Web Solar logger support(http://www.123solar.org/)
    // This is an undocumented feature and should only be used for 123solar
    cfg->s123 = S123_NOP;
	cfg->loadlive = 0;	//force settings to prepare for live loading to http://pvoutput.org/loadlive.jsp
	cfg->startdate = 0;
	cfg->settime = 0;
	cfg->mqtt = 0;

	bool help_requested = false;

    //Set quiet/help mode
    for (int i = 1; i < argc; i++)
	{
		if (*argv[i] == '/')
			*argv[i] = '-';
		
		if (stricmp(argv[i], "-q") == 0)
            cfg->quiet = 1;

		if (stricmp(argv[i], "-?") == 0)
            help_requested = true;
	}

	// Get path of executable
	// Fix Issue 169 (expand symlinks)
	cfg->AppPath = realpath(argv[0]);

	size_t pos = cfg->AppPath.find_last_of("/\\");
	if (pos != string::npos)
		cfg->AppPath.erase(++pos);
	else
		cfg->AppPath.clear();

	//Build fullpath to config file (SBFspot.cfg should be in same folder as SBFspot.exe)
	cfg->ConfigFile = cfg->AppPath + "SBFspot.cfg";

    char *pEnd = NULL;
    long lValue = 0;

    if ((cfg->quiet == 0) && (!help_requested))
    {
        SayHello(0);
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
                InvalidArg(argv[i]);
                return -1;
            }
            lValue = strtol(argv[i]+3, &pEnd, 10);
            if ((lValue < 0) || (lValue > MAX_CFG_AD) || (*pEnd != 0))
            {
                InvalidArg(argv[i]);
                return -1;
            }
            else
                cfg->archDays = (int)lValue;

        }

        //Set #months (archived monthdata)
        else if (strnicmp(argv[i], "-am", 3) == 0)
        {
			if (strlen(argv[i]) > 6)
            {
                InvalidArg(argv[i]);
                return -1;
            }
            lValue = strtol(argv[i]+3, &pEnd, 10);
            if ((lValue < 0) || (lValue > MAX_CFG_AM) || (*pEnd != 0))
            {
                InvalidArg(argv[i]);
                return -1;
            }
            else
                cfg->archMonths = (int)lValue;
        }

		//Set #days (archived events)
		else if (strnicmp(argv[i], "-ae", 3) == 0)
		{
			if (strlen(argv[i]) > 6)
			{
				InvalidArg(argv[i]);
				return -1;
			}
			lValue = strtol(argv[i]+3, &pEnd, 10);
			if ((lValue < 0) || (lValue > MAX_CFG_AE) || (*pEnd != 0))
			{
				InvalidArg(argv[i]);
				return -1;
			}
			else
				cfg->archEventMonths = (int)lValue;

		}

        //Set debug level
        else if(strnicmp(argv[i], "-d", 2) == 0)
        {
            lValue = strtol(argv[i]+2, &pEnd, 10);
            if (strlen(argv[i]) == 2) lValue = 2;	// only -d sets medium debug level
            if ((lValue < 0) || (lValue > 5) || (*pEnd != 0))
            {
                InvalidArg(argv[i]);
                return -1;
            }
            else
                cfg->debug = (int)lValue;
        }

        //Set verbose level
        else if (strnicmp(argv[i], "-v", 2) == 0)
        {
            lValue = strtol(argv[i]+2, &pEnd, 10);
            if (strlen(argv[i]) == 2) lValue = 2;	// only -v sets medium verbose level
            if ((lValue < 0) || (lValue > 5) || (*pEnd != 0))
            {
                InvalidArg(argv[i]);
                return -1;
            }
            else
                cfg->verbose = (int)lValue;
        }

		//force settings to prepare for live loading to http://pvoutput.org/loadlive.jsp
		else if ((stricmp(argv[i], "-liveload") == 0) || (stricmp(argv[i], "-loadlive") == 0))
			cfg->loadlive = 1;

        //Set inquiryDark flag
        else if (stricmp(argv[i], "-finq") == 0)
            cfg->forceInq = 1;

        //Set WebSolarLog flag (Undocumented - For WSL usage only)
        else if (stricmp(argv[i], "-wsl") == 0)
            cfg->wsl = 1;

        //Set 123Solar command value (Undocumented - For WSL usage only)
        else if (strnicmp(argv[i], "-123s", 5) == 0)
        {
            if (strlen(argv[i]) == 5)
                cfg->s123 = S123_DATA;
            else if (strnicmp(argv[i]+5, "=DATA", 5) == 0)
                cfg->s123 = S123_DATA;
            else if (strnicmp(argv[i]+5, "=INFO", 5) == 0)
                cfg->s123 = S123_INFO;
            else if (strnicmp(argv[i]+5, "=SYNC", 5) == 0)
                cfg->s123 = S123_SYNC;
            else if (strnicmp(argv[i]+5, "=STATE", 6) == 0)
                cfg->s123 = S123_STATE;
            else
            {
                InvalidArg(argv[i]);
                return -1;
            }
        }

        //Set NoCSV flag (Disable CSV export - Overrules Config setting)
        else if (stricmp(argv[i], "-nocsv") == 0)
            cfg->nocsv = 1;

        //Set NoSQL flag (Disable SQL export)
        else if (stricmp(argv[i], "-nosql") == 0)
            cfg->nosql = 1;

		//Set NoSpot flag (Disable Spot CSV export)
        else if (stricmp(argv[i], "-sp0") == 0)
            cfg->nospot = 1;

        else if (stricmp(argv[i], "-installer") == 0)
            cfg->userGroup = UG_INSTALLER;

        else if (strnicmp(argv[i], "-password:", 10) == 0)
            if (strlen(argv[i]) == 10)
            {
                InvalidArg(argv[i]);
                return -1;
            }
			else
			{
				memset(cfg->SMA_Password, 0, sizeof(cfg->SMA_Password));
				strncpy(cfg->SMA_Password, argv[i] + 10, sizeof(cfg->SMA_Password) - 1);
			}

		else if (strnicmp(argv[i], "-startdate:", 11) == 0)
		{
            if (strlen(argv[i]) == 11)
            {
                InvalidArg(argv[i]);
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
					cfg->startdate = mktime(&tm_start);
					if (-1 == cfg->startdate)
					{
						InvalidArg(argv[i]);
						return -1;
					}
				}
				else
				{
					InvalidArg(argv[i]);
					return -1;
				}
 			}
		}

		//look for alternative config file
        else if (strnicmp(argv[i], "-cfg", 4) == 0)
        {
            if (strlen(argv[i]) == 4)
            {
                InvalidArg(argv[i]);
                return -1;
            }
            else
			{
				//Fix Issue G90 (code.google.com)
				//If -cfg arg has no '\' it's only a filename and should be in the same folder as SBFspot executable
				cfg->ConfigFile = argv[i] + 4;
				if (cfg->ConfigFile.find_first_of("/\\") == string::npos)
					cfg->ConfigFile = cfg->AppPath + (argv[i] + 4);
			}
        }

        else if (stricmp(argv[i], "-settime") == 0)
			cfg->settime = 1;

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
			cfg->mqtt = 1;

        //Show Help
        else if (stricmp(argv[i], "-?") == 0)
        {
            SayHello(1);
            return 1;	// Caller should terminate, no error
        }

        else if (cfg->quiet == 0)
        {
            InvalidArg(argv[i]);
            return -1;
        }

    }

	if (cfg->settime == 1)
	{
		// Verbose output level should be at least = 2 (normal)
		if (cfg->verbose < 2)
			cfg->verbose = 2;

		cfg->forceInq = 1;
	}

    //Disable verbose/debug modes when silent
    if (cfg->quiet == 1)
    {
        cfg->verbose = 0;
        cfg->debug = 0;
    }

    return 0;
}

void InvalidArg(char *arg)
{
    std::cout << "Invalid argument: " << arg << "\nUse -? for help" << std::endl;
}

void SayHello(int ShowHelp)
{
#if __BYTE_ORDER == __BIG_ENDIAN
#define BYTEORDER "BE"
#else
#define BYTEORDER "LE"
#endif
    std::cout << "SBFspot V" << VERSION << "\n";
    std::cout << "Yet another tool to read power production of SMA solar inverters\n";
    std::cout << "(c) 2012-2020, SBF (https://github.com/SBFspot/SBFspot)\n";
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
		std::cout << " -mqtt               Publish spot data to MQTT broker\n" << std::endl;

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

//month: January = 0, February = 1...
int DaysInMonth(int month, int year)
{
    const int days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if ((month < 0) || (month > 11)) return 0;  //Error - Invalid month
    // If febuary, check for leap year
    if ((month == 1) && ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)))
        return 29;
    else
        return days[month];
}

/* read Config from file */
int GetConfig(Config *cfg)
{
    //Initialise config structure and set default values
    strncpy(cfg->prgVersion, VERSION, sizeof(cfg->prgVersion));
    memset(cfg->BT_Address, 0, sizeof(cfg->BT_Address));
    memset(cfg->IP_Address, 0, sizeof(cfg->IP_Address));
    cfg->outputPath[0] = 0;
	cfg->outputPath_Events[0] = 0;
    if (cfg->userGroup == UG_USER) cfg->SMA_Password[0] = 0;
    cfg->plantname[0] = 0;
    cfg->latitude = 0.0;
    cfg->longitude = 0.0;
    cfg->archdata_from = 0;
    cfg->archdata_to = 0;
    cfg->delimiter = ';';
    cfg->precision = 3;
    cfg->decimalpoint = ',';
    cfg->BT_Timeout = 5;
    cfg->BT_ConnectRetries = 10;

    cfg->calcMissingSpot = 0;
    strcpy(cfg->DateTimeFormat, "%d/%m/%Y %H:%M:%S");
    strcpy(cfg->DateFormat, "%d/%m/%Y");
    strcpy(cfg->TimeFormat, "%H:%M:%S");
    cfg->synchTime = 1;
    cfg->CSV_Export = 1;
    cfg->CSV_ExtendedHeader = 1;
    cfg->CSV_Header = 1;
    cfg->CSV_SaveZeroPower = 1;
    cfg->SunRSOffset = 900;
    cfg->SpotTimeSource = 0;
    cfg->SpotWebboxHeader = 0;
    cfg->MIS_Enabled = 0;
	strcpy(cfg->locale, "en-US");
	cfg->synchTimeLow = 1;
	cfg->synchTimeHigh = 3600;
	// MQTT default values
	cfg->mqtt_host = "localhost";
	cfg->mqtt_port = ""; // mosquitto: 1883/8883 for TLS
#if defined(WIN32)
	cfg->mqtt_publish_exe = "%ProgramFiles%\\mosquitto\\mosquitto_pub.exe";
#else
	cfg->mqtt_publish_exe = "/usr/bin/mosquitto_pub";
#endif
	cfg->mqtt_topic = "sbfspot";
	cfg->mqtt_publish_args = "-h {host} -t {topic} -m \"{{message}}\"";
	cfg->mqtt_publish_data = "Timestamp,SunRise,SunSet,InvSerial,InvName,InvStatus,EToday,ETotal,PACTot,UDC1,UDC2,IDC1,IDC2,PDC1,PDC2";
	cfg->mqtt_item_format = "\"{key}\": {value}";

    const char *CFG_Boolean = "(0-1)";
    const char *CFG_InvalidValue = "Invalid value for '%s' %s\n";

    FILE *fp;

	if ((fp = fopen(cfg->ConfigFile.c_str(), "r")) == NULL)
    {
        std::cerr << "Error! Could not open file " << cfg->ConfigFile << std::endl;
        return -1;
    }

	if (cfg->verbose >= 2)
		std::cout << "Reading config '" << cfg->ConfigFile << "'" << std::endl;

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
					memset(cfg->BT_Address, 0, sizeof(cfg->BT_Address));
					strncpy(cfg->BT_Address, value, sizeof(cfg->BT_Address) - 1);
				}
                else if(strnicmp(variable, "IP_Address", 10) == 0)
				{
					boost::split(cfg->ip_addresslist, value, boost::is_any_of(","));
					for (unsigned int i = 0; i < cfg->ip_addresslist.size(); i++)
					{
						try
						{
						    boost::asio::ip::address ipv4Addr = boost::asio::ip::address::from_string(cfg->ip_addresslist[i]);
							if (!ipv4Addr.is_v4())
								throw -2;
						}
						catch (...)
						{
	                        std::cerr << "Invalid value for '" << variable << "' " << cfg->ip_addresslist[i] << std::endl;
		                    rc = -2;
							break;
						}
					}

					if (rc == 0)
					{
						memset(cfg->IP_Address, 0, sizeof(cfg->IP_Address));
						strncpy(cfg->IP_Address, cfg->ip_addresslist[0].c_str(), sizeof(cfg->IP_Address) - 1);
					}
				}
				else if(stricmp(variable, "Password") == 0)
                {
					if (cfg->userGroup == UG_USER)
					{
						memset(cfg->SMA_Password, 0, sizeof(cfg->SMA_Password));
						strncpy(cfg->SMA_Password, value, sizeof(cfg->SMA_Password) - 1);
					}
                }
				else if (stricmp(variable, "OutputPath") == 0)
				{
					memset(cfg->outputPath, 0, sizeof(cfg->outputPath));
					strncpy(cfg->outputPath, value, sizeof(cfg->outputPath) - 1);
				}
				else if (stricmp(variable, "OutputPathEvents") == 0)
				{
					memset(cfg->outputPath_Events, 0, sizeof(cfg->outputPath_Events));
					strncpy(cfg->outputPath_Events, value, sizeof(cfg->outputPath_Events) - 1);
				}
				else if(stricmp(variable, "Latitude") == 0) cfg->latitude = (float)atof(value);
				else if(stricmp(variable, "Longitude") == 0) cfg->longitude = (float)atof(value);
				else if (stricmp(variable, "Plantname") == 0)
				{
					memset(cfg->plantname, 0, sizeof(cfg->plantname));
					strncpy(cfg->plantname, value, sizeof(cfg->plantname) - 1);
				}
				else if(stricmp(variable, "CalculateMissingSpotValues") == 0)
                {
                    lValue = strtol(value, &pEnd, 10);
                    if (((lValue == 0) || (lValue == 1)) && (*pEnd == 0))
                        cfg->calcMissingSpot = (int)lValue;
                    else
                    {
                        fprintf(stderr, CFG_InvalidValue, variable, CFG_Boolean);
                        rc = -2;
                    }
                }
				else if (stricmp(variable, "DatetimeFormat") == 0)
				{
					memset(cfg->DateTimeFormat, 0, sizeof(cfg->DateTimeFormat));
					strncpy(cfg->DateTimeFormat, value, sizeof(cfg->DateTimeFormat) - 1);
				}
				else if (stricmp(variable, "DateFormat") == 0)
				{
					memset(cfg->DateFormat, 0, sizeof(cfg->DateFormat));
					strncpy(cfg->DateFormat, value, sizeof(cfg->DateFormat)-1);
				}
				else if (stricmp(variable, "TimeFormat") == 0)
				{
					memset(cfg->TimeFormat, 0, sizeof(cfg->TimeFormat));
					strncpy(cfg->TimeFormat, value, sizeof(cfg->TimeFormat) - 1);
				}
				else if(stricmp(variable, "DecimalPoint") == 0)
                {
					if (stricmp(value, "comma") == 0) cfg->decimalpoint = ',';
					else if ((stricmp(value, "dot") == 0) || (stricmp(value, "point") == 0)) cfg->decimalpoint = '.'; // Fix Issue 84 - 'Point' is accepted for backward compatibility
                    else
                    {
						fprintf(stderr, CFG_InvalidValue, variable, "(comma|dot)");
                        rc = -2;
                    }
                }
				else if(stricmp(variable, "CSV_Delimiter") == 0)
                {
					if (stricmp(value, "comma") == 0) cfg->delimiter = ',';
					else if (stricmp(value, "semicolon") == 0) cfg->delimiter = ';';
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
                        cfg->synchTime = (int)lValue;
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
						cfg->synchTimeLow = (int)lValue;
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
						cfg->synchTimeHigh = (int)lValue;
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
                        cfg->CSV_Export = (int)lValue;
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
                        cfg->CSV_ExtendedHeader = (int)lValue;
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
                        cfg->CSV_Header = (int)lValue;
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
                        cfg->CSV_SaveZeroPower = (int)lValue;
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
                        cfg->SunRSOffset = (int)lValue;
                    else
                    {
                        fprintf(stderr, CFG_InvalidValue, variable, "(0-3600)");
                        rc = -2;
                    }
                }
				else if(stricmp(variable, "CSV_Spot_TimeSource") == 0)
                {
					if (stricmp(value, "Inverter") == 0) cfg->SpotTimeSource = 0;
					else if (stricmp(value, "Computer") == 0) cfg->SpotTimeSource = 1;
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
                        cfg->SpotWebboxHeader = (int)lValue;
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
                        cfg->MIS_Enabled = (int)lValue;
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
						strcpy(cfg->locale, value);
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
						cfg->BT_ConnectRetries = (int)lValue;
                    else
                    {
                        fprintf(stderr, CFG_InvalidValue, variable, "(1-15)");
                        rc = -2;
                    }
                }
				else if(stricmp(variable, "Timezone") == 0)
				{
					cfg->timezone = value;
					boost::local_time::tz_database tzDB;
					string tzdbPath = cfg->AppPath + "date_time_zonespec.csv";
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
					cfg->tz = tzDB.time_zone_from_region(value);
					if (!cfg->tz)
					{
						cout << "Invalid timezone specified: " << value << endl;
						return -2;
					}
				}

				else if(stricmp(variable, "SQL_Database") == 0)
					cfg->sqlDatabase = value;
#if defined(USE_MYSQL)
				else if(stricmp(variable, "SQL_Hostname") == 0)
					cfg->sqlHostname = value;
				else if(stricmp(variable, "SQL_Username") == 0)
					cfg->sqlUsername = value;
				else if(stricmp(variable, "SQL_Password") == 0)
					cfg->sqlUserPassword = value;
#endif
				else if (stricmp(variable, "MQTT_Host") == 0)
					cfg->mqtt_host = value;
				else if (stricmp(variable, "MQTT_Port") == 0)
					cfg->mqtt_port = value;
				else if (stricmp(variable, "MQTT_Publisher") == 0)
					cfg->mqtt_publish_exe = value;
				else if (stricmp(variable, "MQTT_PublisherArgs") == 0)
					cfg->mqtt_publish_args = value;
				else if (stricmp(variable, "MQTT_Topic") == 0)
					cfg->mqtt_topic = value;
				else if (stricmp(variable, "MQTT_ItemFormat") == 0)
					cfg->mqtt_item_format = value;
				else if (stricmp(variable, "MQTT_Data") == 0)
					cfg->mqtt_publish_data = value;
				else if (stricmp(variable, "MQTT_ItemDelimiter") == 0)
				{
					if (stricmp(value, "comma") == 0) cfg->mqtt_item_delimiter = ",";
					else if (stricmp(value, "semicolon") == 0) cfg->mqtt_item_delimiter = ";";
					else if (stricmp(value, "blank") == 0) cfg->mqtt_item_delimiter = " ";
					else if (stricmp(value, "none") == 0) cfg->mqtt_item_delimiter = "";
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

    if (strlen(cfg->BT_Address) > 0)
        cfg->ConnectionType = CT_BLUETOOTH;
    else
    {
        cfg->ConnectionType = CT_ETHERNET;
        cfg->IP_Port = 9522;
    }

    if (strlen(cfg->SMA_Password) == 0)
    {
        fprintf(stderr, "Missing USER Password.\n");
        rc = -2;
    }

    if (cfg->decimalpoint == cfg->delimiter)
    {
        fprintf(stderr, "'CSV_Delimiter' and 'DecimalPoint' must be different character.\n");
        rc = -2;
    }

    //Overrule CSV_Export from config with Commandline setting -nocsv
    if (cfg->nocsv == 1)
        cfg->CSV_Export = 0;

    //Silently enable CSV_Header when CSV_ExtendedHeader is enabled
    if (cfg->CSV_ExtendedHeader == 1)
        cfg ->CSV_Header = 1;

    if (strlen(cfg->outputPath) == 0)
    {
		fprintf(stderr, "Missing OutputPath.\n");
		rc = -2;
    }

	//If OutputPathEvents is omitted, use OutputPath
	if (strlen(cfg->outputPath_Events) == 0)
		strcpy(cfg->outputPath_Events, cfg->outputPath);

    if (strlen(cfg->plantname) == 0)
    {
        strncpy(cfg->plantname, "MyPlant", sizeof(cfg->plantname));
    }

	if (cfg->timezone.empty())
	{
		cout << "Missing timezone.\n";
		rc = -2;
	}

	//force settings to prepare for live loading to http://pvoutput.org/loadlive.jsp
	if (cfg->loadlive == 1)
	{
		strncat(cfg->outputPath, "/LoadLive", sizeof(cfg->outputPath));
		strcpy(cfg->DateTimeFormat, "%H:%M");
		cfg->CSV_Export = 1;
		cfg->decimalpoint = '.';
		cfg->CSV_Header = 0;
		cfg->CSV_ExtendedHeader = 0;
		cfg->CSV_SaveZeroPower = 0;
		cfg->delimiter = ';';
//		cfg->PVoutput = 0;
		cfg->archEventMonths = 0;
		cfg->archMonths = 0;
		cfg->nospot = 1;
	}

	// If 1st day of the month and -am1 specified, force to -am2 to get last day of prev month
	if (cfg->archMonths == 1)
	{
		time_t now = time(NULL);
		struct tm *tm_now = localtime(&now);
		if (tm_now->tm_mday == 1)
			cfg->archMonths++;
	}

	if (cfg->verbose > 2) ShowConfig(cfg);

    return rc;
}

void ShowConfig(Config *cfg)
{
    std::cout << "Configuration settings:";
	if (strlen(cfg->IP_Address) == 0)	// No IP address -> Show BT address
		std::cout << "\nBTAddress=" << cfg->BT_Address;
	if (strlen(cfg->BT_Address) == 0)	// No BT address -> Show IP address
		std::cout << "\nIP_Address=" << cfg->IP_Address;
	std::cout << "\nPassword=<undisclosed>" << \
		"\nMIS_Enabled=" << cfg->MIS_Enabled << \
		"\nPlantname=" << cfg->plantname << \
		"\nOutputPath=" << cfg->outputPath << \
		"\nOutputPathEvents=" << cfg->outputPath_Events << \
		"\nLatitude=" << cfg->latitude << \
		"\nLongitude=" << cfg->longitude << \
		"\nTimezone=" << cfg->timezone << \
		"\nCalculateMissingSpotValues=" << cfg->calcMissingSpot << \
		"\nDateTimeFormat=" << cfg->DateTimeFormat << \
		"\nDateFormat=" << cfg->DateFormat << \
		"\nTimeFormat=" << cfg->TimeFormat << \
		"\nSynchTime=" << cfg->synchTime << \
		"\nSynchTimeLow=" << cfg->synchTimeLow << \
		"\nSynchTimeHigh=" << cfg->synchTimeHigh << \
		"\nSunRSOffset=" << cfg->SunRSOffset << \
		"\nDecimalPoint=" << dp2txt(cfg->decimalpoint) << \
		"\nCSV_Delimiter=" << delim2txt(cfg->delimiter) << \
		"\nPrecision=" << cfg->precision << \
		"\nCSV_Export=" << cfg->CSV_Export << \
		"\nCSV_ExtendedHeader=" << cfg->CSV_ExtendedHeader << \
		"\nCSV_Header=" << cfg->CSV_Header << \
		"\nCSV_SaveZeroPower=" << cfg->CSV_SaveZeroPower << \
		"\nCSV_Spot_TimeSource=" << cfg->SpotTimeSource << \
		"\nCSV_Spot_WebboxHeader=" << cfg->SpotWebboxHeader << \
		"\nLocale=" << cfg->locale << \
		"\nBTConnectRetries=" << cfg->BT_ConnectRetries << std::endl;

#if defined(USE_MYSQL) || defined(USE_SQLITE)
	std::cout << "SQL_Database=" << cfg->sqlDatabase << std::endl;
#endif

#if defined(USE_MYSQL)
	std::cout << "SQL_Hostname=" << cfg->sqlHostname << \
		"\nSQL_Username=" << cfg->sqlUsername << \
		"\nSQL_Password=<undisclosed>" << std::endl;
#endif

	if (cfg->mqtt == 1)
	{
		std::cout << "MQTT_Host=" << cfg->mqtt_host << \
			"\nMQTT_Port=" << cfg->mqtt_port << \
			"\nMQTT_Topic=" << cfg->mqtt_topic << \
			"\nMQTT_Publisher=" << cfg->mqtt_publish_exe << \
			"\nMQTT_PublisherArgs=" << cfg->mqtt_publish_args << \
			"\nMQTT_Data=" << cfg->mqtt_publish_data << \
			"\nMQTT_ItemFormat=" << cfg->mqtt_item_format << std::endl;
	}

	std::cout << "### End of Config ###" << std::endl;
}

int isCrcValid(unsigned char lb, unsigned char hb)
{
    if (ConnType == CT_BLUETOOTH)
    {
        if ((lb == 0x7E) || (hb == 0x7E) ||
                (lb == 0x7D) || (hb == 0x7D))
            return 0;
        else
            return 1;
    }
    else
        return 1;   //Always true for ethernet
}

//Power Values are missing on some inverters
void CalcMissingSpot(InverterData *invData)
{
	if (invData->Pdc1 == 0) invData->Pdc1 = (invData->Idc1 * invData->Udc1) / 100000;
	if (invData->Pdc2 == 0) invData->Pdc2 = (invData->Idc2 * invData->Udc2) / 100000;

	if (invData->Pac1 == 0) invData->Pac1 = (invData->Iac1 * invData->Uac1) / 100000;
	if (invData->Pac2 == 0) invData->Pac2 = (invData->Iac2 * invData->Uac2) / 100000;
	if (invData->Pac3 == 0) invData->Pac3 = (invData->Iac3 * invData->Uac3) / 100000;

    if (invData->TotalPac == 0) invData->TotalPac = invData->Pac1 + invData->Pac2 + invData->Pac3;
}

/*
 * isValidSender() compares 6-byte senderaddress with our inverter BT address
 * If senderaddress = addr_unknown (FF:FF:FF:FF:FF:FF) then any address is valid
 */
int isValidSender(unsigned char senderaddr[6], unsigned char address[6])
{
    for (int i = 0; i < 6; i++)
        if ((senderaddr[i] != address[i]) && (senderaddr[i] != 0xFF))
            return 0;

    return 1;
}

int getInverterData(InverterData *devList[], enum getInverterDataType type)
{
    if (DEBUG_NORMAL) printf("getInverterData(%d)\n", type);
    const char *strWatt = "%-12s: %ld (W) %s";
    const char *strVolt = "%-12s: %.2f (V) %s";
    const char *strAmp = "%-12s: %.3f (A) %s";
    const char *strkWh = "%-12s: %.3f (kWh) %s";
    const char *strHour = "%-12s: %.3f (h) %s";

    int rc = E_OK;

    int recordsize = 0;
    int validPcktID = 0;

    unsigned long command;
    unsigned long first;
    unsigned long last;

    switch(type)
    {
    case EnergyProduction:
        // SPOT_ETODAY, SPOT_ETOTAL
        command = 0x54000200;
        first = 0x00260100;
        last = 0x002622FF;
        break;

    case SpotDCPower:
        // SPOT_PDC1, SPOT_PDC2
        command = 0x53800200;
        first = 0x00251E00;
        last = 0x00251EFF;
        break;

    case SpotDCVoltage:
        // SPOT_UDC1, SPOT_UDC2, SPOT_IDC1, SPOT_IDC2
        command = 0x53800200;
        first = 0x00451F00;
        last = 0x004521FF;
        break;

    case SpotACPower:
        // SPOT_PAC1, SPOT_PAC2, SPOT_PAC3
        command = 0x51000200;
        first = 0x00464000;
        last = 0x004642FF;
        break;

    case SpotACVoltage:
        // SPOT_UAC1, SPOT_UAC2, SPOT_UAC3, SPOT_IAC1, SPOT_IAC2, SPOT_IAC3
        command = 0x51000200;
        first = 0x00464800;
        last = 0x004655FF;
        break;

    case SpotGridFrequency:
        // SPOT_FREQ
        command = 0x51000200;
        first = 0x00465700;
        last = 0x004657FF;
        break;

    case MaxACPower:
        // INV_PACMAX1, INV_PACMAX2, INV_PACMAX3
        command = 0x51000200;
        first = 0x00411E00;
        last = 0x004120FF;
        break;

    case MaxACPower2:
        // INV_PACMAX1_2
        command = 0x51000200;
        first = 0x00832A00;
        last = 0x00832AFF;
        break;

    case SpotACTotalPower:
        // SPOT_PACTOT
        command = 0x51000200;
        first = 0x00263F00;
        last = 0x00263FFF;
        break;

    case TypeLabel:
        // INV_NAME, INV_TYPE, INV_CLASS
        command = 0x58000200;
        first = 0x00821E00;
        last = 0x008220FF;
        break;

    case SoftwareVersion:
        // INV_SWVERSION
        command = 0x58000200;
        first = 0x00823400;
        last = 0x008234FF;
        break;

    case DeviceStatus:
        // INV_STATUS
        command = 0x51800200;
        first = 0x00214800;
        last = 0x002148FF;
        break;

    case GridRelayStatus:
        // INV_GRIDRELAY
        command = 0x51800200;
        first = 0x00416400;
        last = 0x004164FF;
        break;

    case OperationTime:
        // SPOT_OPERTM, SPOT_FEEDTM
        command = 0x54000200;
        first = 0x00462E00;
        last = 0x00462FFF;
        break;

    case BatteryChargeStatus:
        command = 0x51000200;
        first = 0x00295A00;
        last = 0x00295AFF;
        break;

    case BatteryInfo:
        command = 0x51000200;
        first = 0x00491E00;
        last = 0x00495DFF;
        break;

	case InverterTemperature:
		command = 0x52000200;
		first = 0x00237700;
		last = 0x002377FF;
		break;

	case sbftest:
		command = 0x64020200;
		first = 0x00618D00;
		last = 0x00618DFF;

		case MeteringGridMsTotW:
		command = 0x51000200;
		first = 0x00463600;
		last = 0x004637FF;
		break;

    default:
        return E_BADARG;
    };

    for (int i=0; devList[i]!=NULL && i<MAX_INVERTERS; i++)
    {
		do
		{
			pcktID++;
			writePacketHeader(pcktBuf, 0x01, addr_unknown);
			if (devList[i]->SUSyID == SID_SB240)
				writePacket(pcktBuf, 0x09, 0xE0, 0, devList[i]->SUSyID, devList[i]->Serial);
			else
				writePacket(pcktBuf, 0x09, 0xA0, 0, devList[i]->SUSyID, devList[i]->Serial);
			writeLong(pcktBuf, command);
			writeLong(pcktBuf, first);
			writeLong(pcktBuf, last);
			writePacketTrailer(pcktBuf);
			writePacketLength(pcktBuf);
		}
		while (!isCrcValid(pcktBuf[packetposition-3], pcktBuf[packetposition-2]));

		if (ConnType == CT_BLUETOOTH)
		{
			bthSend(pcktBuf);
		}
		else
		{
			ethSend(pcktBuf, devList[i]->IPAddress);
		}

		validPcktID = 0;
        do
        {
            if (ConnType == CT_BLUETOOTH)
                rc = getPacket(devList[i]->BTAddress, 1);
            else
                rc = ethGetPacket();

            if (rc != E_OK) return rc;

            if ((ConnType == CT_BLUETOOTH) && (!validateChecksum()))
                return E_CHKSUM;
            else
            {
                unsigned short rcvpcktID = get_short(pcktBuf+27) & 0x7FFF;
                if (pcktID == rcvpcktID)
                {
                    int inv = getInverterIndexBySerial(devList, get_short(pcktBuf + 15), get_long(pcktBuf + 17));
                    if (inv >= 0)
                    {
                        validPcktID = 1;
                        int32_t value = 0;
                        int64_t value64 = 0;
                        unsigned char Vtype = 0;
                        unsigned char Vbuild = 0;
                        unsigned char Vminor = 0;
                        unsigned char Vmajor = 0;
                        for (int ii = 41; ii < packetposition - 3; ii += recordsize)
                        {
                            uint32_t code = ((uint32_t)get_long(pcktBuf + ii));
                            LriDef lri = (LriDef)(code & 0x00FFFF00);
                            uint32_t cls = code & 0xFF;
                            unsigned char dataType = code >> 24;
                            time_t datetime = (time_t)get_long(pcktBuf + ii + 4);

                            // fix: We can't rely on dataType because it can be both 0x00 or 0x40 for DWORDs
                            if ((lri == MeteringDyWhOut) || (lri == MeteringTotWhOut) || (lri == MeteringTotFeedTms) || (lri == MeteringTotOpTms))	//QWORD
                            //if ((code == SPOT_ETODAY) || (code == SPOT_ETOTAL) || (code == SPOT_FEEDTM) || (code == SPOT_OPERTM))	//QWORD
                            {
                                value64 = get_longlong(pcktBuf + ii + 8);
                                if ((value64 == (int64_t)NaN_S64) || (value64 == (int64_t)NaN_U64)) value64 = 0;
                            }
                            else if ((dataType != 0x10) && (dataType != 0x08))	//Not TEXT or STATUS, so it should be DWORD
                            {
                                value = (int32_t)get_long(pcktBuf + ii + 16);
                                if ((value == (int32_t)NaN_S32) || (value == (int32_t)NaN_U32)) value = 0;
                            }

                            switch (lri)
                            {
                            case GridMsTotW: //SPOT_PACTOT
                                if (recordsize == 0) recordsize = 28;
                                //This function gives us the time when the inverter was switched off
                                devList[inv]->SleepTime = datetime;
                                devList[inv]->TotalPac = value;
                                devList[inv]->flags |= type;
                                if (DEBUG_NORMAL) printf(strWatt, "SPOT_PACTOT", value, ctime(&datetime));
                                break;

                            case OperationHealthSttOk: //INV_PACMAX1
                                if (recordsize == 0) recordsize = 28;
                                devList[inv]->Pmax1 = value;
                                devList[inv]->flags |= type;
                                if (DEBUG_NORMAL) printf(strWatt, "INV_PACMAX1", value, ctime(&datetime));
                                break;

                            case OperationHealthSttWrn: //INV_PACMAX2
                                if (recordsize == 0) recordsize = 28;
                                devList[inv]->Pmax2 = value;
                                devList[inv]->flags |= type;
                                if (DEBUG_NORMAL) printf(strWatt, "INV_PACMAX2", value, ctime(&datetime));
                                break;

                            case OperationHealthSttAlm: //INV_PACMAX3
                                if (recordsize == 0) recordsize = 28;
                                devList[inv]->Pmax3 = value;
                                devList[inv]->flags |= type;
                                if (DEBUG_NORMAL) printf(strWatt, "INV_PACMAX3", value, ctime(&datetime));
                                break;

                            case GridMsWphsA: //SPOT_PAC1
                                if (recordsize == 0) recordsize = 28;
                                devList[inv]->Pac1 = value;
                                devList[inv]->flags |= type;
                                if (DEBUG_NORMAL) printf(strWatt, "SPOT_PAC1", value, ctime(&datetime));
                                break;

                            case GridMsWphsB: //SPOT_PAC2
                                if (recordsize == 0) recordsize = 28;
                                devList[inv]->Pac2 = value;
                                devList[inv]->flags |= type;
                                if (DEBUG_NORMAL) printf(strWatt, "SPOT_PAC2", value, ctime(&datetime));
                                break;

                            case GridMsWphsC: //SPOT_PAC3
                                if (recordsize == 0) recordsize = 28;
                                devList[inv]->Pac3 = value;
                                devList[inv]->flags |= type;
                                if (DEBUG_NORMAL) printf(strWatt, "SPOT_PAC3", value, ctime(&datetime));
                                break;

                            case GridMsPhVphsA: //SPOT_UAC1
                                if (recordsize == 0) recordsize = 28;
                                devList[inv]->Uac1 = value;
                                devList[inv]->flags |= type;
                                if (DEBUG_NORMAL) printf(strVolt, "SPOT_UAC1", toVolt(value), ctime(&datetime));
                                break;

                            case GridMsPhVphsB: //SPOT_UAC2
                                if (recordsize == 0) recordsize = 28;
                                devList[inv]->Uac2 = value;
                                devList[inv]->flags |= type;
                                if (DEBUG_NORMAL) printf(strVolt, "SPOT_UAC2", toVolt(value), ctime(&datetime));
                                break;

                            case GridMsPhVphsC: //SPOT_UAC3
                                if (recordsize == 0) recordsize = 28;
                                devList[inv]->Uac3 = value;
                                devList[inv]->flags |= type;
                                if (DEBUG_NORMAL) printf(strVolt, "SPOT_UAC3", toVolt(value), ctime(&datetime));
                                break;

                            case GridMsAphsA_1: //SPOT_IAC1
							case GridMsAphsA:
                                if (recordsize == 0) recordsize = 28;
                                devList[inv]->Iac1 = value;
                                devList[inv]->flags |= type;
                                if (DEBUG_NORMAL) printf(strAmp, "SPOT_IAC1", toAmp(value), ctime(&datetime));
                                break;

                            case GridMsAphsB_1: //SPOT_IAC2
							case GridMsAphsB:
                                if (recordsize == 0) recordsize = 28;
                                devList[inv]->Iac2 = value;
                                devList[inv]->flags |= type;
                                if (DEBUG_NORMAL) printf(strAmp, "SPOT_IAC2", toAmp(value), ctime(&datetime));
                                break;

                            case GridMsAphsC_1: //SPOT_IAC3
							case GridMsAphsC:
                                if (recordsize == 0) recordsize = 28;
                                devList[inv]->Iac3 = value;
                                devList[inv]->flags |= type;
                                if (DEBUG_NORMAL) printf(strAmp, "SPOT_IAC3", toAmp(value), ctime(&datetime));
                                break;

                            case GridMsHz: //SPOT_FREQ
                                if (recordsize == 0) recordsize = 28;
                                devList[inv]->GridFreq = value;
                                devList[inv]->flags |= type;
                                if (DEBUG_NORMAL) printf("%-12s: %.2f (Hz) %s", "SPOT_FREQ", toHz(value), ctime(&datetime));
                                break;

                            case DcMsWatt: //SPOT_PDC1 / SPOT_PDC2
                                if (recordsize == 0) recordsize = 28;
                                if (cls == 1)   // MPP1
                                {
                                    devList[inv]->Pdc1 = value;
                                    if (DEBUG_NORMAL) printf(strWatt, "SPOT_PDC1", value, ctime(&datetime));
                                }
                                if (cls == 2)   // MPP2
                                {
                                    devList[inv]->Pdc2 = value;
                                    if (DEBUG_NORMAL) printf(strWatt, "SPOT_PDC2", value, ctime(&datetime));
                                }
                                devList[inv]->flags |= type;
                                break;

                            case DcMsVol: //SPOT_UDC1 / SPOT_UDC2
                                if (recordsize == 0) recordsize = 28;
                                if (cls == 1)
                                {
                                    devList[inv]->Udc1 = value;
                                    if (DEBUG_NORMAL) printf(strVolt, "SPOT_UDC1", toVolt(value), ctime(&datetime));
                                }
                                if (cls == 2)
                                {
                                    devList[inv]->Udc2 = value;
                                    if (DEBUG_NORMAL) printf(strVolt, "SPOT_UDC2", toVolt(value), ctime(&datetime));
                                }
                                devList[inv]->flags |= type;
                                break;

                            case DcMsAmp: //SPOT_IDC1 / SPOT_IDC2
                                if (recordsize == 0) recordsize = 28;
                                if (cls == 1)
                                {
                                    devList[inv]->Idc1 = value;
                                    if (DEBUG_NORMAL) printf(strAmp, "SPOT_IDC1", toAmp(value), ctime(&datetime));
                                }
                                if (cls == 2)
                                {
                                    devList[inv]->Idc2 = value;
                                    if (DEBUG_NORMAL) printf(strAmp, "SPOT_IDC2", toAmp(value), ctime(&datetime));
                                }
                                devList[inv]->flags |= type;
                                break;

                            case MeteringTotWhOut: //SPOT_ETOTAL
                                if (recordsize == 0) recordsize = 16;
								//In case SPOT_ETODAY missing, this function gives us inverter time (eg: SUNNY TRIPOWER 6.0)
								devList[inv]->InverterDatetime = datetime;
								devList[inv]->ETotal = value64;
                                devList[inv]->flags |= type;
                                if (DEBUG_NORMAL) printf(strkWh, "SPOT_ETOTAL", tokWh(value64), ctime(&datetime));
                                break;

                            case MeteringDyWhOut: //SPOT_ETODAY
                                if (recordsize == 0) recordsize = 16;
                                //This function gives us the current inverter time
                                devList[inv]->InverterDatetime = datetime;
                                devList[inv]->EToday = value64;
                                devList[inv]->flags |= type;
                                if (DEBUG_NORMAL) printf(strkWh, "SPOT_ETODAY", tokWh(value64), ctime(&datetime));
                                break;

                            case MeteringTotOpTms: //SPOT_OPERTM
                                if (recordsize == 0) recordsize = 16;
                                devList[inv]->OperationTime = value64;
                                devList[inv]->flags |= type;
                                if (DEBUG_NORMAL) printf(strHour, "SPOT_OPERTM", toHour(value64), ctime(&datetime));
                                break;

                            case MeteringTotFeedTms: //SPOT_FEEDTM
                                if (recordsize == 0) recordsize = 16;
                                devList[inv]->FeedInTime = value64;
                                devList[inv]->flags |= type;
                                if (DEBUG_NORMAL) printf(strHour, "SPOT_FEEDTM", toHour(value64), ctime(&datetime));
                                break;

                            case NameplateLocation: //INV_NAME
                                if (recordsize == 0) recordsize = 40;
                                //This function gives us the time when the inverter was switched on
                                devList[inv]->WakeupTime = datetime;
                                strncpy(devList[inv]->DeviceName, (char *)pcktBuf + ii + 8, sizeof(devList[inv]->DeviceName)-1);
                                devList[inv]->flags |= type;
                                if (DEBUG_NORMAL) printf("%-12s: '%s' %s", "INV_NAME", devList[inv]->DeviceName, ctime(&datetime));
                                break;

                            case NameplatePkgRev: //INV_SWVER
                                if (recordsize == 0) recordsize = 40;
                                Vtype = pcktBuf[ii + 24];
                                char ReleaseType[4];
                                if (Vtype > 5)
                                    sprintf(ReleaseType, "%d", Vtype);
                                else
                                    sprintf(ReleaseType, "%c", "NEABRS"[Vtype]); //NOREV-EXPERIMENTAL-ALPHA-BETA-RELEASE-SPECIAL
                                Vbuild = pcktBuf[ii + 25];
                                Vminor = pcktBuf[ii + 26];
                                Vmajor = pcktBuf[ii + 27];
                                //Vmajor and Vminor = 0x12 should be printed as '12' and not '18' (BCD)
                                snprintf(devList[inv]->SWVersion, sizeof(devList[inv]->SWVersion), "%c%c.%c%c.%02d.%s", '0'+(Vmajor >> 4), '0'+(Vmajor & 0x0F), '0'+(Vminor >> 4), '0'+(Vminor & 0x0F), Vbuild, ReleaseType);
                                devList[inv]->flags |= type;
                                if (DEBUG_NORMAL) printf("%-12s: '%s' %s", "INV_SWVER", devList[inv]->SWVersion, ctime(&datetime));
                                break;

                            case NameplateModel: //INV_TYPE
                                if (recordsize == 0) recordsize = 40;
                                for (int idx = 8; idx < recordsize; idx += 4)
                                {
                                    unsigned long attribute = ((unsigned long)get_long(pcktBuf + ii + idx)) & 0x00FFFFFF;
                                    unsigned char status = pcktBuf[ii + idx + 3];
                                    if (attribute == 0xFFFFFE) break;	//End of attributes
                                    if (status == 1)
                                    {
										string devtype = tagdefs.getDesc(attribute);
										if (!devtype.empty())
										{
											memset(devList[inv]->DeviceType, 0, sizeof(devList[inv]->DeviceType));
											strncpy(devList[inv]->DeviceType, devtype.c_str(), sizeof(devList[inv]->DeviceType) - 1);
										}
										else
										{
											strncpy(devList[inv]->DeviceType, "UNKNOWN TYPE", sizeof(devList[inv]->DeviceType));
                                            printf("Unknown Inverter Type. Report this issue at https://github.com/SBFspot/SBFspot/issues with following info:\n");
                                            printf("0x%08lX and Inverter Type=<Fill in the exact type> (e.g. SB1300TL-10)\n", attribute);
										}
                                    }
                                }
                                devList[inv]->flags |= type;
                                if (DEBUG_NORMAL) printf("%-12s: '%s' %s", "INV_TYPE", devList[inv]->DeviceType, ctime(&datetime));
                                break;

                            case NameplateMainModel: //INV_CLASS
                                if (recordsize == 0) recordsize = 40;
                                for (int idx = 8; idx < recordsize; idx += 4)
                                {
                                    unsigned long attribute = ((unsigned long)get_long(pcktBuf + ii + idx)) & 0x00FFFFFF;
                                    unsigned char attValue = pcktBuf[ii + idx + 3];
                                    if (attribute == 0xFFFFFE) break;	//End of attributes
                                    if (attValue == 1)
                                    {
                                        devList[inv]->DevClass = (DEVICECLASS)attribute;
										string devclass = tagdefs.getDesc(attribute);
										if (!devclass.empty())
										{
											memset(devList[inv]->DeviceClass, 0, sizeof(devList[inv]->DeviceClass));
											strncpy(devList[inv]->DeviceClass, devclass.c_str(), sizeof(devList[inv]->DeviceClass) - 1);
										}
										else
										{
                                            strncpy(devList[inv]->DeviceClass, "UNKNOWN CLASS", sizeof(devList[inv]->DeviceClass));
                                            printf("Unknown Device Class. Report this issue at https://github.com/SBFspot/SBFspot/issues with following info:\n");
                                            printf("0x%08lX and Device Class=...\n", attribute);
                                        }
                                    }
                                }
                                devList[inv]->flags |= type;
                                if (DEBUG_NORMAL) printf("%-12s: '%s' %s", "INV_CLASS", devList[inv]->DeviceClass, ctime(&datetime));
                                break;

                            case OperationHealth: //INV_STATUS:
                                if (recordsize == 0) recordsize = 40;
                                for (int idx = 8; idx < recordsize; idx += 4)
                                {
                                    unsigned long attribute = ((unsigned long)get_long(pcktBuf + ii + idx)) & 0x00FFFFFF;
                                    unsigned char attValue = pcktBuf[ii + idx + 3];
                                    if (attribute == 0xFFFFFE) break;	//End of attributes
                                    if (attValue == 1)
                                        devList[inv]->DeviceStatus = attribute;
                                }
                                devList[inv]->flags |= type;
								if (DEBUG_NORMAL) printf("%-12s: '%s' %s", "INV_STATUS", tagdefs.getDesc(devList[inv]->DeviceStatus, "?").c_str(), ctime(&datetime));
                                break;

                            case OperationGriSwStt: //INV_GRIDRELAY
                                if (recordsize == 0) recordsize = 40;
                                for (int idx = 8; idx < recordsize; idx += 4)
                                {
                                    unsigned long attribute = ((unsigned long)get_long(pcktBuf + ii + idx)) & 0x00FFFFFF;
                                    unsigned char attValue = pcktBuf[ii + idx + 3];
                                    if (attribute == 0xFFFFFE) break;	//End of attributes
                                    if (attValue == 1)
                                        devList[inv]->GridRelayStatus = attribute;
                                }
                                devList[inv]->flags |= type;
                                if (DEBUG_NORMAL) printf("%-12s: '%s' %s", "INV_GRIDRELAY", tagdefs.getDesc(devList[inv]->GridRelayStatus, "?").c_str(), ctime(&datetime));
                                break;

                            case BatChaStt:
                                if (recordsize == 0) recordsize = 28;
                                devList[inv]->BatChaStt = value;
                                devList[inv]->flags |= type;
                                break;

                            case BatDiagCapacThrpCnt:
                                if (recordsize == 0) recordsize = 28;
                                devList[inv]->BatDiagCapacThrpCnt = value;
                                devList[inv]->flags |= type;
                                break;

                            case BatDiagTotAhIn:
                                if (recordsize == 0) recordsize = 28;
                                devList[inv]->BatDiagTotAhIn = value;
                                devList[inv]->flags |= type;
                                break;

                            case BatDiagTotAhOut:
                                if (recordsize == 0) recordsize = 28;
                                devList[inv]->BatDiagTotAhOut = value;
                                devList[inv]->flags |= type;
                                break;

                            case BatTmpVal:
                                if (recordsize == 0) recordsize = 28;
                                devList[inv]->BatTmpVal = value;
                                devList[inv]->flags |= type;
                                break;

                            case BatVol:
                                if (recordsize == 0) recordsize = 28;
                                devList[inv]->BatVol = value;
                                devList[inv]->flags |= type;
                                break;

                            case BatAmp:
                                if (recordsize == 0) recordsize = 28;
                                devList[inv]->BatAmp = value;
                                devList[inv]->flags |= type;
                                break;

							case CoolsysTmpNom:
                                if (recordsize == 0) recordsize = 28;
								devList[inv]->Temperature = value;
								devList[inv]->flags |= type;
								break;

							case MeteringGridMsTotWhOut:
                                if (recordsize == 0) recordsize = 28;
								devList[inv]->MeteringGridMsTotWOut = value;
								break;

							case MeteringGridMsTotWhIn:
                                if (recordsize == 0) recordsize = 28;
								devList[inv]->MeteringGridMsTotWIn = value;
								break;

                            default:
                                if (recordsize == 0) recordsize = 12;
                            }
                        }
                    }
                }
                else
                {
                    if (DEBUG_HIGHEST) printf("Packet ID mismatch. Expected %d, received %d\n", pcktID, rcvpcktID);
                }
            }
        }
        while (validPcktID == 0);
    }

    return E_OK;
}

void resetInverterData(InverterData *inv)
{
	inv->BatAmp = 0;
	inv->BatChaStt = 0;
	inv->BatDiagCapacThrpCnt = 0;
	inv->BatDiagTotAhIn = 0;
	inv->BatDiagTotAhOut = 0;
	inv->BatTmpVal = 0;
	inv->BatVol = 0;
	inv->BT_Signal = 0;
	inv->calEfficiency = 0;
	inv->calPacTot = 0;
	inv->calPdcTot = 0;
	inv->DevClass = AllDevices;
	inv->DeviceClass[0] = 0;
	inv->DeviceName[0] = 0;
	inv->DeviceStatus = 0;
	inv->DeviceType[0] = 0;
	inv->EToday = 0;
	inv->ETotal = 0;
	inv->FeedInTime = 0;
	inv->flags = 0;
	inv->GridFreq = 0;
	inv->GridRelayStatus = 0;
	inv->Iac1 = 0;
	inv->Iac2 = 0;
	inv->Iac3 = 0;
	inv->Idc1 = 0;
	inv->Idc2 = 0;
	inv->InverterDatetime = 0;
	inv->IPAddress[0] = 0;
	inv->modelID = 0;
	inv->NetID = 0;
	inv->OperationTime = 0;
	inv->Pac1 = 0;
	inv->Pac2 = 0;
	inv->Pac3 = 0;
	inv->Pdc1 = 0;
	inv->Pdc2 = 0;
	inv->Pmax1 = 0;
	inv->Pmax2 = 0;
	inv->Pmax3 = 0;
	inv->Serial = 0;
	inv->SleepTime = 0;
	inv->SUSyID = 0;
	inv->SWVersion[0] = 0;
	inv->Temperature = 0;
	inv->TotalPac = 0;
	inv->Uac1 = 0;
	inv->Uac2 = 0;
	inv->Uac3 = 0;
	inv->Udc1 = 0;
	inv->Udc2 = 0;
	inv->WakeupTime = 0;
	inv->monthDataOffset = 0;
	inv->multigateID = -1;
	inv->MeteringGridMsTotWIn = 0;
	inv->MeteringGridMsTotWOut = 0;
	inv->hasBattery = false;
}

E_SBFSPOT setDeviceData(InverterData *inv, LriDef lri, uint16_t cmd, Rec40S32 &data)
{
	E_SBFSPOT rc = E_OK;

	do
	{
        pcktID++;
		time_t now = time(NULL);
		writePacketHeader(pcktBuf, 0x01, inv->BTAddress);
        writePacket(pcktBuf, 0x12, 0xE0, 0x0100, inv->SUSyID, inv->Serial);
		writeShort(pcktBuf, 0x010E);
		writeShort(pcktBuf, cmd);
		writeLong(pcktBuf, 0x0A);
		writeLong(pcktBuf, lri | 0x02000001);
		writeLong(pcktBuf, now);
		writeLong(pcktBuf, data.MinLL());
		writeLong(pcktBuf, data.MaxLL());
		writeLong(pcktBuf, data.MinUL());
		writeLong(pcktBuf, data.MaxUL());
		writeLong(pcktBuf, data.MinActual());
		writeLong(pcktBuf, data.MaxActual());
		writeLong(pcktBuf, data.Res1());
		writeLong(pcktBuf, data.Res2());
		writePacketTrailer(pcktBuf);
		writePacketLength(pcktBuf);
	} while (!isCrcValid(pcktBuf[packetposition-3], pcktBuf[packetposition-2]));

    if (ConnType == CT_BLUETOOTH)
    {
        bthSend(pcktBuf);
    }
    else
    {
        ethSend(pcktBuf, inv->IPAddress);
    }

	return rc;
}

E_SBFSPOT getDeviceData(InverterData *inv, LriDef lri, uint16_t cmd, Rec40S32 &data)
{
	E_SBFSPOT rc = E_OK;

	const int recordsize = 40;
	do
	{
		pcktID++;
		writePacketHeader(pcktBuf, 0x01, inv->BTAddress);
		if (inv->SUSyID == SID_SB240)
			writePacket(pcktBuf, 0x09, 0xE0, 0, inv->SUSyID, inv->Serial);
		else
			writePacket(pcktBuf, 0x09, 0xA0, 0, inv->SUSyID, inv->Serial);
		writeShort(pcktBuf, 0x0200);
		writeShort(pcktBuf, cmd);
		writeLong(pcktBuf, lri);
		writeLong(pcktBuf, lri | 0xFF);
		writePacketTrailer(pcktBuf);
		writePacketLength(pcktBuf);
	}
	while (!isCrcValid(pcktBuf[packetposition-3], pcktBuf[packetposition-2]));

    if (ConnType == CT_BLUETOOTH)
    {
        bthSend(pcktBuf);
    }
    else
    {
        ethSend(pcktBuf, inv->IPAddress);
    }

	int validPcktID = 0;
    do
    {
        if (ConnType == CT_BLUETOOTH)
            rc = getPacket(addr_unknown, 1);
        else
            rc = ethGetPacket();

        if (rc != E_OK) return rc;

        if ((ConnType == CT_BLUETOOTH) && (!validateChecksum()))
            return E_CHKSUM;
        else
        {
            unsigned short rcvpcktID = get_short(pcktBuf+27) & 0x7FFF;
            if (pcktID == rcvpcktID)
            {
                uint32_t serial = get_long(pcktBuf + 17);
				if (serial == inv->Serial)
                {
					rc = E_NODATA;
                    validPcktID = 1;
                    for (int i = 41; i < packetposition - 3; i += recordsize)
                    {
						data.LRI((uint32_t)get_long(pcktBuf + i));
						data.DateTime((time_t)get_long(pcktBuf + i + 4));
						data.MinLL(get_long(pcktBuf + i + 8));
						data.MaxLL(get_long(pcktBuf + i + 12));
						data.MinUL(get_long(pcktBuf + i + 16));
						data.MaxUL(get_long(pcktBuf + i + 20));
						data.MinActual(get_long(pcktBuf + i + 24));
						data.MaxActual(get_long(pcktBuf + i + 28));
						data.Res1(get_long(pcktBuf + i + 32));
						data.Res2(get_long(pcktBuf + i + 36));
						rc = E_OK;
                    }
                }
				else if (DEBUG_HIGHEST) printf("Serial Nr mismatch. Expected %lu, received %d\n", inv->Serial, serial);
            }
            else if (DEBUG_HIGHEST) printf("Packet ID mismatch. Expected %d, received %d\n", pcktID, rcvpcktID);
        }
    }
    while (validPcktID == 0);

    return rc;
}

E_SBFSPOT getDeviceList(InverterData *devList[], int multigateID)
{
	E_SBFSPOT rc = E_OK;
	
	const int recordsize = 32;

	uint32_t devcount = 0;
	while ((devList[devcount] != NULL) && (devcount < MAX_INVERTERS))
		devcount++;

	if (devcount >= MAX_INVERTERS)
	{
		std::cerr << "ERROR: MAX_INVERTERS too low." << std::endl;
		return E_BUFOVRFLW;
	}

	do
	{
		pcktID++;
		writePacketHeader(pcktBuf, 0x01, NULL);
		writePacket(pcktBuf, 0x09, 0xE0, 0, devList[multigateID]->SUSyID, devList[multigateID]->Serial);
		writeShort(pcktBuf, 0x0200);
		writeShort(pcktBuf, 0xFFF5);
		writeLong(pcktBuf, 0);
		writeLong(pcktBuf, 0xFFFFFFFF);
		writePacketTrailer(pcktBuf);
		writePacketLength(pcktBuf);
	}
	while (!isCrcValid(pcktBuf[packetposition-3], pcktBuf[packetposition-2]));

	if (ethSend(pcktBuf, devList[multigateID]->IPAddress) == -1)	// SOCKET_ERROR
		return E_NODATA;

	int validPcktID = 0;
    do
    {
        rc = ethGetPacket();

        if (rc != E_OK) return rc;

		int16_t errorcode = get_short(pcktBuf + 23);
		if (errorcode != 0)
		{
			std::cerr << "Received errorcode=" << errorcode << std::endl;
			return (E_SBFSPOT)errorcode;
		}

		unsigned short rcvpcktID = get_short(pcktBuf+27) & 0x7FFF;
        if (pcktID == rcvpcktID)
        {
			//uint32_t lastrec = get_long(pcktBuf + 37);

			uint32_t serial = get_long(pcktBuf + 17);
			if (serial == devList[multigateID]->Serial)
            {
				rc = E_NODATA;
                validPcktID = 1;
                for (int i = 41; i < packetposition - 3; i += recordsize)
                {
					uint16_t devclass = get_short(pcktBuf + i + 4);
					if ((devclass == 3) && (devcount < (uint32_t)MAX_INVERTERS))
					{
						devList[devcount] = new InverterData;
						resetInverterData(devList[devcount]);
						devList[devcount]->SUSyID = get_short(pcktBuf + i + 6);
						devList[devcount]->Serial = get_long(pcktBuf + i + 8);
						strcpy(devList[devcount]->IPAddress, devList[multigateID]->IPAddress);
						devList[devcount]->multigateID = multigateID;
						if (++devcount >= MAX_INVERTERS)
						{
							std::cerr << "ERROR: MAX_INVERTERS too low." << std::endl;
							rc = E_BUFOVRFLW;
							break;
						}
						else
							rc = E_OK;
					}
                }
            }
			else if (DEBUG_HIGHEST) printf("Serial Nr mismatch. Expected %lu, received %d\n", devList[multigateID]->Serial, serial);
        }
        else if (DEBUG_HIGHEST) printf("Packet ID mismatch. Expected %d, received %d\n", pcktID, rcvpcktID);
    }
    while (validPcktID == 0);

    return rc;
}

E_SBFSPOT logoffMultigateDevices(InverterData *inverters[])
{
    if (DEBUG_NORMAL) puts("logoffMultigateDevices()");
	for (int mg=0; inverters[mg]!=NULL && mg<MAX_INVERTERS; mg++)
	{
		InverterData *pmg = inverters[mg];
		if (pmg->SUSyID == SID_MULTIGATE)
		{
			pmg->hasDayData = true;
			for (int sb240=0; inverters[sb240]!=NULL && sb240<MAX_INVERTERS; sb240++)
			{
				InverterData *psb = inverters[sb240];
				if ((psb->SUSyID == SID_SB240) && (psb->multigateID == mg))
				{		
					do
					{
						pcktID++;
						writePacketHeader(pcktBuf, 0, NULL);
						writePacket(pcktBuf, 0x08, 0xE0, 0x0300, psb->SUSyID, psb->Serial);
						writeLong(pcktBuf, 0xFFFD010E);
						writeLong(pcktBuf, 0xFFFFFFFF);
						writePacketTrailer(pcktBuf);
						writePacketLength(pcktBuf);
					}
					while (!isCrcValid(pcktBuf[packetposition-3], pcktBuf[packetposition-2]));

					ethSend(pcktBuf, psb->IPAddress);

					psb->logonStatus = 0; // logged of

					if (VERBOSE_NORMAL) std::cout << "Logoff " << psb->SUSyID << ":" << psb->Serial << std::endl;
				}
			}
		}
	}

    return E_OK;
}