/************************************************************************************************
                               ____  ____  _____                _
                              / ___|| __ )|  ___|__ _ __   ___ | |_
                              \___ \|  _ \| |_ / __| '_ \ / _ \| __|
                               ___) | |_) |  _|\__ \ |_) | (_) | |_
                              |____/|____/|_|  |___/ .__/ \___/ \__|
                                                   |_|

    SBFspot - Yet another tool to read power production of SMA solar/battery inverters
    (c)2012-2022, SBF

    Latest version can be found at https://github.com/SBFspot/SBFspot

    Special Thanks to:
    S. Pittaway: Author of "NANODE SMA PV MONITOR" on which this project is based.
    W. Simons  : Early adopter, main tester and SMAdata2 Protocol analyzer
    G. Schnuff : SMAdata2 Protocol analyzer
    T. Frank   : Speedwire support
    Snowmiss   : User manual
    All other users for their contribution to the success of this project

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
#include "mppt.h"

int MAX_CommBuf = 0;
int MAX_pcktBuf = 0;

//Public vars
int debug = 0;
int verbose = 0;
bool quiet = false;
char DateTimeFormat[32];
char DateFormat[32];
CONNECTIONTYPE ConnType = CT_NONE;
TagDefs tagdefs = TagDefs();
bool hasBatteryDevice = false; // Plant has 1 or more battery device(s)

//Free memory allocated by initialiseSMAConnection()
void freemem(InverterData *inverters[])
{
    for (uint32_t i=0; i<MAX_INVERTERS; i++)
        if (inverters[i] != NULL)
        {
            delete inverters[i];
            inverters[i] = NULL;
        }
}

E_SBFSPOT getPacket(uint8_t senderaddr[6], int wait4Command)
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

            if (DEBUG_HIGHEST) HexDump(CommBuf, bib, 10);

            //Check if data is coming from the right inverter
            if (isValidSender(senderaddr, pkHdr->SourceAddr))
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
            if (DEBUG_HIGHEST) HexDump(CommBuf, bib, 10);
            //Check if data is coming from the right inverter
            if (isValidSender(senderaddr, pkHdr->SourceAddr))
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

    if ((rc == E_OK) && (DEBUG_HIGHEST))
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

int getInverterIndexByAddress(InverterData* const inverters[], uint8_t bt_addr[6])
{
    for (uint32_t inv=0; inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
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

    return -1; //No inverter found
}

E_SBFSPOT ethGetPacket(void)
{
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
                if (DEBUG_HIGHEST) HexDump(CommBuf, bib, 10);
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

                    if (DEBUG_HIGHEST)
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

E_SBFSPOT ethInitConnection(InverterData *inverters[], std::vector<std::string> IPaddresslist)
{
    if (VERBOSE_NORMAL)
    {
        puts("Initialising...");
        printf("SUSyID: %d - SessionID: %lu\n", AppSUSyID, AppSerial);
    }

    E_SBFSPOT rc = E_OK;

    uint32_t devcount = 0;

    if ((IPaddresslist.size() == 1) && (IPaddresslist.front() == "0.0.0.0"))
    {
        // Start with UDP multicast to check for SMA devices on the LAN
        // SMA devices announce their presence in response to the discovery request packet
        writeLong(pcktBuf, 0x00414D53);  //Start of SMA header
        writeLong(pcktBuf, 0xA0020400);  //Unknown
        writeLong(pcktBuf, 0xFFFFFFFF);  //Unknown
        writeLong(pcktBuf, 0x20000000);  //Unknown
        writeLong(pcktBuf, 0x00000000);  //Unknown

        ethSend(pcktBuf, IP_Multicast);

        int bytesRead = 0;
        while ((bytesRead = ethRead(CommBuf, sizeof(CommBuf))) > 0)
        {
            if (DEBUG_HIGHEST) HexDump(CommBuf, bytesRead, 10);

            if (memcmp(CommBuf, "SMA", 3) == 0)
            {
                inverters[devcount] = new InverterData;
                resetInverterData(inverters[devcount]);

                // Store received IP address as readable text into InverterData struct
                // IP address is found at pos 38 in the buffer
                sprintf(inverters[devcount]->IPAddress, "%d.%d.%d.%d", CommBuf[38], CommBuf[39], CommBuf[40], CommBuf[41]);
                if (VERBOSE_NORMAL) printf("Valid response from SMA device %s\n", inverters[devcount]->IPAddress);

                if (++devcount >= MAX_INVERTERS)
                    break;
            }
        }

        if (devcount == 0)
        {
            std::cout << "ERROR: No devices responded to discovery query.\n";
            std::cout << "Try to set IP_Address in config.\n";
            return E_INIT;
        }
    }
    else
    {
        for (const auto &ip : IPaddresslist)
        {
            inverters[devcount] = new InverterData;
            resetInverterData(inverters[devcount]);
            memccpy(inverters[devcount]->IPAddress, ip.c_str(), 0, sizeof(inverters[devcount]->IPAddress));
            if (VERBOSE_NORMAL) printf("Device IP address: %s from config\n", inverters[devcount]->IPAddress);

            if (++devcount >= MAX_INVERTERS)
                break;
        }
    }

    for (uint32_t dev = 0; dev < devcount; dev++)
    {
        writePacketHeader(pcktBuf, 0, NULL);
        writePacket(pcktBuf, 0x09, 0xA0, 0, anySUSyID, anySerial);
        writeLong(pcktBuf, 0x00000200);
        writeLong(pcktBuf, 0);
        writeLong(pcktBuf, 0);
        writeLong(pcktBuf, 0);
        writePacketLength(pcktBuf);

        ethSend(pcktBuf, inverters[dev]->IPAddress);

        if ((rc = ethGetPacket()) == E_OK)
        {
            ethPacket *pckt = (ethPacket *)pcktBuf;
            inverters[dev]->SUSyID = btohs(pckt->Source.SUSyID);
            inverters[dev]->Serial = btohl(pckt->Source.Serial);
            if (VERBOSE_NORMAL) printf("Inverter replied: %s -> %d:%lu\n", inverters[dev]->IPAddress, inverters[dev]->SUSyID, inverters[dev]->Serial);

            logoffSMAInverter(inverters[dev]);
        }
        else
        {
            std::cout << "ERROR: Connection to inverter failed!\n";
            std::cout << "Is " << inverters[dev]->IPAddress << " a correct IP?" << std::endl;
            // Fix #412 skipping unresponsive inverter
            // Continue with next device instead of returning E_INIT and skipping the remaining devices
        }
    }

    return rc;
}

E_SBFSPOT initialiseSMAConnection(const char *BTAddress, InverterData *inverters[], bool MIS)
{
    if (VERBOSE_NORMAL)
    {
        puts("Initialising...");
        printf("SUSyID: %d - SessionID: %lu\n", AppSUSyID, AppSerial);
    }

    //Convert BT_Address '00:00:00:00:00:00' to BTAddress[6]
    //scanf reads %02X as int, but we need uint8_t
    unsigned int tmp[6];
    sscanf(BTAddress, "%02X:%02X:%02X:%02X:%02X:%02X", &tmp[5], &tmp[4], &tmp[3], &tmp[2], &tmp[1], &tmp[0]);

    // Multiple Inverter Support disabled
    // Connect to 1 and only 1 device (V2.0.6 compatibility mode)
    if (!MIS)
    {
        // Allocate memory for inverter data struct
        inverters[0] = new InverterData;
        resetInverterData(inverters[0]);

        // Copy previously converted BT address
        for (int i=0; i<6; i++)
            inverters[0]->BTAddress[i] = (uint8_t)tmp[i];

        // Call 2.0.6 init function
        return initialiseSMAConnection(inverters[0]);
    }

    for (int i=0; i<6; i++)
        RootDeviceAddress[i] = (uint8_t)tmp[i];

    //Init Inverter
    uint8_t version[6] = {1,0,0,0,0,0};
    writePacketHeader(pcktBuf, 0x0201, version);
    writeByte(pcktBuf, 'v');
    writeByte(pcktBuf, 'e');
    writeByte(pcktBuf, 'r');
    writeByte(pcktBuf, 13);    //CR
    writeByte(pcktBuf, 10);    //LF
    writePacketLength(pcktBuf);
    bthSend(pcktBuf);

    // This can take up to 3 seconds!
    if (getPacket(RootDeviceAddress, 0x02) != E_OK)
        return E_INIT;

    // Check protocol version
    // 3 => FW <  1.71 (NOK)
    // 4 => FW >= 1.71 (OK)
    if (pcktBuf[19] < 4)
        return E_FWVERSION;

    //Search devices
    uint8_t NetID = pcktBuf[22];
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
        memcpy(RootDeviceAddress, pcktBuf + 18, sizeof(RootDeviceAddress));

    if (DEBUG_NORMAL)
        printf("Root device address: %02X:%02X:%02X:%02X:%02X:%02X\n",
               RootDeviceAddress[5], RootDeviceAddress[4], RootDeviceAddress[3],
               RootDeviceAddress[2], RootDeviceAddress[1], RootDeviceAddress[0]);

    //Get local BT address
    memcpy(LocalBTAddress, pcktBuf + 25, sizeof(LocalBTAddress));

    if (DEBUG_NORMAL)
        printf("Local BT address: %02X:%02X:%02X:%02X:%02X:%02X\n",
               LocalBTAddress[5], LocalBTAddress[4], LocalBTAddress[3],
               LocalBTAddress[2], LocalBTAddress[1], LocalBTAddress[0]);

    if (getPacket(RootDeviceAddress, 0x05) != E_OK)
        return E_INIT;

    //Get network topology
    int pcktsize = get_short(pcktBuf+1);
    uint32_t devcount = 0;
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

                memcpy(inverters[devcount]->BTAddress, pcktBuf + ptr, sizeof(InverterData::BTAddress));

                inverters[devcount]->NetID = NetID;
                devcount++;
            }
            else if (DEBUG_HIGHEST) printf("MAX_INVERTERS limit (%d) reached.\n", MAX_INVERTERS);

        }
        else if (DEBUG_NORMAL) printf(memcmp((uint8_t *)pcktBuf+ptr, LocalBTAddress, sizeof(LocalBTAddress)) == 0 ? "Local BT Address\n" : "Another device?\n");
    }

    /***********************************************************************
        This part is only needed if you have more than one inverter
        The purpose is to (re)build the network when we have found only 1
    ************************************************************************/
    if((devcount == 1) && (NetID > 1))
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
            return E_INIT;    // Something went wrong... Failed to initialise
        }

        if (0x1001 == PacketType)
        {
            PacketType = 0;    //reset it
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

                        memcpy(inverters[devcount]->BTAddress, pcktBuf + ptr, sizeof(InverterData::BTAddress));

                        inverters[devcount]->NetID = NetID;
                        devcount++;
                    }
                    else if (DEBUG_HIGHEST) printf("MAX_INVERTERS limit (%d) reached.\n", MAX_INVERTERS);

                }
                else if (DEBUG_NORMAL) printf(memcmp((uint8_t *)pcktBuf+ptr, LocalBTAddress, sizeof(LocalBTAddress)) == 0 ? "Local BT Address\n" : "Another device?\n");
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
    for (uint32_t idx=0; inverters[idx]!=NULL && idx<MAX_INVERTERS; idx++)
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
E_SBFSPOT initialiseSMAConnection(InverterData* const invData)
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
    memcpy(LocalBTAddress, pcktBuf + 26, sizeof(LocalBTAddress));

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

