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

#include "misc.h"
#include "SBFNet.h"
#include "TagDefs.h"
#include "EventData.h"
#include "Ethernet.h"
#include "Types.h"
#include <vector>
#include <algorithm>
#include <string>
#include "boost_ext.h"
#include "boost/date_time/posix_time/posix_time.hpp"
#include "boost/date_time/local_time/local_time.hpp"
#include "boost/date_time/gregorian/gregorian.hpp"
#include "boost/format.hpp"
#include "nan.h"

#include "Rec40S32.h"

#define NA              "N/A"

// Number of retries when timeout occurs
#define MAX_RETRY       3

//User Group
#define UG_USER         0x07L
#define UG_INSTALLER    0x0AL

//Wellknown SUSyID's
#define SID_MULTIGATE   175
#define SID_SB240       244

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
E_SBFSPOT initialiseSMAConnection(const char *BTAddress, InverterData *inverters[], bool MIS);
E_SBFSPOT ethInitConnection(InverterData *inverters[], std::vector<std::string> IPaddresslist);
void CalcMissingSpot(InverterData *invData);
int DaysInMonth(int month, int year);
int getBT_SignalStrength(InverterData *invData);
void freemem(InverterData *inverters[]);
int GetConfig(Config *cfg);
E_SBFSPOT getInverterData(InverterData *inverters[], enum getInverterDataType type);
int getInverterIndexByAddress(InverterData* const inverters[], uint8_t bt_addr[6]);
E_SBFSPOT getPacket(uint8_t senderaddr[6], int wait4Command);
void HexDump(uint8_t *buf, int count, int radix);
void InvalidArg(char *arg);
bool isCrcValid(uint8_t lb, uint8_t hb);
bool isValidSender(uint8_t senderaddr[6], uint8_t address[6]);
E_SBFSPOT logonSMAInverter(InverterData* const inverters[], long userGroup, const char *password);
E_SBFSPOT logoffSMAInverter(InverterData* const inverter);
E_SBFSPOT logoffMultigateDevices(InverterData* const inverters[]);
int parseCmdline(int argc, char **argv, Config *cfg);
void SayHello(int ShowHelp);
E_SBFSPOT SetPlantTime_V1();
E_SBFSPOT SetPlantTime_V2(time_t ndays, time_t lowerlimit, time_t upperlimit);
E_SBFSPOT ethGetPacket(void);
void resetInverterData(InverterData *inv);
void ShowConfig(Config *cfg);
E_SBFSPOT getDeviceData(InverterData *inv, LriDef lri, uint16_t cmd, Rec40S32 &data);
E_SBFSPOT setDeviceData(InverterData *inv, LriDef lri, uint16_t cmd, Rec40S32 &data);
E_SBFSPOT getDeviceList(InverterData *devList[], int multigateID);

extern uint8_t CommBuf[COMMBUFSIZE];

extern uint8_t pcktBuf[maxpcktBufsize];
extern uint8_t RootDeviceAddress[6];
extern uint8_t LocalBTAddress[6];
extern uint8_t addr_broadcast[6];
extern uint8_t addr_unknown[6];
extern unsigned short pcktID;
extern int packetposition;
extern CONNECTIONTYPE ConnType;
extern const unsigned short AppSUSyID;
extern unsigned long AppSerial;
extern const unsigned short anySUSyID;
extern const unsigned long anySerial;
extern const uint32_t MAX_INVERTERS;

extern const char *IP_Multicast;
extern const char *IP_Inverter;

extern TagDefs tagdefs;

extern char DateTimeFormat[32];
extern char DateFormat[32];
extern bool hasBatteryDevice;
