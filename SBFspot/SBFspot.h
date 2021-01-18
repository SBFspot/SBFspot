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

#pragma once

#include "misc.h"
#include "SBFNet.h"
#include "TagDefs.h"
#include "EventData.h"
#include "Ethernet.h"
#include "Types.h"
#include <time.h>
#include <vector>
#include <algorithm>
#include <string>
#include "boost_ext.h"
#include "boost/date_time/posix_time/posix_time.hpp"
#include "boost/date_time/local_time/local_time.hpp"
#include "boost/date_time/gregorian/gregorian.hpp"
#include "boost/format.hpp"

#include "Rec40S32.h"

#define NaN_S16	0x8000		                    // "Not a Number" representation for SHORT (converted to 0 by SBFspot)
#define NaN_U16	0xFFFF		                    // "Not a Number" representation for USHORT (converted to 0 by SBFspot)
#define NaN_S32	(int32_t) 0x80000000	        // "Not a Number" representation for LONG (converted to 0 by SBFspot)
#define NaN_U32	(uint32_t)0xFFFFFFFF	        // "Not a Number" representation for ULONG (converted to 0 by SBFspot)
#define NaN_S64	(int64_t) 0x8000000000000000	// "Not a Number" representation for LONGLONG (converted to 0 by SBFspot)
#define NaN_U64	(uint64_t)0xFFFFFFFFFFFFFFFF	// "Not a Number" representation for ULONGLONG (converted to 0 by SBFspot)

#define NA				"N/A"

//User Group
#define	UG_USER			0x07L
#define UG_INSTALLER	0x0AL

//Wellknown SUSyID's
#define SID_MULTIGATE	175
#define SID_SB240		244

#if !defined(ARRAYSIZE)
#define ARRAYSIZE(a) sizeof(a) / sizeof(a[0])
#endif

#define tokWh(value64) (double)(value64)/1000
#define tokW(value32) (float)(value32)/1000
#define toHour(value64) (double)(value64)/3600
#define toAmp(value32) (float)value32/1000
#define toVolt(value32) (float)value32/100
#define toHz(value32) (float)value32/100
#define toTemp(value32) (float)value32/100

//Function prototypes
E_SBFSPOT initialiseSMAConnection(InverterData *invData);
E_SBFSPOT ethInitConnection(InverterData *inverters[], const char *IP_Address);
E_SBFSPOT ethInitConnectionMulti(InverterData *inverters[], std::vector<std::string> IPaddresslist);
void CalcMissingSpot(InverterData *invData);
int DaysInMonth(int month, int year);
int getBT_SignalStrength(InverterData *invData);
void freemem(InverterData *inverters[]);
int GetConfig(Config *cfg);
int getInverterData(InverterData *inverters[], enum getInverterDataType type);
int getInverterIndexByAddress(InverterData* const inverters[], unsigned char bt_addr[6]);
int getInverterIndexBySerial(InverterData *inverters[], unsigned short SUSyID, uint32_t Serial);
int getInverterIndexBySerial(InverterData *inverters[], uint32_t Serial);
E_SBFSPOT getPacket(unsigned char senderaddr[6], int wait4Command);
void HexDump(unsigned char *buf, int count, int radix);
E_SBFSPOT initialiseSMAConnection(const char *BTAddress, InverterData *inverters[], int MIS);
void InvalidArg(char *arg);
int isCrcValid(unsigned char lb, unsigned char hb);
int isValidSender(unsigned char senderaddr[6], unsigned char address[6]);
E_SBFSPOT logonSMAInverter(InverterData* const inverters[], long userGroup, const char *password);
E_SBFSPOT logoffSMAInverter(InverterData* const inverter);
E_SBFSPOT logoffMultigateDevices(InverterData* const inverters[]);
int parseCmdline(int argc, char **argv, Config *cfg);
void SayHello(int ShowHelp);
E_SBFSPOT SetPlantTime(time_t ndays, time_t lowerlimit = 0, time_t upperlimit = 0);
E_SBFSPOT ethGetPacket(void);
void resetInverterData(InverterData *inv);
void ShowConfig(Config *cfg);
E_SBFSPOT getDeviceData(InverterData *inv, LriDef lri, uint16_t cmd, Rec40S32 &data);
E_SBFSPOT setDeviceData(InverterData *inv, LriDef lri, uint16_t cmd, Rec40S32 &data);
E_SBFSPOT getDeviceList(InverterData *devList[], int multigateID);

extern unsigned char CommBuf[COMMBUFSIZE];

extern BYTE pcktBuf[maxpcktBufsize];
extern unsigned char RootDeviceAddress[6];
extern unsigned char LocalBTAddress[6];
extern unsigned char addr_broadcast[6];
extern unsigned char addr_unknown[6];
extern unsigned short pcktID;
extern int packetposition;
extern CONNECTIONTYPE ConnType;
extern unsigned short AppSUSyID;
extern unsigned long AppSerial;
extern const unsigned short anySUSyID;
extern const unsigned long anySerial;
extern const uint32_t MAX_INVERTERS;

extern const char *IP_Broadcast;
extern const char *IP_Inverter;

extern TagDefs tagdefs;

extern char DateTimeFormat[32];
extern char DateFormat[32];
extern bool hasBatteryDevice;