E_SBFSPOT logonSMAInverter(InverterData* const inverters[], long userGroup, const char *password)
{
#define MAX_PWLENGTH 12
    uint8_t pw[MAX_PWLENGTH] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    if (DEBUG_NORMAL) puts("logonSMAInverter()");

    char encChar = (userGroup == UG_USER)? 0x88:0xBB;
    //Encode password
    unsigned int idx;
    for (idx = 0; (password[idx] != 0) && (idx < sizeof(pw)); idx++)
        pw[idx] = password[idx] + encChar;
    for (; idx < MAX_PWLENGTH; idx++)
        pw[idx] = encChar;

    E_SBFSPOT rc = E_OK;
    bool validPcktID = false;

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
            writeLong(pcktBuf, userGroup);    // User / Installer
            writeLong(pcktBuf, 0x00000384); // Timeout = 900sec ?
            writeLong(pcktBuf, now);
            writeLong(pcktBuf, 0);
            writeArray(pcktBuf, pw, sizeof(pw));
            writePacketTrailer(pcktBuf);
            writePacketLength(pcktBuf);
        }
        while (!isCrcValid(pcktBuf[packetposition-3], pcktBuf[packetposition-2]));

        bthSend(pcktBuf);

        do    //while (!validPcktID);
        {
            // In a multi inverter plant we get a reply from all inverters
            for (uint32_t i=0; inverters[i]!=NULL && i<MAX_INVERTERS; i++)
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
                            validPcktID = true;
                            unsigned short retcode = get_short(pcktBuf + 23);
                            switch (retcode)
                            {
                                case 0: rc = E_OK; break;
                                case 0x0100: rc = E_INVPASSW; break;
                                default: rc = (E_SBFSPOT)retcode; break;
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
        while (!validPcktID);
    }
    else    // CT_ETHERNET
    {
        for (uint32_t inv=0; inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
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
                writeLong(pcktBuf, userGroup);    // User / Installer
                writeLong(pcktBuf, 0x00000384); // Timeout = 900sec ?
                writeLong(pcktBuf, now);
                writeLong(pcktBuf, 0);
                writeArray(pcktBuf, pw, sizeof(pw));
                writePacketTrailer(pcktBuf);
                writePacketLength(pcktBuf);
            }
            while (!isCrcValid(pcktBuf[packetposition-3], pcktBuf[packetposition-2]));

            ethSend(pcktBuf, inverters[inv]->IPAddress);

            validPcktID = false;
            do
            {
                if ((rc = ethGetPacket()) == E_OK)
                {
                    ethPacket *pckt = (ethPacket *)pcktBuf;
                    if (pcktID == (btohs(pckt->PacketID) & 0x7FFF))   // Valid Packet ID
                    {
                        validPcktID = true;
                        unsigned short retcode = btohs(pckt->ErrorCode);
                        switch (retcode)
                        {
                            case 0: rc = E_OK; break;
                            case 0x0100: rc = E_INVPASSW; break;
                            default: rc = (E_SBFSPOT)retcode; break;
                        }
                    }
                    else
                        if (DEBUG_HIGHEST) printf("Packet ID mismatch. Expected %d, received %d\n", pcktID, (btohs(pckt->PacketID) & 0x7FFF));
                }
            } while (!validPcktID && (rc == E_OK)); // Fix Issue 167
        }
    }

    return rc;
}

E_SBFSPOT logoffSMAInverter(InverterData* const inverter)
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


