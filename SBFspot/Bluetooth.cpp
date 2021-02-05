/************************************************************************************************
	SBFspot - Yet another tool to read power production of SMA solar inverters
	(c)2012-2018, SBF

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

#include "misc.h"
#include "bluetooth.h"
#include "Defines.h"
#include "SBFspot.h"

int bytes_in_buffer = 0;
int bufptr = 0;

E_SBFSPOT bthInitConnection(const char *BTAddress, InverterData *inverters[], int MIS)
{
    if (VERBOSE_NORMAL) puts("Initializing...");

    //Generate a Serial Number for application
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
        return bthInitConnection(inverters[0]);
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
    if (bthGetPacket(RootDeviceAddress, 0x02) != E_OK)
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
    if (bthGetPacket(RootDeviceAddress, 0x0A) != E_OK)
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

    if (bthGetPacket(RootDeviceAddress, 0x05) != E_OK)
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

        if (bthGetPacket(RootDeviceAddress, 0x04) != E_OK)
            return E_INIT;

        writePacketHeader(pcktBuf, 0x03, RootDeviceAddress);
        writeShort(pcktBuf, 0x0002);

        writePacketLength(pcktBuf);
        bthSend(pcktBuf);

        if (bthGetPacket(RootDeviceAddress, 0x04) != E_OK)
            return E_INIT;

        writePacketHeader(pcktBuf, 0x03, RootDeviceAddress);
        writeShort(pcktBuf, 0x0001);
        writeByte(pcktBuf, 0x01);

        writePacketLength(pcktBuf);
        bthSend(pcktBuf);

        if (bthGetPacket(RootDeviceAddress, 0x04) != E_OK)
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
            rc = bthGetPacket(RootDeviceAddress, 0xFF);
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
            rc = bthGetPacket(RootDeviceAddress, 0x05);
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
            bthGetPacket(RootDeviceAddress, 0x06);
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
        if (bthGetPacket(addr_unknown, 0x01) != E_OK)
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
E_SBFSPOT bthInitConnection(InverterData* const invData)
{
    //Wait for announcement/broadcast message from PV inverter
    if (bthGetPacket(invData->BTAddress, 2) != E_OK)
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

    if (bthGetPacket(invData->BTAddress, 5) != E_OK)
        return E_INIT;

    //Get local BT address - Added V3.1.5 (bthSetPlantTime)
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

    if (bthGetPacket(invData->BTAddress, 1) != E_OK)
        return E_INIT;

    if (!validateChecksum())
        return E_CHKSUM;

    invData->Serial = get_long(pcktBuf + 57);
    if (VERBOSE_NORMAL) printf("Serial Nr: %08lX (%lu)\n", invData->Serial, invData->Serial);

    logoffSMAInverter(invData);

    return E_OK;
}

E_SBFSPOT bthGetPacket(const unsigned char senderaddr[6], int wait4Command)
{
    if (DEBUG_NORMAL) printf("bthGetPacket(%d)\n", wait4Command);
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
                        if (index >= COMMBUFSIZE)
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

E_SBFSPOT bthSetPlantTime(time_t ndays, time_t lowerlimit, time_t upperlimit)
{
    // If not a Bluetooth connection, just quit
    if (ConnType != CT_BLUETOOTH)
        return E_OK;

    if (DEBUG_NORMAL)
        std::cout <<"bthSetPlantTime()" << std::endl;

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
    E_SBFSPOT rc = bthGetPacket(addr_unknown, 1);

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

int bthGetSignalStrength(InverterData *invData)
{
    writePacketHeader(pcktBuf, 0x03, invData->BTAddress);
    writeByte(pcktBuf,0x05);
    writeByte(pcktBuf,0x00);
    writePacketLength(pcktBuf);
    bthSend(pcktBuf);

    bthGetPacket(invData->BTAddress, 4);

    invData->BT_Signal = (float)pcktBuf[22] * 100.0f / 255.0f;
    return 0;
}

#ifdef WIN32
//http://www.winsocketdotnetworkprogramming.com/winsock2programming/winsock2advancedotherprotocol4p.html
//Windows Sockets Error Codes: http://msdn.microsoft.com/en-us/library/ms740668(v=vs.85).aspx

int bthConnect(const char *btAddr)
{
	WSADATA wsd;
	SOCKADDR_BTH sab;
	int WSALastError = 0;

    if (WSAStartup(MAKEWORD(2, 2), &wsd) != 0)
		return WSAGetLastError();	//Unable to load Winsock

	BT_ADDR addr;
	if (str2ba(btAddr, &addr) != 0)
		return WSAEDESTADDRREQ;

    sock = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
    if (sock == INVALID_SOCKET)
    {
		//Unable to create socket
		WSALastError = WSAGetLastError();
        WSACleanup();
        return WSALastError;
	}

    memset (&sab, 0, sizeof(sab));
    sab.addressFamily  = AF_BTH;
    // Set the btAddr member to a BT_ADDR variable that
    // contains the address of the target device. App
    // can accept the device address as a string but must convert
    // the address and store it in a variable of type BT_ADDR.
    sab.btAddr = addr;

    // If the service identifier is available, then set the
    // serviceClassId member of SOCKADDR_BTH to the GUID of
    // the RFCOMM-based service. In this case, the client
    // performs an SDP query and then uses the resulting server channel.
    // sab.serviceClassId = nguiD;

    // Or If you want to use a hard-coded channel number, set the
    // port member of SOCKADDR_BTH to the server channel number (1-31)
    sab.port = 1;

    // Connect to the Bluetooth socket, created previously
    if (connect(sock, (SOCKADDR *)&sab, sizeof(sab)) == SOCKET_ERROR)
    {
		//Failed to connect
		WSALastError = WSAGetLastError();
        closesocket(sock);
        WSACleanup();
        return WSALastError;
    }

	return 0; //OK - Connected
}

int bthClose()
{
	int rc = 0;
	if (sock != 0)
	{
		closesocket(sock);
		sock = 0;
		rc = WSACleanup();
	}

	return rc;
}

int bthSend(unsigned char *btbuffer)
{
	if (DEBUG_NORMAL) HexDump(btbuffer, packetposition, 10);
    int bytes_sent = send(sock, (const char *)btbuffer, packetposition, 0);
	if (bytes_sent >= 0)
	{
		if (DEBUG_NORMAL) std::cout << bytes_sent << " Bytes sent" << std::endl;
	}
	else
	{
		std::cerr << "send() returned an error: " << errno << std::endl;
	}

    return bytes_sent;
}

int bthSearchDevices()
{
	WSADATA m_data;
	SOCKET s;
	WSAPROTOCOL_INFO protocolInfo;
	int protocolInfoSize;
	WSAQUERYSET querySet, *pResults;
	HANDLE hLookup;
	int result;
	BYTE buffer[1024];
	DWORD bufferLength, flags, addressSize;
	CSADDR_INFO *pCSAddr;
	WCHAR addressAsString[1024];

	puts("Scanning for SMA devices...\n");

	// Load the winsock2 library
	if (WSAStartup(MAKEWORD(2,2), &m_data) == 0)
	{
		// Create a bluetooth socket
		s = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
		if (s == INVALID_SOCKET)
		{
			printf("Failed to get bluetooth socket with error code %ld\n", WSAGetLastError());
			WSACleanup();
			return 1;
		}

		protocolInfoSize = sizeof(protocolInfo);

		// Get the bluetooth device info using getsockopt()
		if (getsockopt(s, SOL_SOCKET, SO_PROTOCOL_INFO, (char*)&protocolInfo, &protocolInfoSize) != 0)
		{
			printf("Failed to get protocol info, error %ld\n", WSAGetLastError());
			WSACleanup();
			return 1;
		}

		// Query set criteria
		memset(&querySet, 0, sizeof(querySet));
		querySet.dwSize = sizeof(querySet);
		querySet.dwNameSpace = NS_BTH;

		// Set the flags for query
		flags = LUP_RETURN_NAME | LUP_CONTAINERS | LUP_RETURN_ADDR | LUP_FLUSHCACHE | LUP_RETURN_TYPE;

		// Start a device in range query...
		result = WSALookupServiceBegin(&querySet, flags, &hLookup);

		if (result != 0)
		{
            printf("Unable to search bluetooth devices: error %ld\n", WSAGetLastError());
			WSACleanup();
			return 1;
		}

		while (result == 0)
		{
			bufferLength = sizeof(buffer);
			pResults = (WSAQUERYSET *)&buffer;

			result = WSALookupServiceNext(hLookup, flags, &bufferLength, pResults);
			if (result == 0)
            {
                // Get the device info, name, address etc
                pCSAddr = (CSADDR_INFO *)pResults->lpcsaBuffer;
                addressSize = sizeof(addressAsString);

                // Print the remote bluetooth device address...
                if (WSAAddressToString(pCSAddr->RemoteAddr.lpSockaddr, pCSAddr->RemoteAddr.iSockaddrLength, &protocolInfo, (LPWSTR)addressAsString, &addressSize) == 0)
				{
					//We're only interrested in SMA devices
					if (wcslen(pResults->lpszServiceInstanceName) > 0)
					{
						if ((_wcsnicmp(pResults->lpszServiceInstanceName, L"SMA", 3) == 0) || (_wcsnicmp(pResults->lpszServiceInstanceName, L"BlueCN", 6) == 0))
						{
							// print BTaddress without ()
							addressAsString[18] = 0;
							printf("%S '%S'\n", addressAsString+1, pResults->lpszServiceInstanceName);
						}
					}
				}
            }
        }

        // Close handle to the device query
        WSALookupServiceEnd(hLookup);
		// Cleanup the winsock library startup
		WSACleanup();
	}

	puts("Done.");
	return 0;
}

/**
      Implementation of str2ba for winsock2.
 */
