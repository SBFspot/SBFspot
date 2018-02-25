/************************************************************************************************
	SBFspot - Yet another tool to read power production of SMA® solar inverters
	(c)2012-2018, SBF

	Latest version found at https://github.com/SBFspot/SBFspot

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

	SMA is a registered trademark of SMA Solar Technology AG

************************************************************************************************/

#include "misc.h"
#include "SBFNet.h"
#include "SBFspot.h"
#include <stdio.h>
#include <string.h>

typedef unsigned char BYTE;

unsigned char  RootDeviceAddress[6]= {0, 0, 0, 0, 0, 0};	//Hold byte array with BT address of primary inverter
unsigned char  LocalBTAddress[6] = {0, 0, 0, 0, 0, 0};		//Hold byte array with BT address of local adapter
unsigned char  addr_broadcast[6] = {0, 0, 0, 0, 0, 0};
unsigned char  addr_unknown[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
unsigned short AppSUSyID;
unsigned long  AppSerial;
const unsigned short anySUSyID = 0xFFFF;
const unsigned long anySerial = 0xFFFFFFFF;

unsigned int cmdcode = 0;

int packetposition = 0;
int FCSChecksum = 0xffff;

unsigned short pcktID = 1;

BYTE pcktBuf[maxpcktBufsize];

const unsigned short fcstab[256] =
{
    0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf, 0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
    0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e, 0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
    0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd, 0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
    0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c, 0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
    0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb, 0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
    0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a, 0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
    0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9, 0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
    0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738, 0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
    0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7, 0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
    0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036, 0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
    0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5, 0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
    0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134, 0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
    0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3, 0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
    0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232, 0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
    0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1, 0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
    0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330, 0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
};

void writeLong(BYTE *btbuffer, unsigned long v)
{
    writeByte(btbuffer,(unsigned char)((v >> 0) & 0xFF));
    writeByte(btbuffer,(unsigned char)((v >> 8) & 0xFF));
    writeByte(btbuffer,(unsigned char)((v >> 16) & 0xFF));
    writeByte(btbuffer,(unsigned char)((v >> 24) & 0xFF));
}

void writeShort(BYTE *btbuffer, unsigned short v)
{
    writeByte(btbuffer,(unsigned char)((v >> 0) & 0xFF));
    writeByte(btbuffer,(unsigned char)((v >> 8) & 0xFF));
}

void writeByte(unsigned char *btbuffer, unsigned char v)
{
    if (ConnType == CT_BLUETOOTH)
    {
        //Keep a rolling checksum over the payload
        FCSChecksum = (FCSChecksum >> 8) ^ fcstab[(FCSChecksum ^ v) & 0xff];
        if (v == 0x7d || v == 0x7e || v == 0x11 || v == 0x12 || v == 0x13)
        {
            btbuffer[packetposition++] = 0x7d;
            btbuffer[packetposition++] = v ^ 0x20;
        }
        else
        {
            btbuffer[packetposition++] = v;
        }
    }
    else    // CT_ETHERNET
        btbuffer[packetposition++] = v;
}

void writeArray(unsigned char *btbuffer, const unsigned char bytes[], int loopcount)
{
    for (int i = 0; i < loopcount; i++)
    {
        writeByte(btbuffer, bytes[i]);
    }
}

void writePacket(unsigned char *buf, unsigned char longwords, unsigned char ctrl, unsigned short ctrl2, unsigned short dstSUSyID, unsigned long dstSerial)
{
	if (ConnType == CT_BLUETOOTH)
	{
        buf[packetposition++] = 0x7E;   //Not included in checksum
        writeLong(buf, BTH_L2SIGNATURE);
	}
	else
        writeLong(buf, ETH_L2SIGNATURE);

    writeByte(buf, longwords);
    writeByte(buf, ctrl);
    writeShort(buf, dstSUSyID);
    writeLong(buf, dstSerial);
    writeShort(buf, ctrl2);
    writeShort(buf, AppSUSyID);
    writeLong(buf, AppSerial);
    writeShort(buf, ctrl2);
    writeShort(buf, 0);
    writeShort(buf, 0);
    writeShort(buf, pcktID | 0x8000);
}

void writePacketTrailer(unsigned char *btbuffer)
{
   	if (ConnType == CT_BLUETOOTH)
   	{
        FCSChecksum ^= 0xFFFF;
        btbuffer[packetposition++] = FCSChecksum & 0x00FF;
        btbuffer[packetposition++] = (FCSChecksum >> 8) & 0x00FF;
        btbuffer[packetposition++] = 0x7E;  //Trailing byte
   	}
   	else
        writeLong(btbuffer, 0);
}

void writePacketHeader(unsigned char *buf, const unsigned int control, const unsigned char *destaddress)
{
	packetposition = 0;

    if (ConnType == CT_BLUETOOTH)
    {
        FCSChecksum = 0xFFFF;

        buf[packetposition++] = 0x7E;
        buf[packetposition++] = 0;  //placeholder for len1
        buf[packetposition++] = 0;  //placeholder for len2
        buf[packetposition++] = 0;  //placeholder for checksum

        int i;
        for(i = 0; i < 6; i++)
            buf[packetposition++] = LocalBTAddress[i];

        for(i = 0; i < 6; i++)
            buf[packetposition++] = destaddress[i];

        buf[packetposition++] = (BYTE)(control & 0xFF);
        buf[packetposition++] = (BYTE)(control >> 8);

        cmdcode = 0xFFFF;  //Just set to dummy value
    }
    else
    {
        //Ignore control and destaddress
        writeLong(buf, 0x00414D53);  // SMA\0
        writeLong(buf, 0xA0020400);
        writeLong(buf, 0x01000000);
        writeByte(buf, 0);
        writeByte(buf, 0);          // Placeholder for packet length
    }
}

void writePacketLength(unsigned char *buf)
{
    if (ConnType == CT_BLUETOOTH)
    {
        buf[1] = packetposition & 0xFF;		    //Lo-Byte
        buf[2] = (packetposition >> 8) & 0xFF;	//Hi-Byte
        buf[3] = buf[0] ^ buf[1] ^ buf[2];      //checksum
    }
    else
    {
        short dataLength = (short)(packetposition - sizeof(ethPacketHeaderL1L2));
        ethPacketHeaderL1L2 *hdr = (ethPacketHeaderL1L2 *)buf;
        hdr->pcktHdrL1.hiPacketLen = (dataLength >> 8) & 0xFF;
        hdr->pcktHdrL1.loPacketLen = dataLength & 0xFF;
    }
}

int validateChecksum()
{
    FCSChecksum = 0xffff;
    //Skip over 0x7e at start and end of packet
    int i;
    for(i = 1; i <= packetposition - 4; i++)
    {
        FCSChecksum = (FCSChecksum >> 8) ^ fcstab[(FCSChecksum ^ pcktBuf[i]) & 0xff];
    }

    FCSChecksum ^= 0xffff;

	if (get_short(pcktBuf + packetposition - 3) == (short)FCSChecksum)
        return true;
    else
    {
		if (DEBUG_HIGH) printf("Invalid chk 0x%04X - Found 0x%02X%02X\n", FCSChecksum, pcktBuf[packetposition-2], pcktBuf[packetposition-3]);
		return false;
    }
}

int getBT_SignalStrength(InverterData *invData)
{
	writePacketHeader(pcktBuf, 0x03, invData->BTAddress);
    writeByte(pcktBuf,0x05);
    writeByte(pcktBuf,0x00);
    writePacketLength(pcktBuf);
    bthSend(pcktBuf);

	getPacket(invData->BTAddress, 4);

	invData->BT_Signal = (float)pcktBuf[22] * 100.0f / 255.0f;
    return 0;
}

int64_t get_longlong(BYTE *buf)
{
    register int64_t lnglng = 0;

	lnglng += *(buf+7);
	lnglng <<= 8;
	lnglng += *(buf+6);
	lnglng <<= 8;
	lnglng += *(buf+5);
	lnglng <<= 8;
	lnglng += *(buf+4);
	lnglng <<= 8;
	lnglng += *(buf+3);
	lnglng <<= 8;
	lnglng += *(buf+2);
	lnglng <<= 8;
	lnglng += *(buf+1);
	lnglng <<= 8;
	lnglng += *(buf);

    return lnglng;
}

int32_t get_long(BYTE *buf)
{
    register int32_t lng = 0;

	lng += *(buf+3);
	lng <<= 8;
	lng += *(buf+2);
	lng <<= 8;
	lng += *(buf+1);
	lng <<= 8;
	lng += *(buf);

    return lng;
}

short get_short(BYTE *buf)
{
    register short shrt = 0;

	shrt += *(buf+1);
	shrt <<= 8;
	shrt += *(buf);

    return shrt;
}