#define MAX_CFG_AD 300    // Days
#define MAX_CFG_AM 300    // Months
#define MAX_CFG_AE 300    // Months
int parseCmdline(int argc, char **argv, Config *cfg)
{
    cfg->debug = 0;             // debug level - 0=none, 5=highest
    cfg->verbose = 0;           // verbose level - 0=none, 5=highest
    cfg->archDays = 1;          // today only
    cfg->archMonths = 1;        // this month only
    cfg->archEventMonths = 1;   // this month only
    cfg->forceInq = false;      // Inquire inverter also during the night
    cfg->userGroup = UG_USER;
    cfg->quiet = false;
    cfg->nocsv = false;
    cfg->nospot = false;
    cfg->nosql = false;
    // 123Solar Web Solar logger support(http://www.123solar.org/)
    // This is an undocumented feature and should only be used for 123solar
    cfg->s123 = S123_NOP;
    cfg->loadlive = false;      //force settings to prepare for live loading to http://pvoutput.org/loadlive.jsp
    cfg->startdate = 0;
    cfg->settime = false;
    cfg->mqtt = false;

    bool help_requested = false;

    //Set quiet/help mode
    for (int i = 1; i < argc; i++)
    {
        if (*argv[i] == '/')
            *argv[i] = '-';

        if (stricmp(argv[i], "-q") == 0)
            cfg->quiet = true;

        if (stricmp(argv[i], "-?") == 0)
            help_requested = true;
    }

    // Get path of executable
    // Fix Issue 169 (expand symlinks)
    cfg->AppPath = realpath(argv[0]);

    size_t pos = cfg->AppPath.find_last_of("/\\");
    if (pos != std::string::npos)
        cfg->AppPath.erase(++pos);
    else
        cfg->AppPath.clear();

    //Build fullpath to config file (SBFspot.cfg should be in same folder as SBFspot.exe)
    cfg->ConfigFile = cfg->AppPath + "SBFspot.cfg";

    char *pEnd = NULL;
    long lValue = 0;

    if (!cfg->quiet && !help_requested)
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
            if (strlen(argv[i]) == 2) lValue = 2;    // only -d sets medium debug level
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
            if (strlen(argv[i]) == 2) lValue = 2;    // only -v sets medium verbose level
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
            cfg->loadlive = true;

        //Set inquiryDark flag
        else if (stricmp(argv[i], "-finq") == 0)
            cfg->forceInq = true;

        //Set 123Solar command value (Undocumented - For 123Solar usage only)
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
            cfg->nocsv = true;

        //Set NoSQL flag (Disable SQL export)
        else if (stricmp(argv[i], "-nosql") == 0)
            cfg->nosql = true;

        //Set NoSpot flag (Disable Spot CSV export)
        else if (stricmp(argv[i], "-sp0") == 0)
            cfg->nospot = true;

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
                if (dt_start.length() == 8)    //YYYYMMDD
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

        // look for alternative config file (consistent with other args like -startdate and -password)
        else if (strnicmp(argv[i], "-cfg:", 5) == 0)
        {
            if (strlen(argv[i]) == 5)
            {
                InvalidArg(argv[i]);
                return -1;
            }
            else
            {
                //If -cfg: arg has no '\' it's only a filename and should be in the same folder as SBFspot executable
                cfg->ConfigFile = argv[i] + 5;
                if (cfg->ConfigFile.find_first_of("/\\") == std::string::npos)
                    cfg->ConfigFile = cfg->AppPath + (argv[i] + 5);
            }
        }

        // look for alternative config file (for backward compatibility)
        else if (strnicmp(argv[i], "-cfg", 4) == 0)
        {
            if (strlen(argv[i]) == 4)
            {
                InvalidArg(argv[i]);
                return -1;
            }
            else
            {
                //If -cfg arg has no '\' it's only a filename and should be in the same folder as SBFspot executable
                cfg->ConfigFile = argv[i] + 4;
                if (cfg->ConfigFile.find_first_of("/\\") == std::string::npos)
                    cfg->ConfigFile = cfg->AppPath + (argv[i] + 4);
            }
        }

        else if (stricmp(argv[i], "-settime") == 0)
            cfg->settime = true;

        //Scan for bluetooth devices
        else if (stricmp(argv[i], "-scan") == 0)
        {
#if defined(_WIN32)
            bthSearchDevices();
#else
            puts("On LINUX systems, use hcitool scan");
#endif
            return 1;    // Caller should terminate, no error
        }

        else if (stricmp(argv[i], "-mqtt") == 0)
            cfg->mqtt = true;

        //Show Help
        else if (stricmp(argv[i], "-?") == 0)
        {
            SayHello(1);
            return 1;    // Caller should terminate, no error
        }

        else if (!cfg->quiet)
        {
            InvalidArg(argv[i]);
            return -1;
        }

    }

    if (cfg->settime)
    {
        // Verbose output level should be at least = 2 (normal)
        if (cfg->verbose < 2)
            cfg->verbose = 2;

        cfg->forceInq = true;
    }

    //Disable verbose/debug modes when silent
    if (cfg->quiet)
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
    std::cout << "(c) 2012-2022, SBF (https://github.com/SBFspot/SBFspot)\n";
    std::cout << "Compiled for " << OS << " (" << BYTEORDER << ") " << sizeof(long) * 8 << " bit";
#if defined(USE_SQLITE)
    std::cout << " with SQLite support\n";
#endif
#if defined(USE_MYSQL)
    std::cout << " with MySQL support\n";
#endif
    if (ShowHelp != 0)
    {
        std::cout << "SBFspot [-options]\n";
        std::cout << " -scan               Scan for bluetooth enabled SMA inverters.\n";
        std::cout << " -d#                 Set debug level: 0-5 (0=none, default=2)\n";
        std::cout << " -v#                 Set verbose output level: 0-5 (0=none, default=2)\n";
        std::cout << " -ad#                Set #days for archived daydata: 0-" << MAX_CFG_AD << "\n";
        std::cout << "                     0=disabled, 1=today (default), ...\n";
        std::cout << " -am#                Set #months for archived monthdata: 0-" << MAX_CFG_AM << "\n";
        std::cout << "                     0=disabled, 1=current month (default), ...\n";
        std::cout << " -ae#                Set #months for archived events: 0-" << MAX_CFG_AE << "\n";
        std::cout << "                     0=disabled, 1=current month (default), ...\n";
        std::cout << " -cfg:filename.ext   Set alternative config file\n";
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

        std::cout << "\nLibraries used:\n";
#if defined(USE_SQLITE)
        std::cout << "\tSQLite V" << sqlite3_libversion() << '\n';
#endif
#if defined(USE_MYSQL)
        unsigned long mysql_version = mysql_get_client_version();
        std::cout << "\tMySQL V"
            << mysql_version / 10000     << "." // major
            << mysql_version / 100 % 100 << "." // minor
            << mysql_version % 100       << " (Client)" // build
            << '\n';
#endif
        std::cout << "\tBOOST V"     
            << BOOST_VERSION / 100000     << "." // major
            << BOOST_VERSION / 100 % 1000 << "." // minor
            << BOOST_VERSION % 100               // build
            << std::endl;
    }
}

