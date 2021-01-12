/************************************************************************************************
    SBFspot - Yet another tool to read power production of SMA® solar inverters
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

#include "ArchData.h"
#include "CSVexport.h"
#include "SBFspot.h"
#include "SQLselect.h"
#include "mqtt.h"
#include "sunrise_sunset.h"

const uint32_t MAX_INVERTERS = 20;

using namespace boost;

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
    for (uint32_t i=0; i<MAX_INVERTERS; Inverters[i++]=NULL);

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
        for (uint32_t inv=0; Inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
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
    for (uint32_t inv=0; Inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
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
                    for (uint32_t ii=0; Inverters[ii]!=NULL && ii<MAX_INVERTERS; ii++)
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
                    for (uint32_t ii=0; Inverters[ii]!=NULL && ii<MAX_INVERTERS; ii++)
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
            for (uint32_t inv=0; Inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
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
            for (uint32_t inv=0; Inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
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
            for (uint32_t inv=0; Inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
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
        for (uint32_t inv=0; Inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
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
        for (uint32_t inv=0; Inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
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
            for (uint32_t inv=0; Inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
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
            for (uint32_t inv=0; Inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
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
        for (uint32_t inv=0; Inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
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

    for (uint32_t inv=0; Inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
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

    for (uint32_t inv=0; Inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
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
        for (uint32_t inv=0; Inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
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
        for (uint32_t inv=0; Inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
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
    db_SQL_Export db;
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
                for (uint32_t inv=0; Inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
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
                for (uint32_t inv = 0; Inverters[inv] != NULL && inv<MAX_INVERTERS; inv++)
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
        if (VERBOSE_LOW)
            std::cout << "Reading events: " << to_simple_string(dt_utc) << std::endl;
        //Get user level events
        rc = ArchiveEventData(Inverters, dt_utc, UG_USER);
        if (rc == E_EOF) break; // No more data (first event reached)
        else if (rc != E_OK) std::cerr << "ArchiveEventData(user) returned an error: " << rc << std::endl;

        //When logged in as installer, get installer level events
        if (cfg.userGroup == UG_INSTALLER)
        {
            rc = ArchiveEventData(Inverters, dt_utc, UG_INSTALLER);
            if (rc == E_EOF) break; // No more data (first event reached)
            else if (rc != E_OK) std::cerr << "ArchiveEventData(installer) returned an error: " << rc << std::endl;
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
        for (uint32_t inv=0; Inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
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