int str2ba(const char *straddr, BTH_ADDR *btaddr)
{
      int i;
      unsigned int aaddr[6];
      BTH_ADDR tmpaddr = 0;

      if (sscanf(straddr, "%02x:%02x:%02x:%02x:%02x:%02x",
                  &aaddr[0], &aaddr[1], &aaddr[2],
                  &aaddr[3], &aaddr[4], &aaddr[5]) != 6)
            return 1;
      *btaddr = 0;
      for (i = 0; i < 6; i++) {
            tmpaddr = (BTH_ADDR) (aaddr[i] & 0xff);
            *btaddr = ((*btaddr) << 8) + tmpaddr;
      }
      return 0;
}

int setBlockingMode()
{
	unsigned long Mode = 0;
	return ioctlsocket(sock, FIONBIO, &Mode);
}

int setNonBlockingMode()
{
	unsigned long Mode = 1;
	return ioctlsocket(sock, FIONBIO, &Mode);
}

void bthClear()
{
	unsigned char buf[COMMBUFSIZE];

	setNonBlockingMode();

	int numbytes = 0;
	do
	{
		numbytes = recv(sock, (char *)&buf, sizeof(buf), 0);
		if (numbytes == -1)
		{
			numbytes = 0;
			if (DEBUG_HIGHEST)
			{
				if (WSAGetLastError() == WSAEWOULDBLOCK)
					puts("BT buffer is empty.");
				else
					printf("recv() returned an error: %d", WSAGetLastError());
			}
		}
		else
		{
			if (DEBUG_HIGHEST)
			{
				puts("Still some data available in BT buffer...");
				HexDump(buf, numbytes, 10);
			}
		}
	} while (numbytes != 0);

	setBlockingMode();
}