//month: January = 0, February = 1...
int DaysInMonth(int month, int year)
{
    const int days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if ((month < 0) || (month > 11)) return 0;  //Error - Invalid month
    // If february, check for leap year
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
    memset(cfg->Local_BT_Address, 0, sizeof(cfg->Local_BT_Address));
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

    cfg->calcMissingSpot = false;
    strcpy(cfg->DateTimeFormat, "%d/%m/%Y %H:%M:%S");
    strcpy(cfg->DateFormat, "%d/%m/%Y");
    strcpy(cfg->TimeFormat, "%H:%M:%S");
    cfg->synchTime = 1;
    cfg->CSV_Export = true;
    cfg->CSV_ExtendedHeader = true;
    cfg->CSV_Header = true;
    cfg->CSV_SaveZeroPower = true;
    cfg->SunRSOffset = 900;
    cfg->SpotTimeSource = false;
    cfg->SpotWebboxHeader = false;
    cfg->MIS_Enabled = false;
    strcpy(cfg->locale, "en-US");
    cfg->synchTimeLow = 1;
    cfg->synchTimeHigh = 3600;
    // MQTT default values
    cfg->mqtt_host = "localhost";
    cfg->mqtt_port = ""; // mosquitto: 1883/8883 for TLS
#if defined(_WIN32)
    cfg->mqtt_publish_exe = "%ProgramFiles%\\mosquitto\\mosquitto_pub.exe";
#else
    cfg->mqtt_publish_exe = "/usr/bin/mosquitto_pub";
#endif
    cfg->mqtt_topic = "sbfspot";
    cfg->mqtt_publish_args = "-h {host} -t {topic} -m \"{{message}}\"";
    cfg->mqtt_publish_data = "Timestamp,SunRise,SunSet,InvSerial,InvName,InvStatus,EToday,ETotal,PACTot,UDC1,UDC2,IDC1,IDC2,PDC1,PDC2";
    cfg->mqtt_item_format = "\"{key}\": {value}";

    cfg->sqlPort = 3306;

    const char *CFG_Boolean = "(0-1)";
    const char *CFG_InvalidValue = "Invalid value for '%s' %s\n";

    FILE *fp;

    if ((fp = fopen(cfg->ConfigFile.c_str(), "r")) == NULL)
    {
        std::cout << "Error! Could not open file " << cfg->ConfigFile << std::endl;
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
                    strncpy(cfg->BT_Address, value, sizeof(cfg->BT_Address) - 1);
                }
                else if (stricmp(variable, "LocalBTaddress") == 0)
                {
                    strncpy(cfg->Local_BT_Address, value, sizeof(cfg->Local_BT_Address) - 1);
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
                            std::cout << "Invalid value for '" << variable << "' " << cfg->ip_addresslist[i] << std::endl;
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
                        cfg->calcMissingSpot = (lValue == 1);
                    else
                    {
                        fprintf(stdout, CFG_InvalidValue, variable, CFG_Boolean);
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
                        fprintf(stdout, CFG_InvalidValue, variable, "(comma|dot)");
                        rc = -2;
                    }
                }
                else if(stricmp(variable, "CSV_Delimiter") == 0)
                {
                    if (stricmp(value, "comma") == 0) cfg->delimiter = ',';
                    else if (stricmp(value, "semicolon") == 0) cfg->delimiter = ';';
                    else
                    {
                        fprintf(stdout, CFG_InvalidValue, variable, "(comma|semicolon)");
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
                        fprintf(stdout, CFG_InvalidValue, variable, "(0-30)");
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
                        fprintf(stdout, CFG_InvalidValue, variable, "(1-120)");
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
                        fprintf(stdout, CFG_InvalidValue, variable, "(1200-3600)");
                        rc = -2;
                    }
                }
                else if(stricmp(variable, "CSV_Export") == 0)
                {
                    lValue = strtol(value, &pEnd, 10);
                    if (((lValue == 0) || (lValue == 1)) && (*pEnd == 0))
                        cfg->CSV_Export = (lValue == 1);
                    else
                    {
                        fprintf(stdout, CFG_InvalidValue, variable, CFG_Boolean);
                        rc = -2;
                    }
                }
                else if(stricmp(variable, "CSV_ExtendedHeader") == 0)
                {
                    lValue = strtol(value, &pEnd, 10);
                    if (((lValue == 0) || (lValue == 1)) && (*pEnd == 0))
                        cfg->CSV_ExtendedHeader = (lValue == 1);
                    else
                    {
                        fprintf(stdout, CFG_InvalidValue, variable, CFG_Boolean);
                        rc = -2;
                    }
                }
                else if(stricmp(variable, "CSV_Header") == 0)
                {
                    lValue = strtol(value, &pEnd, 10);
                    if (((lValue == 0) || (lValue == 1)) && (*pEnd == 0))
                        cfg->CSV_Header = (lValue == 1);
                    else
                    {
                        fprintf(stdout, CFG_InvalidValue, variable, CFG_Boolean);
                        rc = -2;
                    }
                }
                else if(stricmp(variable, "CSV_SaveZeroPower") == 0)
                {
                    lValue = strtol(value, &pEnd, 10);
                    if (((lValue == 0) || (lValue == 1)) && (*pEnd == 0))
                        cfg->CSV_SaveZeroPower = (lValue == 1);
                    else
                    {
                        fprintf(stdout, CFG_InvalidValue, variable, CFG_Boolean);
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
                        fprintf(stdout, CFG_InvalidValue, variable, "(0-3600)");
                        rc = -2;
                    }
                }
                else if(stricmp(variable, "CSV_Spot_TimeSource") == 0)
                {
                    if (stricmp(value, "Inverter") == 0) cfg->SpotTimeSource = false;
                    else if (stricmp(value, "Computer") == 0) cfg->SpotTimeSource = true;
                    else
                    {
                        fprintf(stdout, CFG_InvalidValue, variable, "Inverter|Computer");
                        rc = -2;
                    }
                }

                else if(stricmp(variable, "CSV_Spot_WebboxHeader") == 0)
                {
                    lValue = strtol(value, &pEnd, 10);
                    if (((lValue == 0) || (lValue == 1)) && (*pEnd == 0))
                        cfg->SpotWebboxHeader = (lValue == 1);
                    else
                    {
                        fprintf(stdout, CFG_InvalidValue, variable, CFG_Boolean);
                        rc = -2;
                    }
                }
                else if(stricmp(variable, "MIS_Enabled") == 0)
                {
                    lValue = strtol(value, &pEnd, 10);
                    if (((lValue == 0) || (lValue == 1)) && (*pEnd == 0))
                        cfg->MIS_Enabled = (lValue == 1);
                    else
                    {
                        fprintf(stdout, CFG_InvalidValue, variable, CFG_Boolean);
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
                        fprintf(stdout, CFG_InvalidValue, variable, "de-DE|en-US|fr-FR|nl-NL|it-IT|es-ES");
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
                        fprintf(stdout, CFG_InvalidValue, variable, "(1-15)");
                        rc = -2;
                    }
                }
                else if(stricmp(variable, "Timezone") == 0)
                {
                    cfg->timezone = value;
                    boost::local_time::tz_database tzDB;
                    std::string tzdbPath = cfg->AppPath + "date_time_zonespec.csv";
                    // load the time zone database which comes with boost
                    // file must be UNIX file format (line delimiter=LF)
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
                        std::cout << "Invalid timezone specified: " << value << std::endl;
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
                else if (stricmp(variable, "SQL_Port") == 0)
                    try
                        {
                        cfg->sqlPort = boost::lexical_cast<unsigned int>(value);
                        }
                        catch (...)
                        {
                            fprintf(stdout, CFG_InvalidValue, variable, "");
                            rc = -2;
                            break;
                        }
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
                        fprintf(stdout, CFG_InvalidValue, variable, "(none|blank|comma|semicolon)");
                        rc = -2;
                    }
                }

                // Add more config keys here

                else
                    fprintf(stdout, "Warning: Ignoring keyword '%s'\n", variable);
            }
        }
    }
    fclose(fp);

    if (rc != 0) return rc;

    if (strlen(cfg->BT_Address) > 0)
        cfg->ConnectionType = CT_BLUETOOTH;
    else if (strlen(cfg->IP_Address) > 0)
    {
        cfg->ConnectionType = CT_ETHERNET;
        cfg->IP_Port = 9522;
    }
    else
        cfg->ConnectionType = CT_NONE;

    if (strlen(cfg->SMA_Password) == 0)
    {
        fprintf(stdout, "Missing USER Password.\n");
        rc = -2;
    }

    if (cfg->decimalpoint == cfg->delimiter)
    {
        fprintf(stdout, "'CSV_Delimiter' and 'DecimalPoint' must be different character.\n");
        rc = -2;
    }

    if (strlen(cfg->outputPath) == 0)
    {
        fprintf(stdout, "Missing OutputPath.\n");
        rc = -2;
    }

    if (cfg->timezone.empty())
    {
        std::cout << "Missing timezone.\n";
        rc = -2;
    }

    if (rc == 0)
    {
        if (strlen(cfg->plantname) == 0)
            strncpy(cfg->plantname, "MyPlant", sizeof(cfg->plantname));

        //Overrule CSV_Export from config with Commandline setting -nocsv
        if (cfg->nocsv)
            cfg->CSV_Export = false; // Fix #549

        //Silently enable CSV_Header when CSV_ExtendedHeader is enabled
        if (cfg->CSV_ExtendedHeader)
            cfg->CSV_Header = true;

        //If OutputPathEvents is omitted, use OutputPath
        if (strlen(cfg->outputPath_Events) == 0)
            strcpy(cfg->outputPath_Events, cfg->outputPath);

        //force settings to prepare for live loading to http://pvoutput.org/loadlive.jsp
        if (cfg->loadlive)
        {
            strncat(cfg->outputPath, "/LoadLive", sizeof(cfg->outputPath));
            strcpy(cfg->DateTimeFormat, "%H:%M");
            cfg->CSV_Export = true;
            cfg->decimalpoint = '.';
            cfg->CSV_Header = false;
            cfg->CSV_ExtendedHeader = false;
            cfg->CSV_SaveZeroPower = false;
            cfg->delimiter = ';';
            cfg->archEventMonths = 0;
            cfg->archMonths = 0;
            cfg->nospot = true;
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
    }

    return rc;
}

