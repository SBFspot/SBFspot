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

#include "Defines.h"

int debug = 0;
int verbose = 0;
int quiet = 0;

char DateTimeFormat[32];
char DateFormat[32];
bool hasBatteryDevice = false;
CONNECTIONTYPE ConnType = CT_NONE;

int MAX_CommBuf = 0;
int MAX_pcktBuf = 0;

uint8_t CommBuf[COMMBUFSIZE];
uint8_t pcktBuf[COMMBUFSIZE];
uint8_t RootDeviceAddress[6]= {0, 0, 0, 0, 0, 0};	//Hold byte array with BT address of primary inverter
uint8_t LocalBTAddress[6] = {0, 0, 0, 0, 0, 0};		//Hold byte array with BT address of local adapter
uint16_t pcktID = 1;
int packetposition = 0;
int FCSChecksum = 0xffff;

unsigned long AppSerial = 0;
unsigned int cmdcode = 0;

TagDefs tagdefs = TagDefs();
