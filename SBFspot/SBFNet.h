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

#ifndef _SMANET_H_
#define _SMANET_H_

#include "osselect.h"
#include "bluetooth.h"

//#define maxpcktBufsize 520
#define maxpcktBufsize COMMBUFSIZE

#define BTH_L2SIGNATURE 0x656003FF
#define ETH_L2SIGNATURE 0x65601000

//Function prototypes
//void ethWritePacketHeader(unsigned char *buf);
//void ethWritePacket(unsigned char *buf, unsigned char ctrl, unsigned short ctrl2, unsigned short dstSUSyID, unsigned long dstSerial);

void writeLong(unsigned char *btbuffer, const unsigned long v);
void writeShort(unsigned char *btbuffer, const unsigned short v);
void writeByte(unsigned char *btbuffer, const unsigned char v);
void writeArray(unsigned char *btbuffer, const unsigned char bytes[], const int count);
//void writePacket(unsigned char *btbuffer, const unsigned char size, const unsigned char ctrl, const unsigned short dstSUSyID, const unsigned long dstSerial, const unsigned short packetcount, const unsigned char a, const unsigned char b, const unsigned char c);
void writePacket(unsigned char *buf, unsigned char longwords, unsigned char ctrl, unsigned short ctrl2, unsigned short dstSUSyID, unsigned long dstSerial);
void writePacketTrailer(unsigned char *btbuffer);
void writePacketHeader(unsigned char *btbuffer, const unsigned int control, const unsigned char *destaddress);
void writePacketLength(unsigned char *buffer);
int validateChecksum(void);
short get_short(unsigned char *buf);
int32_t get_long(unsigned char *buf);
int64_t get_longlong(unsigned char *buf);

#endif