void ShowConfig(Config *cfg)
{
    std::cout << "\nConfiguration settings:";
    if (strlen(cfg->BT_Address) > 0)
        std::cout << "\nBTAddress=" << cfg->BT_Address << \
        "\nMIS_Enabled=" << cfg->MIS_Enabled << \
        "\nBTConnectRetries=" << cfg->BT_ConnectRetries;

    if (strlen(cfg->Local_BT_Address) > 0)
        std::cout << "\nLocalBTAddress=" << cfg->Local_BT_Address;
    
    if (cfg->ip_addresslist.size() > 0)
    {
        std::ostringstream iplist;
        for (const auto &ip : cfg->ip_addresslist)
            iplist << ',' << ip;

        std::cout << "\nIP_Address=" << iplist.str().substr(1);
    }
    
    std::cout << "\nPassword=<undisclosed>" << \
        "\nPlantname=" << cfg->plantname << \
        "\nOutputPath=" << cfg->outputPath << \
        "\nOutputPathEvents=" << cfg->outputPath_Events << \
        "\nLatitude=" << cfg->latitude << \
        "\nLongitude=" << cfg->longitude << \
        "\nTimezone=" << cfg->timezone << \
        "\nLocale=" << cfg->locale << \
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
        "\nCSV_Spot_WebboxHeader=" << cfg->SpotWebboxHeader;

#if defined(USE_MYSQL) || defined(USE_SQLITE)
    std::cout << "\nSQL_Database=" << cfg->sqlDatabase;
#endif

#if defined(USE_MYSQL)
    std::cout << "\nSQL_Hostname=" << cfg->sqlHostname << \
        "\nSQL_Port=" << cfg->sqlPort << \
        "\nSQL_Username=" << cfg->sqlUsername << \
        "\nSQL_Password=<undisclosed>";
#endif

    if (cfg->mqtt)
    {
        std::cout << "\nMQTT_Host=" << cfg->mqtt_host << \
            "\nMQTT_Port=" << cfg->mqtt_port << \
            "\nMQTT_Topic=" << cfg->mqtt_topic << \
            "\nMQTT_Publisher=" << cfg->mqtt_publish_exe << \
            "\nMQTT_PublisherArgs=" << cfg->mqtt_publish_args << \
            "\nMQTT_Data=" << cfg->mqtt_publish_data << \
            "\nMQTT_ItemFormat=" << cfg->mqtt_item_format;
    }

    std::cout << "\nEnd of Config\n" << std::endl;
}

bool isCrcValid(uint8_t lb, uint8_t hb)
{
    if ((ConnType == CT_BLUETOOTH) && ((lb == 0x7E) || (hb == 0x7E) || (lb == 0x7D) || (hb == 0x7D)))
        return false;
    else
        return true;   //Always true for ethernet
}

//Power Values are missing on some inverters
void CalcMissingSpot(InverterData *invData)
{
    for (auto &mppt : invData->mpp)
    {
        if (mppt.second.Pdc() == 0) mppt.second.Pdc(mppt.second.Idc() * mppt.second.Udc() / 100000);
    }

    if (invData->Pac1 == 0) invData->Pac1 = (invData->Iac1 * invData->Uac1) / 100000;
    if (invData->Pac2 == 0) invData->Pac2 = (invData->Iac2 * invData->Uac2) / 100000;
    if (invData->Pac3 == 0) invData->Pac3 = (invData->Iac3 * invData->Uac3) / 100000;

    if (invData->TotalPac == 0) invData->TotalPac = invData->Pac1 + invData->Pac2 + invData->Pac3;
}

/*
* isValidSender() compares 6-byte senderaddress with our inverter BT address
* If senderaddress = addr_unknown (FF:FF:FF:FF:FF:FF) then any address is valid
*/
bool isValidSender(uint8_t senderaddr[6], uint8_t address[6])
{
    for (int i = 0; i < 6; i++)
        if ((senderaddr[i] != address[i]) && (senderaddr[i] != 0xFF))
            return false;

    return true;
}

const std::string u64_tostring(const uint64_t u64)
{
    std::ostringstream ss;

    if (is_NaN(u64))
        ss << "NaN";
    else
        ss << u64;

    return ss.str();
}

const std::string s64_tostring(const int64_t s64)
{
    std::ostringstream ss;

    if (is_NaN(s64))
        ss << "NaN";
    else
        ss << s64;

    return ss.str();
}

const std::string u32_tostring(const uint32_t u32)
{
    std::ostringstream ss;

    if (is_NaN(u32))
        ss << "NaN";
    else
        ss << u32;

    return ss.str();
}

const std::string s32_tostring(const int32_t s32)
{
    std::ostringstream ss;

    if (is_NaN(s32))
        ss << "NaN";
    else
        ss << s32;

    return ss.str();
}

const std::string version_tostring(int32_t version)
{
    char ver[16];

    uint8_t Vtype = version & 0xFF;
    Vtype = Vtype > 5 ? '?' : "NEABRS"[Vtype]; //NOREV-EXPERIMENTAL-ALPHA-BETA-RELEASE-SPECIAL
    uint8_t Vbuild = (version >> 8) & 0xFF;
    uint8_t Vminor = (version >> 16) & 0xFF;
    uint8_t Vmajor = (version >> 24) & 0xFF;

    //Vmajor and Vminor = 0x12 should be printed as '12' and not '18' (BCD)
    snprintf(ver, sizeof(ver), "%c%c.%c%c.%02d.%c", '0' + (Vmajor >> 4), '0' + (Vmajor & 0x0F), '0' + (Vminor >> 4), '0' + (Vminor & 0x0F), Vbuild, Vtype);

    return std::string(ver);
}

void debug_watt(const char *txt, const int32_t val, const time_t dt)
{
    if (DEBUG_NORMAL)
    {
        printf("%-12s: %d (W) %s", txt, val, ctime(&dt));
    }
}

void debug_volt(const char *txt, const int32_t val, const time_t dt)
{
    if (DEBUG_NORMAL)
    {
        printf("%-12s: %.2f (V) %s", txt, toVolt(val), ctime(&dt));
    }
}