#endif /* WIN32 */

#ifdef linux

struct sockaddr_rc addr = {0};

int bthConnect(const char *btAddr)
{
    int status = 0;
    sock = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);

    addr.rc_family = AF_BLUETOOTH;
    addr.rc_channel = 1;
    str2ba(btAddr, &addr.rc_bdaddr);

    // Connect to Inverter
    status = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
    return(status);
}

int bthClose()
{
	if (sock != 0)
	{
		close(sock);
		sock = 0;
	}
    return 0;
}

int bthSend(unsigned char *btbuffer)
{
	if (DEBUG_NORMAL) HexDump(btbuffer, packetposition, 10);

    int bytes_sent = send(sock, btbuffer, packetposition, 0);

	if (bytes_sent >= 0)
	{
		if (DEBUG_NORMAL) std::cout << bytes_sent << " Bytes sent" << std::endl;
	}
	else
	{
		std::cerr << "send() returned an error: " << errno << std::endl;
	}

    return bytes_sent;
}

int setNonBlockingMode()
{
	int flags = fcntl(sock, F_GETFL, 0);
	return fcntl(sock, F_SETFL, flags | O_NONBLOCK);
}

int setBlockingMode()
{
	int flags = fcntl(sock, F_GETFL, 0);
	return fcntl(sock, F_SETFL, flags & (!O_NONBLOCK));
}

void bthClear()
{
	unsigned char buf[COMMBUFSIZE];

	setNonBlockingMode();

	int numbytes = 0;
	do
	{
		numbytes = recv(sock, (char *)&buf, sizeof(buf), 0);

        if (numbytes == -1)
        {
            numbytes = 0;
            if (errno == EWOULDBLOCK)
            {
                if (DEBUG_HIGHEST)
                    puts("BT buffer is empty.");
            }
            else
                if (DEBUG_HIGHEST)
                    printf("recv() returned an error: %d", errno);
        }
        else if (DEBUG_HIGHEST)
        {
            puts("Still some data available in BT buffer...");
            HexDump(buf, numbytes, 10);
        }
	} while (numbytes != 0);

	setBlockingMode();
}

#endif /* linux */

int bthRead(unsigned char *buf, unsigned int bufsize)
{
    int bytes_read;

    fd_set readfds;

    struct timeval tv;
    tv.tv_sec = BT_TIMEOUT;     //set timeout of reading
    tv.tv_usec = 0;

    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);

    select(sock+1, &readfds, NULL, NULL, &tv);

    if (FD_ISSET(sock, &readfds))       // did we receive anything within BT_TIMEOUT seconds
        bytes_read = recv(sock, (char *)buf, bufsize, 0);
    else
    {
		if (DEBUG_HIGHEST) puts("Timeout reading socket");
        return -1; // E_NODATA
    }

    if ( bytes_read > 0)
    {
        if (bytes_read > MAX_CommBuf)
        {
            MAX_CommBuf = bytes_read;
			if (DEBUG_NORMAL)
				printf("MAX_CommBuf is now %d bytes\n", MAX_CommBuf);
        }
       	if (DEBUG_NORMAL)
			printf("Received %d bytes\n", bytes_read);
    }
    else
	{
		std::cerr << "recv() returned an error: " << errno << std::endl;
        return -1; // E_NODATA
	}

    return bytes_read;
}
