/************************************************************************************************
    SBFspot - Yet another tool to read power production of SMA solar inverters
    (c)2012-2025, SBF

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

#include "ArchData.h"
#include "CSVexport.h"
#include "mqtt.h"
#include <vector>
#include "mppt.h"

Inverter::Inverter(const Config& config)
    : m_config(config)
{
    //Allocate array to hold InverterData structs
    m_inverters = new InverterData*[MAX_INVERTERS];
    for (uint32_t i=0; i<MAX_INVERTERS; m_inverters[i++]=NULL);
}

Inverter::~Inverter()
{
    delete[] m_inverters;
}

int Inverter::process()
{
    char msg[80];
    int rc = logOn();
    if (rc != 0)
    {
        logOff();
        return rc;
    }

    if (VERBOSE_NORMAL) puts("Logon OK");

    // If SBFspot is executed with -settime argument
    if (m_config.settime || m_config.settime2)
    {
        if (m_config.settime)
            rc = SetPlantTime_V2(0, 0, 0);
        else if (m_config.settime2)
            rc = SetPlantTime_V1();
        
        logoffSMAInverter(m_inverters[0]);
        logOff();
        bthClose(); // Close socket

        return rc;
    }

    // Synchronize plant time with system time
    // Only BT connected devices and if enabled in config _or_ requested by 123Solar
    // Most probably Speedwire devices get their time from the local IP network
    if ((m_config.ConnectionType == CT_BLUETOOTH) && (m_config.synchTime > 0 || m_config.s123 == S123_SYNC ))
        if ((rc = SetPlantTime_V2(m_config.synchTime, m_config.synchTimeLow, m_config.synchTimeHigh)) != E_OK)
            std::cout << "SetPlantTime returned an error: " << rc << std::endl;

    //if ((rc = getInverterData(m_inverters, sbftest)) != E_OK)
    //    std::cout << "getInverterData(sbftest) returned an error: " << rc << std::endl;

    if ((rc = getInverterData(m_inverters, SoftwareVersion)) != E_OK)
        std::cout << "getSoftwareVersion returned an error: " << rc << std::endl;

    if ((rc = getInverterData(m_inverters, TypeLabel)) != E_OK)
        std::cout << "getTypeLabel returned an error: " << rc << std::endl;
    else
    {
        for (uint32_t inv=0; m_inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
        {
            m_inverters[inv]->hasBattery = (m_inverters[inv]->DevClass == BatteryInverter) ||
                (m_inverters[inv]->DevClass == HybridInverter) ||
                (m_inverters[inv]->SUSyID == 292);   //SB 3600-SE (Smart Energy)

            if (m_inverters[inv]->hasBattery)
                hasBatteryDevice = true;

            if (VERBOSE_NORMAL)
            {
                printf("SUSyID: %d - SN: %lu\n", m_inverters[inv]->SUSyID, m_inverters[inv]->Serial);
                printf("Device Name:      %s\n", m_inverters[inv]->DeviceName.c_str());
                printf("Device Class:     %s%s\n", m_inverters[inv]->DeviceClass.c_str(), (m_inverters[inv]->SUSyID == 292) ? " (with battery)":"");
                printf("Device Type:      %s\n", m_inverters[inv]->DeviceType.c_str());
                printf("Software Version: %s\n", m_inverters[inv]->SWVersion.c_str());
            }
        }
    }

    // Check for Multigate and get connected devices
    for (uint32_t inv=0; m_inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
    {
        if ((m_inverters[inv]->DevClass == CommunicationProduct) && (m_inverters[inv]->SUSyID == SID_MULTIGATE))
        {
            if (VERBOSE_HIGH)
                std::cout << "Multigate found. Looking for connected devices..." << std::endl;

            // multigate has its own ID
            m_inverters[inv]->multigateID = inv;

            if ((rc = getDeviceList(m_inverters, inv)) != 0)
                std::cout << "getDeviceList returned an error: " << rc << std::endl;
            else
            {
                if (VERBOSE_HIGH)
                {
                    std::cout << "Found these devices:" << std::endl;
                    for (uint32_t ii=0; m_inverters[ii]!=NULL && ii<MAX_INVERTERS; ii++)
                    {
                        std::cout << "ID:" << ii << " S/N:" << m_inverters[ii]->SUSyID << "-" << m_inverters[ii]->Serial << " IP:" << m_inverters[ii]->IPAddress << std::endl;
                    }
                }

                if (logonSMAInverter(m_inverters, m_config.userGroup, m_config.SMA_Password) != E_OK)
                {
                    snprintf(msg, sizeof(msg), "Logon failed. Check '%s' Password\n", m_config.userGroup == UG_USER? "USER":"INSTALLER");
                    print_error(stdout, PROC_CRITICAL, msg);
                    logOff();
                    ethClose();
                    return 1;
                }

                if ((rc = getInverterData(m_inverters, SoftwareVersion)) != E_OK)
                    printf("getSoftwareVersion returned an error: %d\n", rc);

                if ((rc = getInverterData(m_inverters, TypeLabel)) != E_OK)
                    printf("getTypeLabel returned an error: %d\n", rc);
                else
                {
                    for (uint32_t ii=0; m_inverters[ii]!=NULL && ii<MAX_INVERTERS; ii++)
                    {
                        if (VERBOSE_NORMAL)
                        {
                            printf("SUSyID: %d - SN: %lu\n", m_inverters[ii]->SUSyID, m_inverters[ii]->Serial);
                            printf("Device Name:      %s\n", m_inverters[ii]->DeviceName.c_str());
                            printf("Device Class:     %s\n", m_inverters[ii]->DeviceClass.c_str());
                            printf("Device Type:      %s\n", m_inverters[ii]->DeviceType.c_str());
                            printf("Software Version: %s\n", m_inverters[ii]->SWVersion.c_str());
                        }
                    }
                }
            }
        }
    }

    if (hasBatteryDevice)
    {
        if ((rc = getInverterData(m_inverters, BatteryChargeStatus)) != E_OK)
            std::cout << "getBatteryChargeStatus returned an error: " << rc << std::endl;
        else
        {
            for (uint32_t inv = 0; m_inverters[inv] != NULL && inv < MAX_INVERTERS; inv++)
            {
                if (m_inverters[inv]->hasBattery)
                {
                    if (VERBOSE_NORMAL)
                    {
                        printf("SUSyID: %d - SN: %lu\n", m_inverters[inv]->SUSyID, m_inverters[inv]->Serial);
                        printf("Batt. Charging Status: %lu%%\n", m_inverters[inv]->BatChaStt);
                    }
                }
            }
        }

        if ((rc = getInverterData(m_inverters, BatteryInfo)) != E_OK)
            std::cout << "getBatteryInfo returned an error: " << rc << std::endl;
        else
        {
            for (uint32_t inv = 0; m_inverters[inv] != NULL && inv < MAX_INVERTERS; inv++)
            {
                if (m_inverters[inv]->hasBattery)
                {
                    if (VERBOSE_NORMAL)
                    {
                        printf("SUSyID: %d - SN: %lu\n", m_inverters[inv]->SUSyID, m_inverters[inv]->Serial);
                        printf("Batt. Temperature: %3.1f%s\n", (float)(m_inverters[inv]->BatTmpVal / 10), tagdefs.getDesc(tagdefs.TAG_DEG_C).c_str());
                        printf("Batt. Voltage    : %3.2fV\n", toVolt(m_inverters[inv]->BatVol));
                        printf("Batt. Current    : %2.3fA\n", toAmp(m_inverters[inv]->BatAmp));
                    }
                }
            }
        }
    }

    if ((rc = getInverterData(m_inverters, MeteringGridMsTotW)) < E_OK)
        std::cout << "getMeteringGridInfo returned an error: " << rc << std::endl;
    else
    {
        if (rc == E_OK)
        {
            for (uint32_t inv = 0; m_inverters[inv] != NULL && inv < MAX_INVERTERS; inv++)
            {
                if (VERBOSE_NORMAL)
                {
                    printf("SUSyID: %d - SN: %lu\n", m_inverters[inv]->SUSyID, m_inverters[inv]->Serial);
                    printf("Grid Power Out : %dW\n", m_inverters[inv]->MeteringGridMsTotWOut);
                    printf("Grid Power In  : %dW\n", m_inverters[inv]->MeteringGridMsTotWIn);
                }
            }
        }
    }

    if ((rc = getInverterData(m_inverters, DeviceStatus)) != E_OK)
        std::cout << "getDeviceStatus returned an error: " << rc << std::endl;
    else
    {
        for (uint32_t inv=0; m_inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
        {
            if (VERBOSE_NORMAL)
            {
                printf("SUSyID: %d - SN: %lu\n", m_inverters[inv]->SUSyID, m_inverters[inv]->Serial);
                printf("Device Status:      %s\n", tagdefs.getDesc(m_inverters[inv]->DeviceStatus, "?").c_str());
            }
        }
    }

    rc = getInverterData(m_inverters, InverterTemperature);
    if ((rc != E_OK) && (rc != E_LRINOTAVAIL))
        std::cout << "getInverterTemperature returned an error: " << rc << std::endl;
    else
    {
        for (uint32_t inv=0; m_inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
        {
            if (VERBOSE_NORMAL)
            {
                printf("SUSyID: %d - SN: %lu\n", m_inverters[inv]->SUSyID, m_inverters[inv]->Serial);
                printf("Device Temperature: ");
                if (is_NaN(m_inverters[inv]->Temperature))
                    printf("%s\n", tagdefs.getDesc(tagdefs.TAG_NaNStt).c_str());
                else
                    printf("%3.1f%s\n", ((float)m_inverters[inv]->Temperature / 100), tagdefs.getDesc(tagdefs.TAG_DEG_C).c_str());
            }
        }
    }

    if (m_inverters[0]->DevClass == SolarInverter)
    {
        if ((rc = getInverterData(m_inverters, GridRelayStatus)) != E_OK)
            std::cout << "getGridRelayStatus returned an error: " << rc << std::endl;
        else
        {
            for (uint32_t inv=0; m_inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
            {
                if (m_inverters[inv]->DevClass == SolarInverter)
                {
                    if (VERBOSE_NORMAL)
                    {
                        printf("SUSyID: %d - SN: %lu\n", m_inverters[inv]->SUSyID, m_inverters[inv]->Serial);
                        printf("GridRelay Status:      %s\n", tagdefs.getDesc(m_inverters[inv]->GridRelayStatus, "?").c_str());
                    }
                }
            }
        }
    }

    if ((rc = getInverterData(m_inverters, EnergyProduction)) != E_OK)
    {
        std::cout << "getEnergyProduction returned an error: " << rc << std::endl;
    }

    // Issue #290 Etoday and temperature are shown as ZERO from STP6.0 inverter
    // Flag to indicate whether archdata has been loaded (for all inverters)
    bool archdata_available = false;

    for (uint32_t inv = 0; m_inverters[inv] != NULL && inv < MAX_INVERTERS; inv++)
    {
        if (m_inverters[inv]->EToday == 0 && m_inverters[inv]->ETotal != 0)
        {
            if (!archdata_available)
            {
                time_t arch_time = time(nullptr);

                if ((rc = ArchiveDayData(m_inverters, arch_time)) == E_OK)
                    archdata_available = true;
                else if (rc != E_ARCHNODATA)
                    std::cout << "ArchiveDayData returned an error: " << rc << std::endl;
            }

            if (archdata_available && m_inverters[inv]->dayData[0].totalWh != 0) // Fix #459
            {
                // EToday = Current ETotal - StartOfDay ETotal
                m_inverters[inv]->EToday = m_inverters[inv]->ETotal - m_inverters[inv]->dayData[0].totalWh;
                if (VERBOSE_NORMAL)
                {
                    printf("SUSyID: %d - SN: %lu\n", m_inverters[inv]->SUSyID, m_inverters[inv]->Serial);
                    printf("Calculated EToday: %.3fkWh\n", tokWh(m_inverters[inv]->EToday));
                }
            }
        }
    }

    if ((rc = getInverterData(m_inverters, OperationTime)) != E_OK)
        std::cout << "getOperationTime returned an error: " << rc << std::endl;
    else
    {
        for (uint32_t inv=0; m_inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
        {
            if (VERBOSE_NORMAL)
            {
                printf("SUSyID: %d - SN: %lu\n", m_inverters[inv]->SUSyID, m_inverters[inv]->Serial);
                puts("Energy Production:");
                printf("\tEToday: %.3fkWh\n", tokWh(m_inverters[inv]->EToday));
                printf("\tETotal: %.3fkWh\n", tokWh(m_inverters[inv]->ETotal));
                printf("\tOperation Time: %.2fh\n", toHour(m_inverters[inv]->OperationTime));
                printf("\tFeed-In Time  : %.2fh\n", toHour(m_inverters[inv]->FeedInTime));
            }
        }
    }

    if ((rc = getInverterData(m_inverters, SpotDCPower)) != E_OK)
        std::cout << "getSpotDCPower returned an error: " << rc << std::endl;

    if ((rc = getInverterData(m_inverters, SpotDCVoltage)) != E_OK)
        std::cout << "getSpotDCVoltage returned an error: " << rc << std::endl;

    if ((rc = getInverterData(m_inverters, SpotACPower)) != E_OK)
        std::cout << "getSpotACPower returned an error: " << rc << std::endl;

    if ((rc = getInverterData(m_inverters, SpotACVoltage)) != E_OK)
        std::cout << "getSpotACVoltage returned an error: " << rc << std::endl;

    if ((rc = getInverterData(m_inverters, SpotACTotalPower)) != E_OK)
        std::cout << "getSpotACTotalPower returned an error: " << rc << std::endl;

    for (uint32_t inv = 0; m_inverters[inv] != NULL && inv<MAX_INVERTERS; inv++)
    {
        //Calculate missing AC/DC Spot Values
        if (m_config.calcMissingSpot)
            CalcMissingSpot(m_inverters[inv]);

        //m_inverters[inv]->calPdcTot = m_inverters[inv]->Pdc1 + m_inverters[inv]->Pdc2;
        if (VERBOSE_NORMAL)
        {
            printf("SUSyID: %d - SN: %lu\n", m_inverters[inv]->SUSyID, m_inverters[inv]->Serial);
            puts("DC Spot Data:");

            for (const auto &mpp : m_inverters[inv]->mpp)
            {
                printf("\tMPPT %d Pdc: %7.3fkW - Udc: %6.2fV - Idc: %6.3fA\n", mpp.first, mpp.second.kW(), mpp.second.Volt(), mpp.second.Amp());
            }
            printf("\tCalculated Total Pdc: %7.3fkW\n", tokW(m_inverters[inv]->calPdcTot));
        }

        m_inverters[inv]->calPacTot = m_inverters[inv]->Pac1 + m_inverters[inv]->Pac2 + m_inverters[inv]->Pac3;
        //Calculated Inverter Efficiency
        m_inverters[inv]->calEfficiency = m_inverters[inv]->calPdcTot == 0 ? 0.0f : 100.0f * (float)m_inverters[inv]->calPacTot / (float)m_inverters[inv]->calPdcTot;

        if (VERBOSE_NORMAL)
        {
            puts("AC Spot Data:");
            printf("\tPhase 1 Pac : %7.3fkW - Uac: %6.2fV - Iac: %6.3fA\n", tokW(m_inverters[inv]->Pac1), toVolt(m_inverters[inv]->Uac1), toAmp(m_inverters[inv]->Iac1));
            printf("\tPhase 2 Pac : %7.3fkW - Uac: %6.2fV - Iac: %6.3fA\n", tokW(m_inverters[inv]->Pac2), toVolt(m_inverters[inv]->Uac2), toAmp(m_inverters[inv]->Iac2));
            printf("\tPhase 3 Pac : %7.3fkW - Uac: %6.2fV - Iac: %6.3fA\n", tokW(m_inverters[inv]->Pac3), toVolt(m_inverters[inv]->Uac3), toAmp(m_inverters[inv]->Iac3));
            printf("\tTotal Pac   : %7.3fkW - Calculated Pac: %7.3fkW\n", tokW(m_inverters[inv]->TotalPac), tokW(m_inverters[inv]->calPacTot));
            printf("\tEfficiency  : %7.2f%%\n", m_inverters[inv]->calEfficiency);
        }
    }

    if ((rc = getInverterData(m_inverters, SpotGridFrequency)) != E_OK)
        std::cout << "getSpotGridFrequency returned an error: " << rc << std::endl;
    else
    {
        for (uint32_t inv = 0; m_inverters[inv] != NULL && inv<MAX_INVERTERS; inv++)
        {
            if (VERBOSE_NORMAL)
            {
                printf("SUSyID: %d - SN: %lu\n", m_inverters[inv]->SUSyID, m_inverters[inv]->Serial);
                printf("Grid Freq. : %.2fHz\n", toHz(m_inverters[inv]->GridFreq));
            }
        }
    }

    if (m_inverters[0]->DevClass == SolarInverter)
    {
        for (uint32_t inv=0; m_inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
        {
            if (VERBOSE_NORMAL)
            {
                printf("SUSyID: %d - SN: %lu\n", m_inverters[inv]->SUSyID, m_inverters[inv]->Serial);
                if (m_inverters[inv]->InverterDatetime != 0)
                    std::cout << "Current Inverter Time: " << strftime_t(m_config.DateTimeFormat, m_inverters[inv]->InverterDatetime) << std::endl;

                if (m_inverters[inv]->WakeupTime != 0)
                    std::cout << "Inverter Wake-Up Time: " << strftime_t(m_config.DateTimeFormat, m_inverters[inv]->WakeupTime) << std::endl;

                if (m_inverters[inv]->SleepTime != 0)
                    std::cout << "Inverter Sleep Time  : " << strftime_t(m_config.DateTimeFormat, m_inverters[inv]->SleepTime) << std::endl;
            }
        }
    }

    // Open DB
#if defined(USE_SQLITE) || defined(USE_MYSQL)
    if (!m_config.nosql)
    {
#if defined(USE_MYSQL)
        m_db.open(m_config.sqlHostname, m_config.sqlUsername, m_config.sqlUserPassword, m_config.sqlDatabase, m_config.sqlPort);
#elif defined(USE_SQLITE)
        m_db.open(m_config.sqlDatabase);
#endif
    }
#endif

    exportSpotData();

    //SolarInverter -> Continue to get archive data
    unsigned int idx;

    /***************
    * Get Day Data *
    ****************/
    time_t arch_time = (0 == m_config.startdate) ? time(nullptr) : m_config.startdate;

    for (int count=0; count<m_config.archDays; count++)
    {
        if ((rc = ArchiveDayData(m_inverters, arch_time)) != E_OK)
        {
            if (rc != E_ARCHNODATA)
                std::cout << "ArchiveDayData returned an error: " << rc << std::endl;
        }
        else
        {
            if (VERBOSE_HIGH)
            {
                for (uint32_t inv=0; m_inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
                {
                    printf("SUSyID: %d - SN: %lu\n", m_inverters[inv]->SUSyID, m_inverters[inv]->Serial);
                    for (idx=0; idx<sizeof(m_inverters[inv]->dayData)/sizeof(DayData); idx++)
                        if (m_inverters[inv]->dayData[idx].datetime != 0)
                        {
                            printf("%s : %.3fkWh - %3.3fW\n", strftime_t(m_config.DateTimeFormat, m_inverters[inv]->dayData[idx].datetime).c_str(), (double)m_inverters[inv]->dayData[idx].totalWh/1000, (double)m_inverters[inv]->dayData[idx].watt);
                            fflush(stdout);
                        }
                    puts("======");
                }
            }

            exportDayData();
        }

        //Goto previous day
        arch_time -= 86400;
    }

    /*****************
    * Get Month Data *
    ******************/
    if (m_config.archMonths > 0)
    {
        getMonthDataOffset(m_inverters); //Issues 115/130
        arch_time = (0 == m_config.startdate) ? time(nullptr) : m_config.startdate;
        struct tm arch_tm;
        memcpy(&arch_tm, gmtime(&arch_time), sizeof(arch_tm));

        for (int count=0; count<m_config.archMonths; count++)
        {
            ArchiveMonthData(m_inverters, &arch_tm);

            if (VERBOSE_HIGH)
            {
                for (uint32_t inv=0; m_inverters[inv] != NULL && inv<MAX_INVERTERS; inv++)
                {
                    printf("SUSyID: %d - SN: %lu\n", m_inverters[inv]->SUSyID, m_inverters[inv]->Serial);
                    for (unsigned int ii = 0; ii < sizeof(m_inverters[inv]->monthData) / sizeof(MonthData); ii++)
                        if (m_inverters[inv]->monthData[ii].datetime != 0)
                            printf("%s : %.3fkWh - %3.3fkWh\n", strfgmtime_t(m_config.DateFormat, m_inverters[inv]->monthData[ii].datetime).c_str(), (double)m_inverters[inv]->monthData[ii].totalWh / 1000, (double)m_inverters[inv]->monthData[ii].dayWh / 1000);
                    puts("======");
                }
            }

            exportMonthData();

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
    boost::posix_time::ptime tm_utc(boost::posix_time::from_time_t((0 == m_config.startdate) ? time(nullptr) : m_config.startdate));
    //ptime tm_utc(posix_time::second_clock::universal_time());
    boost::gregorian::date dt_utc(tm_utc.date().year(), tm_utc.date().month(), 1);
    std::string dt_range_csv = str(boost::format("%d%02d") % dt_utc.year() % static_cast<short>(dt_utc.month()));

    for (int m = 0; m < m_config.archEventMonths; m++)
    {
        if (VERBOSE_LOW)
            std::cout << "Reading events: " << to_simple_string(dt_utc) << std::endl;
        //Get user level events
        rc = ArchiveEventData(m_inverters, dt_utc, UG_USER);
        if (rc == E_EOF) break; // No more data (first event reached)
        else if (rc != E_OK) std::cout << "ArchiveEventData(user) returned an error: " << rc << std::endl;

        //When logged in as installer, get installer level events
        if (m_config.userGroup == UG_INSTALLER)
        {
            rc = ArchiveEventData(m_inverters, dt_utc, UG_INSTALLER);
            if (rc == E_EOF) break; // No more data (first event reached)
            else if (rc != E_OK) std::cout << "ArchiveEventData(installer) returned an error: " << rc << std::endl;
        }

        //Move to previous month
        if (dt_utc.month() == 1)
            dt_utc = boost::gregorian::date(dt_utc.year() - 1, 12, 1);
        else
            dt_utc = boost::gregorian::date(dt_utc.year(), dt_utc.month() - 1, 1);

    }

    if (rc == E_OK)
    {
        //Adjust start of range with 1 month
        if (dt_utc.month() == 12)
            dt_utc = boost::gregorian::date(dt_utc.year() + 1, 1, 1);
        else
            dt_utc = boost::gregorian::date(dt_utc.year(), dt_utc.month() + 1, 1);
    }

    if ((rc == E_OK) || (rc == E_EOF))
    {
        if (VERBOSE_HIGH)
        {
            for (uint32_t inv = 0; m_inverters[inv] != NULL && inv < MAX_INVERTERS; inv++)
            {
                if (m_inverters[inv]->eventData.size() > 0)
                {
                    std::cout << "Events for device " << m_inverters[inv]->SUSyID << ":" << m_inverters[inv]->Serial << '\n';

                    // Print events in reverse order (Highest EntryID first)
                    for (auto it = m_inverters[inv]->eventData.rbegin(); it != m_inverters[inv]->eventData.rend(); ++it)
                    {
                        std::cout << it->ToString(m_config.DateTimeFormat) << '\n';
                    }
                }
            }

            std::cout.flush();
        }

        dt_range_csv = str(boost::format("%d%02d-%s") % dt_utc.year() % static_cast<short>(dt_utc.month()) % dt_range_csv);
        exportEventData(dt_range_csv);
    }

    if (m_config.ConnectionType == CT_BLUETOOTH)
        logoffSMAInverter(m_inverters[0]);
    else
    {
        logoffMultigateDevices(m_inverters);
        for (uint32_t inv=0; m_inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
            logoffSMAInverter(m_inverters[inv]);
    }

    logOff();
    bthClose();

#if defined(USE_SQLITE) || defined(USE_MYSQL)
    if ((!m_config.nosql) && m_db.isopen())
        m_db.close();
#endif

    return rc;
}

int Inverter::logOn()
{
    char msg[80];
    int rc = 0;

    AppSerial = genSessionID();

    if (m_config.ConnectionType == CT_BLUETOOTH)
    {
        int attempts = 1;
        do
        {
            if (attempts != 1) sleep(1);
            {
                if (VERBOSE_NORMAL) printf("Connecting to %s (%d/%d)\n", m_config.BT_Address, attempts, m_config.BT_ConnectRetries);
                rc = bthConnect(m_config.BT_Address, m_config.Local_BT_Address);
            }
            attempts++;
        } while ((attempts <= m_config.BT_ConnectRetries) && (rc != 0));


        if (rc != 0)
        {
            snprintf(msg, sizeof(msg), "bthConnect() returned %d\n", rc);
            print_error(stdout, PROC_CRITICAL, msg);
            return rc;
        }

        rc = initialiseSMAConnection(m_config.BT_Address, m_inverters, m_config.MIS_Enabled);

        if (rc != E_OK)
        {
            if (rc == E_FWVERSION)
                print_error(stdout, PROC_CRITICAL, "Incompatible FW version detected.\n");
            else
                print_error(stdout, PROC_CRITICAL, "Failed to initialise communication with inverter.\n");

            bthClose();
            return rc;
        }

        rc = getBT_SignalStrength(m_inverters[0]);
        if (VERBOSE_NORMAL) printf("BT Signal=%0.1f%%\n", m_inverters[0]->BT_Signal);

    }
    else if (m_config.ConnectionType == CT_ETHERNET)
    {
        if (VERBOSE_NORMAL) printf("Connecting to Local Network...\n");
        rc = ethConnect(m_config.IP_Port);
        if (rc != 0)
        {
            print_error(stdout, PROC_CRITICAL, "Failed to set up socket connection.\n");
            return rc;
        }

        rc = ethInitConnection(m_inverters, m_config.ip_addresslist);
        if (rc != E_OK)
        {
            print_error(stdout, PROC_CRITICAL, "Failed to initialise Speedwire connection.\n");
            ethClose();
            return rc;
        }
    }
    else // CT_NONE -> Quit
    {
        print_error(stdout, PROC_CRITICAL, "Connection type unknown.\n");
        return E_BADARG;
    }

    rc = logonSMAInverter(m_inverters, m_config.userGroup, m_config.SMA_Password);
    if (rc != E_OK)
    {
        if (rc == E_INVPASSW)
            snprintf(msg, sizeof(msg), "Logon failed. Check '%s' Password\n", m_config.userGroup == UG_USER? "USER":"INSTALLER");
        else
            snprintf(msg, sizeof(msg), "Logon failed. Reason unknown (%d)\n", rc);

        print_error(stdout, PROC_CRITICAL, msg);
        bthClose();
        return 1;
    }

    /*************************************************
     * At this point we are logged on to the inverter
     *************************************************/

    return rc;
}

void Inverter::logOff()
{
    freemem(m_inverters);
}

void Inverter::exportSpotData()
{
    if ((m_config.CSV_Export) && (!m_config.nospot))
        ExportSpotDataToCSV(&m_config, m_inverters);

    // Undocumented - For 123Solar Web Solar logger usage only)
    // Currently, only data of first inverter is exported
    if ((m_inverters[0]->DevClass == SolarInverter) || (m_inverters[0]->DevClass == BatteryInverter) || (m_inverters[0]->DevClass == HybridInverter))
    {
        if (m_config.s123 == S123_DATA)
            ExportSpotDataTo123s(&m_config, m_inverters);
        if (m_config.s123 == S123_INFO)
            ExportInformationDataTo123s(&m_config, m_inverters);
        if (m_config.s123 == S123_STATE)
            ExportStateDataTo123s(&m_config, m_inverters);
    }

    if (hasBatteryDevice && (m_config.CSV_Export) && (!m_config.nospot))
        ExportBatteryDataToCSV(&m_config, m_inverters);

#if defined(USE_SQLITE) || defined(USE_MYSQL)
    if (!m_config.nosql && m_db.isopen())
    {
        time_t spottime = time(nullptr);
        m_db.type_label(m_inverters);
        m_db.device_status(m_inverters, spottime);
        m_db.exportSpotData(m_inverters, spottime);
        if (hasBatteryDevice)
            m_db.exportBatteryData(m_inverters, spottime);
    }
#endif

    /*******
    * MQTT *
    ********/
    if (m_config.mqtt) // MQTT enabled
    {
        MqttExport mqtt(m_config);
        auto rc = mqtt.exportInverterData(toStdVector(m_inverters));
        if (rc != 0)
        {
            std::cout << "Error " << rc << " while publishing to MQTT Broker" << std::endl;
        }
    }
}

void Inverter::exportDayData()
{
    if (m_config.CSV_Export)
        ExportDayDataToCSV(&m_config, m_inverters);

#if defined(USE_SQLITE) || defined(USE_MYSQL)
    if ((!m_config.nosql) && m_db.isopen())
        m_db.exportDayData(m_inverters);
#endif
}

void Inverter::exportMonthData()
{
    if (m_config.CSV_Export)
        ExportMonthDataToCSV(&m_config, m_inverters);

#if defined(USE_SQLITE) || defined(USE_MYSQL)
    if ((!m_config.nosql) && m_db.isopen())
        m_db.exportMonthData(m_inverters);
#endif
}

void Inverter::exportEventData(const std::string& dt_range_csv)
{
    if ((m_config.CSV_Export) && (m_config.archEventMonths > 0))
        ExportEventsToCSV(&m_config, m_inverters, dt_range_csv);

#if defined(USE_SQLITE) || defined(USE_MYSQL)
    if ((!m_config.nosql) && m_db.isopen())
        m_db.exportEventData(m_inverters, tagdefs);
#endif
}

std::vector<InverterData> Inverter::toStdVector(InverterData* const* const inverters)
{
    std::vector<InverterData> inverterData;
    inverterData.reserve(MAX_INVERTERS);

    for (uint32_t inv = 0; inverters[inv] != NULL && inv < MAX_INVERTERS; inv++)
        inverterData.push_back(*inverters[inv]);

    return inverterData;
}