void debug_amp(const char *txt, const int32_t val, const time_t dt)
{
    if (DEBUG_NORMAL)
    {
        printf("%-12s: %.3f (A) %s", txt, toAmp(val), ctime(&dt));
    }
}

void debug_hz(const char *txt, const int32_t val, const time_t dt)
{
    if (DEBUG_NORMAL)
    {
        printf("%-12s: %.2f (Hz) %s", txt, toHz(val), ctime(&dt));
    }
}

void debug_kwh(const char *txt, const uint64_t val, const time_t dt)
{
    if (DEBUG_NORMAL)
    {
        printf("%-12s: %.3f (kWh) %s", txt, tokWh(val), ctime(&dt));
    }
}

void debug_hour(const char *txt, const uint64_t val, const time_t dt)
{
    if (DEBUG_NORMAL)
    {
        printf("%-12s: %.3f (h) %s", txt, toHour(val), ctime(&dt));
    }
}

void debug_text(const char *txt, const char *val, const time_t dt)
{
    if (DEBUG_NORMAL)
    {
        printf("%-12s: '%s' %s", txt, val, ctime(&dt));
    }
}

std::vector <uint32_t> getattribute(uint8_t *pcktbuf)
{
    const int recordsize = 40;
    uint32_t tag, attribute;
    std::vector <uint32_t> tags;
    for (int idx = 8; idx < recordsize; idx += 4)
    {
        attribute = ((uint32_t)get_long(pcktbuf + idx));
        tag = attribute & 0x00FFFFFF;
        if (tag == 0xFFFFFE)
            break;
        if ((attribute >> 24) == 1)
            tags.push_back(tag);
    }

    return tags;
}

