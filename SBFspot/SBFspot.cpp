/************************************************************************************************
                               ____  ____  _____                _
                              / ___|| __ )|  ___|__ _ __   ___ | |_
                              \___ \|  _ \| |_ / __| '_ \ / _ \| __|
                               ___) | |_) |  _|\__ \ |_) | (_) | |_
                              |____/|____/|_|  |___/ .__/ \___/ \__|
                                                   |_|

	SBFspot - Yet another tool to read power production of SMA solar/battery inverters
	(c)2012-2021, SBF

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

// Fix undefined reference to 'boost::system::system_category()' introduced with PR #361
#define BOOST_ERROR_CODE_HEADER_ONLY

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
#include "mqtt.h"

using namespace std;
using namespace boost;
using namespace boost::date_time;
using namespace boost::posix_time;
using namespace boost::gregorian;

int MAX_CommBuf = 0;
int MAX_pcktBuf = 0;

//Public vars
int debug = 0;
int verbose = 0;
int quiet = 0;
char DateTimeFormat[32];
char DateFormat[32];
CONNECTIONTYPE ConnType = CT_NONE;
TagDefs tagdefs = TagDefs();
bool hasBatteryDevice = false;	// Plant has 1 or more battery device(s)

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

    for (uint32_t inv=0; inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
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
    for (uint32_t inv=0; inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
    {
        if (inverters[inv]->Serial == Serial)
            return inv;
    }

    return -1;
}

int getInverterIndexByAddress(InverterData* const inverters[], unsigned char bt_addr[6])
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

E_SBFSPOT ethInitConnection(InverterData *inverters[], const char *IP_Address)
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

    	//SMA inverter announces its presence in response to the discovery request packet
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

			logoffSMAInverter(inverters[devcount]);
		}
		else
		{
			std::cerr << "ERROR: Connection to inverter failed!" << std::endl;
			std::cerr << "Is " << inverters[devcount]->IPAddress << " the correct IP?" << std::endl;
			std::cerr << "Please check IP_Address in SBFspot.cfg!" << std::endl;
			// Fix #412 skipping unresponsive inverter
			// Continue with next device instead of returning E_INIT and skipping the remaining devices
			// return E_INIT;
		}
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

E_SBFSPOT logonSMAInverter(InverterData* const inverters[], long userGroup, const char *password)
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

    for (uint32_t i=0; devList[i]!=NULL && i<MAX_INVERTERS; i++)
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
	inv->multigateID = NaN_U32;
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

E_SBFSPOT logoffMultigateDevices(InverterData* const inverters[])
{
    if (DEBUG_NORMAL) puts("logoffMultigateDevices()");
	for (uint32_t mg=0; inverters[mg]!=NULL && mg<MAX_INVERTERS; mg++)
	{
		InverterData *pmg = inverters[mg];
		if (pmg->SUSyID == SID_MULTIGATE)
		{
			pmg->hasDayData = true;
			for (uint32_t sb240=0; inverters[sb240]!=NULL && sb240<MAX_INVERTERS; sb240++)
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
