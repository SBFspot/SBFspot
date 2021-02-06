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

#include "../bluetooth.h"
#include "../misc.h"
#include "../Config.h"
#include "../Defines.h"
#include "../Inverter.h"
#include "../SBFspot.h"
#include "../Timer.h"

#include <thread>

InverterData** m_inverters;

int logOn(Config& config)
{
    char msg[80];
    int rc = 0;

    int attempts = 1;
    do
    {
        if (attempts != 1) sleep(1);
        {
            printf("Connecting to %s (%d/%d)\n", config.BT_Address, attempts, config.BT_ConnectRetries);
            rc = bthConnect(config.BT_Address);
        }
        attempts++;
    }
    while ((attempts <= config.BT_ConnectRetries) && (rc != 0));

    if (rc != 0)
    {
        snprintf(msg, sizeof(msg), "bthConnect() returned %d\n", rc);
        print_error(stdout, PROC_CRITICAL, msg);
        return rc;
    }

    /*
    rc = bthInitConnection(config.BT_Address, m_inverters, config.MIS_Enabled);
    if (rc != E_OK)
    {
        print_error(stdout, PROC_CRITICAL, "Failed to initialize communication with inverter.\n");
        bthClose();
        return rc;
    }

    rc = bthGetSignalStrength(m_inverters[0]);
    if (VERBOSE_NORMAL) printf("BT Signal=%0.1f%%\n", m_inverters[0]->BT_Signal);

    if (logonSMAInverter(m_inverters, config.userGroup, config.SMA_Password) != E_OK)
    {
        snprintf(msg, sizeof(msg), "Logon failed. Check '%s' Password\n", config.userGroup == UG_USER? "USER":"INSTALLER");
        print_error(stdout, PROC_CRITICAL, msg);
        bthClose();
        return 1;
    }
    */

    return rc;
}

void logOff()
{
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

    bthSend(pcktBuf);
}

int main(int argc, char **argv)
{
    debug = 5;
    verbose = 5;

    Config config;
    //config.loop = true;
    strncpy(config.BT_Address, argv[1], sizeof(config.BT_Address) - 1);
    Timer timer(config);

    m_inverters = new InverterData*[MAX_INVERTERS];
    for (uint32_t i=0; i<MAX_INVERTERS; m_inverters[i++] = nullptr);

    do
    {
        auto timePoint = timer.nextTimePoint();
        std::this_thread::sleep_until(timePoint);

        logOn(config);
        logOff();
    }
    while(true);

    return 0;
}