E_SBFSPOT getInverterData(InverterData *device, unsigned long command, unsigned long first, unsigned long last)
{
    device->status = E_OK;

    do
    {
        pcktID++;
        writePacketHeader(pcktBuf, 0x01, addr_unknown);
        if (device->SUSyID == SID_SB240)
            writePacket(pcktBuf, 0x09, 0xE0, 0, device->SUSyID, device->Serial);
        else
            writePacket(pcktBuf, 0x09, 0xA0, 0, device->SUSyID, device->Serial);
        writeLong(pcktBuf, command);
        writeLong(pcktBuf, first);
        writeLong(pcktBuf, last);
        writePacketTrailer(pcktBuf);
        writePacketLength(pcktBuf);
    } while (!isCrcValid(pcktBuf[packetposition - 3], pcktBuf[packetposition - 2]));

    if (ConnType == CT_BLUETOOTH)
    {
        bthSend(pcktBuf);
    }
    else
    {
        ethSend(pcktBuf, device->IPAddress);
    }

    unsigned short pcktcount = 0;
    bool validPcktID = false;
    do
    {
        do
        {
            if (ConnType == CT_BLUETOOTH)
                device->status = getPacket(device->BTAddress, 1);
            else
                device->status = ethGetPacket();

            if (device->status != E_OK)
                return device->status;

            if ((ConnType == CT_BLUETOOTH) && (!validateChecksum()))
            {
                device->status = E_CHKSUM;
                return device->status;
            }
            else
            {
                if ((device->status = (E_SBFSPOT)get_short(pcktBuf + 23)) != E_OK)
                {
                    if (VERBOSE_NORMAL) printf("Packet status: %d\n", device->status);
                    return device->status;
                }
                pcktcount = get_short(pcktBuf + 25);
                unsigned short rcvpcktID = get_short(pcktBuf + 27) & 0x7FFF;
                if (pcktID == rcvpcktID)
                {
                    if (((uint16_t)get_short(pcktBuf + 15) == device->SUSyID) && ((uint32_t)get_long(pcktBuf + 17) == device->Serial))
                    {
                        validPcktID = true;
                        int32_t value = 0;
                        int64_t value64 = 0;
                        uint32_t recordsize = 4 * ((uint32_t)pcktBuf[5] - 9) / ((uint32_t)get_long(pcktBuf + 37) - (uint32_t)get_long(pcktBuf + 33) + 1);

                        for (int ii = 41; ii < packetposition - 3; ii += recordsize)
                        {
                            uint8_t *recptr = pcktBuf + ii;
                            uint32_t code = ((uint32_t)get_long(recptr));
                            LriDef lri = (LriDef)(code & 0x00FFFF00);
                            uint32_t cls = code & 0xFF;
                            uint8_t dataType = code >> 24;
                            time_t datetime = (time_t)get_long(recptr + 4);

                            // fix: We can't rely on dataType because it can be both 0x00 or 0x40 for DWORDs
                            //if ((lri == MeteringDyWhOut) || (lri == MeteringTotWhOut) || (lri == MeteringTotFeedTms) || (lri == MeteringTotOpTms))  //QWORD
                            if (recordsize == 16)
                            {
                                value64 = get_longlong(recptr + 8);
                                if (is_NaN(value64) || is_NaN((uint64_t)value64))
                                    value64 = 0;
                            }
                            else if ((dataType != DT_STRING) && (dataType != DT_STATUS))
                            {
                                value = get_long(recptr + 16);
                                if (is_NaN(value) || is_NaN((uint32_t)value))
                                    value = 0;
                            }

                            switch (lri)
                            {
                            case GridMsTotW: //SPOT_PACTOT
                                //This function gives us the time when the inverter was switched off
                                device->SleepTime = datetime;
                                device->TotalPac = value;
                                debug_watt("SPOT_PACTOT", value, datetime);
                                break;

                            case GridMsWphsA: //SPOT_PAC1
                                device->Pac1 = value;
                                debug_watt("SPOT_PAC1", value, datetime);
                                break;

                            case GridMsWphsB: //SPOT_PAC2
                                device->Pac2 = value;
                                debug_watt("SPOT_PAC2", value, datetime);
                                break;

                            case GridMsWphsC: //SPOT_PAC3
                                device->Pac3 = value;
                                debug_watt("SPOT_PAC3", value, datetime);
                                break;

                            case GridMsPhVphsA: //SPOT_UAC1
                                device->Uac1 = value;
                                debug_volt("SPOT_UAC1", value, datetime);
                                break;

                            case GridMsPhVphsB: //SPOT_UAC2
                                device->Uac2 = value;
                                debug_volt("SPOT_UAC2", value, datetime);
                                break;

                            case GridMsPhVphsC: //SPOT_UAC3
                                device->Uac3 = value;
                                debug_volt("SPOT_UAC3", value, datetime);
                                break;

                            case GridMsAphsA_1: //SPOT_IAC1
                            case GridMsAphsA:
                                device->Iac1 = value;
                                debug_amp("SPOT_IAC1", value, datetime);
                                break;

                            case GridMsAphsB_1: //SPOT_IAC2
                            case GridMsAphsB:
                                device->Iac2 = value;
                                debug_amp("SPOT_IAC2", value, datetime);
                                break;

                            case GridMsAphsC_1: //SPOT_IAC3
                            case GridMsAphsC:
                                device->Iac3 = value;
                                debug_amp("SPOT_IAC3", value, datetime);
                                break;

                            case GridMsHz: //SPOT_FREQ
                                device->GridFreq = value;
                                debug_hz("SPOT_FREQ", value, datetime);
                                break;

                            case DcMsWatt: //SPOT_PDC1 / SPOT_PDC2
                            {
                                auto it = device->mpp.find((uint8_t)cls);
                                if (it != device->mpp.end())
                                    it->second.Pdc(value);
                                else
                                {
                                    mppt new_mppt;
                                    new_mppt.Pdc(value);
                                    device->mpp.insert(std::make_pair(cls, new_mppt));
                                }

                                debug_watt((std::string("SPOT_PDC") + std::to_string(cls)).c_str(), value, datetime);

                                device->calPdcTot += value;

                                break;
                            }

                            case DcMsVol: //SPOT_UDC1 / SPOT_UDC2
                            {
                                auto it = device->mpp.find((uint8_t)cls);
                                if (it != device->mpp.end())
                                    it->second.Udc(value);
                                else
                                {
                                    mppt new_mppt;
                                    new_mppt.Udc(value);
                                    device->mpp.insert(std::make_pair(cls, new_mppt));
                                }

                                debug_volt((std::string("SPOT_UDC") + std::to_string(cls)).c_str(), value, datetime);

                                break;
                            }

                            case DcMsAmp: //SPOT_IDC1 / SPOT_IDC2
                            {
                                auto it = device->mpp.find((uint8_t)cls);
                                if (it != device->mpp.end())
                                    it->second.Idc(value);
                                else
                                {
                                    mppt new_mppt;
                                    new_mppt.Idc(value);
                                    device->mpp.insert(std::make_pair(cls, new_mppt));
                                }

                                debug_amp((std::string("SPOT_IDC") + std::to_string(cls)).c_str(), value, datetime);

                                break;
                            }

                            case MeteringTotWhOut: //SPOT_ETOTAL
                                //In case SPOT_ETODAY missing, this function gives us inverter time (eg: SUNNY TRIPOWER 6.0)
                                device->InverterDatetime = datetime;
                                device->ETotal = value64;
                                debug_kwh("SPOT_ETOTAL", value64, datetime);
                                break;

                            case MeteringDyWhOut: //SPOT_ETODAY
                                //This function gives us the current inverter time
                                device->InverterDatetime = datetime;
                                device->EToday = value64;
                                debug_kwh("SPOT_ETODAY", value64, datetime);
                                break;

                            case MeteringTotOpTms: //SPOT_OPERTM
                                device->OperationTime = value64;
                                debug_hour("SPOT_OPERTM", value64, datetime);
                                break;

                            case MeteringTotFeedTms: //SPOT_FEEDTM
                                device->FeedInTime = value64;
                                debug_hour("SPOT_FEEDTM", value64, datetime);
                                break;

                            case NameplateLocation: //INV_NAME
                                //This function gives us the time when the inverter was switched on
                                device->WakeupTime = datetime;
                                device->DeviceName = std::string((char *)recptr + 8, strnlen((char *)recptr + 8, recordsize - 8)); // Fix #506
                                debug_text("INV_NAME", device->DeviceName.c_str(), datetime);
                                break;

                            case NameplatePkgRev: //INV_SWVER
                                device->SWVersion = version_tostring(get_long(recptr + 24));
                                debug_text("INV_SWVER", device->SWVersion.c_str(), datetime);
                                break;

                            case NameplateModel: //INV_TYPE
                            {
                                auto attr = getattribute(recptr);
                                if (attr.size() > 0)
                                {
                                    device->DeviceType = tagdefs.getDesc(attr.front());
                                    if (device->DeviceType.empty())
                                    {
                                        device->DeviceType = "UNKNOWN TYPE";
                                        printf("Unknown Inverter Type. Report this issue at https://github.com/SBFspot/SBFspot/issues with following info:\n");
                                        printf("ID='%d' and Type=<Fill in the exact inverter model> (e.g. SB1300TL-10)\n", attr.front());
                                    }
                                    debug_text("INV_TYPE", device->DeviceType.c_str(), datetime);
                                }
                                break;
                            }

                            case NameplateMainModel: //INV_CLASS
                            {
                                auto attr = getattribute(recptr);
                                if (attr.size() > 0)
                                {
                                    device->DevClass = (DEVICECLASS)attr.front();
                                    device->DeviceClass = tagdefs.getDesc(device->DevClass, "UNKNOWN CLASS");

                                    debug_text("INV_CLASS", device->DeviceClass.c_str(), datetime);
                                }
                                break;
                            }

                            case OperationHealth: //INV_STATUS:
                            {
                                auto attr = getattribute(recptr);
                                if (attr.size() > 0)
                                {
                                    device->DeviceStatus = attr.front();
                                    debug_text("INV_STATUS", tagdefs.getDesc(device->DeviceStatus, "?").c_str(), datetime);
                                }
                                break;
                            }

                            case OperationGriSwStt: //INV_GRIDRELAY
                            {
                                auto attr = getattribute(recptr);
                                if (attr.size() > 0)
                                {
                                    device->GridRelayStatus = attr.front();
                                    debug_text("INV_GRIDRELAY", tagdefs.getDesc(device->GridRelayStatus, "?").c_str(), datetime);
                                }
                                break;
                            }

                            case BatChaStt:
                                device->BatChaStt = value;
                                break;

                            case BatDiagCapacThrpCnt:
                                device->BatDiagCapacThrpCnt = value;
                                break;

                            case BatDiagTotAhIn:
                                device->BatDiagTotAhIn = value;
                                break;

                            case BatDiagTotAhOut:
                                device->BatDiagTotAhOut = value;
                                break;

                            case BatTmpVal:
                                device->BatTmpVal = value;
                                break;

                            case BatVol:
                                device->BatVol = value;
                                break;

                            case BatAmp:
                                device->BatAmp = value;
                                break;

                            case CoolsysTmpNom:
                                device->Temperature = value;
                                break;

                            case MeteringGridMsTotWOut:
                                device->MeteringGridMsTotWOut = value;
                                break;

                            case MeteringGridMsTotWIn:
                                device->MeteringGridMsTotWIn = value;
                                break;

                            default:
                                switch (dataType)
                                {
                                case DT_ULONG:
                                    if (recordsize == 16)
                                    {
                                        printf("%08X %d %s '%s' %s\n", code, recordsize, strtok(ctime(&datetime), "\n"), tagdefs.getDescForLRI(lri).c_str(), u64_tostring(get_longlong(recptr + 8)).c_str());
                                    }
                                    else if (recordsize == 28)
                                    {
                                        printf("%08X %d %s '%s' %s %s %s %s\n", code, recordsize, strtok(ctime(&datetime), "\n"), tagdefs.getDescForLRI(lri).c_str(),
                                            u32_tostring(get_long(recptr + 8)).c_str(),
                                            u32_tostring(get_long(recptr + 12)).c_str(),
                                            u32_tostring(get_long(recptr + 16)).c_str(),
                                            u32_tostring(get_long(recptr + 20)).c_str()
                                        );
                                    }
                                    else if (recordsize == 40)
                                    {
                                        printf("%08X %d %s '%s' %s %s %s %s %s %s\n", code, recordsize, strtok(ctime(&datetime), "\n"), tagdefs.getDescForLRI(lri).c_str(),
                                            u32_tostring(get_long(recptr + 8)).c_str(),
                                            u32_tostring(get_long(recptr + 12)).c_str(),
                                            u32_tostring(get_long(recptr + 16)).c_str(),
                                            u32_tostring(get_long(recptr + 20)).c_str(),
                                            u32_tostring(get_long(recptr + 24)).c_str(),
                                            u32_tostring(get_long(recptr + 28)).c_str()
                                        );
                                    }
                                    else
                                        printf("%08X ?%d? %s '%s'\n", code, recordsize, strtok(ctime(&datetime), "\n"), tagdefs.getDescForLRI(lri).c_str());
                                    break;

                                case DT_STATUS:
                                {
                                    for (const auto &tag : getattribute(recptr))
                                        printf("%08X %d %s %s: '%s'\n", code, recordsize, strtok(ctime(&datetime), "\n"), tagdefs.getDescForLRI(lri).c_str(), tagdefs.getDesc(tag, "???").c_str());
                                }
                                break;

                                case DT_STRING:
                                {
                                    char str[40];
                                    strncpy(str, (char*)recptr + 8, recordsize - 8);
                                    printf("%08X %d %s %s: '%s'\n", code, recordsize, strtok(ctime(&datetime), "\n"), tagdefs.getDescForLRI(lri).c_str(), str);
                                }
                                break;

                                case DT_SLONG:
                                    if (recordsize == 16)
                                    {
                                        printf("%08X %d %s '%s' %s\n", code, recordsize, strtok(ctime(&datetime), "\n"), tagdefs.getDescForLRI(lri).c_str(), s64_tostring(get_longlong(recptr + 8)).c_str());
                                    }
                                    else if (recordsize == 28)
                                    {
                                        printf("%08X %d %s '%s' %s %s %s %s\n", code, recordsize, strtok(ctime(&datetime), "\n"), tagdefs.getDescForLRI(lri).c_str(),
                                            s32_tostring(get_long(recptr + 8)).c_str(),
                                            s32_tostring(get_long(recptr + 12)).c_str(),
                                            s32_tostring(get_long(recptr + 16)).c_str(),
                                            s32_tostring(get_long(recptr + 20)).c_str()
                                        );

                                    }
                                    else if (recordsize == 40)
                                    {
                                        printf("%08X %d %s '%s' %s %s %s %s %s %s\n", code, recordsize, strtok(ctime(&datetime), "\n"), tagdefs.getDescForLRI(lri).c_str(),
                                            s32_tostring(get_long(recptr + 8)).c_str(),
                                            s32_tostring(get_long(recptr + 12)).c_str(),
                                            s32_tostring(get_long(recptr + 16)).c_str(),
                                            s32_tostring(get_long(recptr + 20)).c_str(),
                                            s32_tostring(get_long(recptr + 24)).c_str(),
                                            s32_tostring(get_long(recptr + 28)).c_str()
                                        );
                                    }
                                    else
                                        printf("%08X ?%d? %s '%s'\n", code, recordsize, strtok(ctime(&datetime), "\n"), tagdefs.getDescForLRI(lri).c_str());
                                    break;

                                default:
                                    printf("%08X %d %s '%s'\n", code, recordsize, strtok(ctime(&datetime), "\n"), tagdefs.getDescForLRI(lri).c_str());
                                    break;
                                }
                                break;
                            }
                        }
                    }
                }
                else
                {
                    if (DEBUG_HIGHEST) printf("Packet ID mismatch. Expected %d, received %d\n", pcktID, rcvpcktID);
                    validPcktID = false;
                    pcktcount = 0;
                }
            }
        } while (pcktcount > 0);
    } while (!validPcktID);

    return device->status;
}

