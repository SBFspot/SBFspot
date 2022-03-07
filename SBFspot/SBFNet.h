/************************************************************************************************
    SBFspot - Yet another tool to read power production of SMA solar inverters
    (c)2012-2022, SBF

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

#pragma once

#include "osselect.h"
#include "bluetooth.h"

#define maxpcktBufsize COMMBUFSIZE

#define BTH_L2SIGNATURE 0x656003FF
#define ETH_L2SIGNATURE 0x65601000

//Function prototypes
void writeLong(uint8_t *btbuffer, uint32_t v);
void writeShort(uint8_t *btbuffer, uint16_t v);
void writeByte(uint8_t *btbuffer, uint8_t v);
void writeArray(uint8_t *btbuffer, const uint8_t bytes[], int count);
void writePacket(uint8_t *buf, uint8_t longwords, uint8_t ctrl, unsigned short ctrl2, unsigned short dstSUSyID, unsigned long dstSerial);
void writePacketTrailer(uint8_t *btbuffer);
void writePacketHeader(uint8_t *btbuffer, unsigned int control, const uint8_t *destaddress);
void writePacketLength(uint8_t *buffer);
int validateChecksum(void);
short get_short(uint8_t *buf);
int32_t get_long(uint8_t *buf);
int64_t get_longlong(uint8_t *buf);
uint32_t genSessionID();