E_SBFSPOT getInverterData(InverterData *devList[], enum getInverterDataType type)
{
    E_SBFSPOT rc = E_OK;
    unsigned long command;
    unsigned long first;
    unsigned long last;

    switch (type)
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
        break;

    case MeteringGridMsTotW:
        command = 0x51000200;
        first = 0x00463600;
        last = 0x004637FF;
        break;

    default:
        return E_BADARG;
    };

    for (uint32_t i = 0; devList[i] != NULL && i < MAX_INVERTERS; i++)
    {
        uint32_t retries = MAX_RETRY;
        do
        {
            if ((rc = getInverterData(devList[i], command, first, last)) == E_NODATA)
            {
                if (DEBUG_NORMAL) puts("Retrying...");
                retries--;
            }
            else retries = 0;
        } while (retries > 0);
    }

    return rc;
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
    inv->DeviceStatus = 0;
    inv->EToday = 0;
    inv->ETotal = 0;
    inv->FeedInTime = 0;
    inv->GridFreq = 0;
    inv->GridRelayStatus = 0;
    inv->Iac1 = 0;
    inv->Iac2 = 0;
    inv->Iac3 = 0;
    inv->InverterDatetime = 0;
    inv->IPAddress[0] = 0;
    inv->modelID = 0;
    inv->NetID = 0;
    inv->OperationTime = 0;
    inv->Pac1 = 0;
    inv->Pac2 = 0;
    inv->Pac3 = 0;
    inv->Pmax1 = 0;
    inv->Pmax2 = 0;
    inv->Pmax3 = 0;
    inv->Serial = 0;
    inv->SleepTime = 0;
    inv->SUSyID = 0;
    inv->Temperature = NaN_S32;
    inv->TotalPac = 0;
    inv->Uac1 = 0;
    inv->Uac2 = 0;
    inv->Uac3 = 0;
    inv->WakeupTime = 0;
    inv->monthDataOffset = 0;
    inv->multigateID = NaN_U32;
    inv->MeteringGridMsTotWIn = 0;
    inv->MeteringGridMsTotWOut = 0;
    inv->hasBattery = false;
    inv->mpp.insert(std::make_pair(1, mppt(0, 0, 0)));
    inv->mpp.insert(std::make_pair(2, mppt(0, 0, 0)));
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
    } while (!isCrcValid(pcktBuf[packetposition - 3], pcktBuf[packetposition - 2]));

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
    } while (!isCrcValid(pcktBuf[packetposition - 3], pcktBuf[packetposition - 2]));

    if (ConnType == CT_BLUETOOTH)
    {
        bthSend(pcktBuf);
    }
    else
    {
        ethSend(pcktBuf, inv->IPAddress);
    }

    bool validPcktID = false;
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
            unsigned short rcvpcktID = get_short(pcktBuf + 27) & 0x7FFF;
            if (pcktID == rcvpcktID)
            {
                uint32_t serial = get_long(pcktBuf + 17);
                if (serial == inv->Serial)
                {
                    rc = E_NODATA;
                    validPcktID = true;
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
    } while (!validPcktID);

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
        std::cout << "ERROR: MAX_INVERTERS too low." << std::endl;
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
    } while (!isCrcValid(pcktBuf[packetposition - 3], pcktBuf[packetposition - 2]));

    if (ethSend(pcktBuf, devList[multigateID]->IPAddress) == -1) // SOCKET_ERROR
        return E_NODATA;

    bool validPcktID = false;
    do
    {
        rc = ethGetPacket();

        if (rc != E_OK) return rc;

        int16_t errorcode = get_short(pcktBuf + 23);
        if (errorcode != 0)
        {
            std::cout << "Received errorcode=" << errorcode << std::endl;
            return (E_SBFSPOT)errorcode;
        }

        unsigned short rcvpcktID = get_short(pcktBuf + 27) & 0x7FFF;
        if (pcktID == rcvpcktID)
        {
            //uint32_t lastrec = get_long(pcktBuf + 37);

            uint32_t serial = get_long(pcktBuf + 17);
            if (serial == devList[multigateID]->Serial)
            {
                rc = E_NODATA;
                validPcktID = true;
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
                            std::cout << "ERROR: MAX_INVERTERS too low." << std::endl;
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
    } while (!validPcktID);

    return rc;
}

E_SBFSPOT logoffMultigateDevices(InverterData* const inverters[])
{
    if (DEBUG_NORMAL) puts("logoffMultigateDevices()");
    for (uint32_t mg = 0; inverters[mg] != NULL && mg<MAX_INVERTERS; mg++)
    {
        InverterData *pmg = inverters[mg];
        if (pmg->SUSyID == SID_MULTIGATE)
        {
            pmg->hasDayData = true;
            for (uint32_t sb240 = 0; inverters[sb240] != NULL && sb240<MAX_INVERTERS; sb240++)
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
                    } while (!isCrcValid(pcktBuf[packetposition - 3], pcktBuf[packetposition - 2]));

                    ethSend(pcktBuf, psb->IPAddress);

                    psb->logonStatus = 0; // logged of

                    if (VERBOSE_NORMAL) std::cout << "Logoff " << psb->SUSyID << ":" << psb->Serial << std::endl;
                }
            }
        }
    }

    return E_OK;
}
