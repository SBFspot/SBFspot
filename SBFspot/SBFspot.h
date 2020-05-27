/************************************************************************************************
	SBFspot - Yet another tool to read power production of SMA® solar inverters
	(c)2012-2020, SBF

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

#pragma once

#include "misc.h"
#include "SBFNet.h"
#include "TagDefs.h"
#include "EventData.h"
#include "Ethernet.h"
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

typedef struct
{
    time_t datetime;
    long long totalWh;	// changed to signed - issue 58
    long long dayWh;	// changed to signed - issue 58
} MonthData;

typedef struct
{
    time_t datetime;
    long long totalWh;	// changed to signed - issue 58
    long long watt;		// changed to signed - issue 58
} DayData;

typedef struct
{
	unsigned short code;
	const char *meta;
	const char *fullText;
}
CodeToMeta;

enum getInverterDataType
{
	EnergyProduction	= 1 << 0,
	SpotDCPower			= 1 << 1,
	SpotDCVoltage		= 1 << 2,
	SpotACPower			= 1 << 3,
	SpotACVoltage		= 1 << 4,
	SpotGridFrequency	= 1 << 5,
	MaxACPower			= 1 << 6,
	MaxACPower2			= 1 << 7,
	SpotACTotalPower	= 1 << 8,
	TypeLabel			= 1 << 9,
	OperationTime		= 1 << 10,
	SoftwareVersion		= 1 << 11,
	DeviceStatus		= 1 << 12,
	GridRelayStatus		= 1 << 13,
	BatteryChargeStatus = 1 << 14,
	BatteryInfo         = 1 << 15,
	InverterTemperature	= 1 << 16,
	MeteringGridMsTotW	= 1 << 17,

	sbftest             = 1 << 31
};

typedef enum
{
	S123_NOP = 0,	// Nop
	S123_DATA = 1,	// Send spot data frame
	S123_INFO = 2,	// Send program/inverter information
	S123_SYNC = 3,	// Synchronize inverter
	S123_STATE = 4	// Send inverter state data
} S123_COMMAND;

typedef enum
{
    AllDevices = 8000,          // DevClss0
    SolarInverter = 8001,       // DevClss1
    WindTurbineInverter = 8002, // DevClss2
    BatteryInverter = 8007,     // DevClss7
    Consumer = 8033,            // DevClss33
    SensorSystem = 8064,        // DevClss64
    ElectricityMeter = 8065,    // DevClss65
    CommunicationProduct = 8128 // DevClss128
} DEVICECLASS;

typedef enum
{
	DT_ULONG = 0,
	DT_STATUS = 8,
	DT_STRING = 16,
	DT_FLOAT = 32,
	DT_SLONG = 64
} SMA_DATATYPE;

typedef struct
{
    char DeviceName[33];    //32 bytes + terminating zero
	unsigned char BTAddress[6];
	char IPAddress[20];
	unsigned short SUSyID;
    unsigned long Serial;
	unsigned char NetID;
	float BT_Signal;
    time_t InverterDatetime;
    time_t WakeupTime;
    time_t SleepTime;
    long Pdc1;
    long Pdc2;
    long Udc1;
    long Udc2;
    long Idc1;
    long Idc2;
    long Pmax1;
    long Pmax2;
    long Pmax3;
    long TotalPac;
    long Pac1;
    long Pac2;
    long Pac3;
    long Uac1;
    long Uac2;
    long Uac3;
    long Iac1;
    long Iac2;
    long Iac3;
    long GridFreq;
	long long OperationTime;
    long long FeedInTime;
    long long EToday;
    long long ETotal;
	unsigned short modelID;
	char DeviceType[64];
	char DeviceClass[64];
	DEVICECLASS DevClass;
	char SWVersion[16];	//"03.01.05.R"
	int DeviceStatus;
	int GridRelayStatus;
	int flags;
	DayData dayData[288];
	bool hasDayData;
	MonthData monthData[31];
	bool hasMonthData;
	time_t monthDataOffset;	// Issue 115
	std::vector<EventData> eventData;
	long calPdcTot;
	long calPacTot;
	float calEfficiency;
	unsigned long BatChaStt;			// Current battery charge status
    unsigned long BatDiagCapacThrpCnt;	// Number of battery charge throughputs
    unsigned long BatDiagTotAhIn;		// Amp hours counter for battery charge
    unsigned long BatDiagTotAhOut;		// Amp hours counter for battery discharge
    unsigned long BatTmpVal;			// Battery temperature
    unsigned long BatVol;				// Battery voltage
    long BatAmp;						// Battery current
	long Temperature;					// Inverter Temperature
	int32_t	MeteringGridMsTotWOut;		// Power grid feed-in (Out)
	int32_t MeteringGridMsTotWIn;		// Power grid reference (In)
	bool hasBattery;					// Smart Energy device
	int logonStatus;
	int multigateID;
} InverterData;

typedef enum
{
    CT_NONE = 0,
    CT_BLUETOOTH = 1,
    CT_ETHERNET  = 2
} CONNECTIONTYPE;

typedef struct
{
	std::string	ConfigFile;			//Fullpath to configuration file
	std::string	AppPath;
    char	BT_Address[18];			//Inverter bluetooth address 12:34:56:78:9A:BC
    char	IP_Address[16];			//Inverter IP address 192.168.178.123 (for Speedwirecommunication)
	std::vector<std::string> ip_addresslist; //List of Inverter IP addresses (for Speedwirecommunication )
    int		BT_Timeout;
	int		BT_ConnectRetries;
	short   IP_Port;
	CONNECTIONTYPE ConnectionType;     // CT_BLUETOOTH | CT_ETHERNET
    char	SMA_Password[13];
    float	latitude;
    float	longitude;
    time_t	archdata_from;
    time_t	archdata_to;
    char	delimiter;			//CSV field delimiter
    int		precision;			//CSV value precision
    char	decimalpoint;		//CSV decimal point
    char	outputPath[MAX_PATH];
    char	outputPath_Events[MAX_PATH];
    char	plantname[32];
    std::string sqlDatabase;
    std::string sqlHostname;
    std::string sqlUsername;
    std::string sqlUserPassword;
	int		synchTime;				// 1=Synch inverter time with computer time (default=0)
	float	sunrise;
	float	sunset;
	int		isLight;
	int		calcMissingSpot;		// 0-1
	char	DateTimeFormat[32];
	char	DateFormat[32];
	char	TimeFormat[32];
	int		CSV_Export;
	int		CSV_Header;
	int		CSV_ExtendedHeader;
	int		CSV_SaveZeroPower;
	int		SunRSOffset;			// Offset to start before sunrise and end after sunset
	int		userGroup;				// USER|INSTALLER
	char	prgVersion[16];
	int		SpotTimeSource;			// 0=Use inverter time; 1=Use PC time in Spot CSV
	int		SpotWebboxHeader;		// 0=Use standard Spot CSV hdr; 1=Webbox style hdr
	char	locale[6];				// default en-US
	int		MIS_Enabled;			// Multi Inverter Support
	std::string	timezone;
	boost::local_time::time_zone_ptr tz;
	int		synchTimeLow;			// settime low limit
	int		synchTimeHigh;			// settime high limit
	// MQTT Stuff -- Using mosquitto (https://mosquitto.org/)
	std::string mqtt_publish_exe;	// default /usr/bin/mosquitto_pub ("%ProgramFiles%\mosquitto\mosquitto_pub.exe" on Windows)
	std::string mqtt_host;			// default localhost
	std::string mqtt_port;			// default 1883 (8883 for MQTT over TLS)
	std::string mqtt_topic;			// default sbfspot
	std::string mqtt_publish_args;	// default and optional arguments for mosquitto_pub (-d for debug messages)
	std::string mqtt_publish_data;	// comma delimited list of spot data to publish (Timestamp,Serial,MeteringDyWhOut,GridMsTotW,...)
	std::string mqtt_item_format;   // default "{key}": {value}
	std::string mqtt_item_delimiter;// default comma

	//Commandline settings
	int		debug;				// -d			Debug level (0-5)
	int		verbose;			// -v			Verbose output level (0-5)
	int		archDays;			// -ad			Number of days back to get Archived DayData (0=disabled, 1=today, ...)
	int		archMonths;			// -am			Number of months back to get Archived MonthData (0=disabled, 1=this month, ...)
	int		archEventMonths;	// -ae			Number of months back to get Archived Events (0=disabled, 1=this month, ...)
	int		forceInq;			// -finq		Inquire inverter also during the night
	int		wsl;				// -wsl			WebSolarLog support (http://www.websolarlog.com/index.php/tag/sma-spot/)
	int		quiet;				// -q			Silent operation (No output except for -wsl)
	int		nocsv;				// -nocsv		Disables CSV export (Overrules CSV_Export in config)
	int		nospot;				// -sp0			Disables Spot CSV export
	int		nosql;				// -nosql		Disables SQL export
	int		loadlive;			// -loadlive	Force settings to prepare for live loading to http://pvoutput.org/loadlive.jsp
	time_t	startdate;			// -startdate	Start reading of historic data at the given date (YYYYMMDD)
    S123_COMMAND	s123;		// -123s		123Solar Web Solar logger support(http://www.123solar.org/)
	int		settime;			// -settime		Set plant time
	int		mqtt;				// -mqtt		Publish spot data to mqtt broker
} Config;


//SMA Structs must be aligned on byte boundaries
#pragma pack(push, 1)
typedef struct PacketHeader
{
    unsigned char  SOP;					// Start Of Packet (0x7E)
    unsigned short pkLength;
    unsigned char  pkChecksum;
    unsigned char  SourceAddr[6];		// SMA Inverter Address
    unsigned char  DestinationAddr[6];	// Local BT Address
    unsigned short command;
} pkHeader;

typedef struct
{
    uint32_t      MagicNumber;      // Packet signature 53 4d 41 00 (SMA\0)
    uint32_t      unknown1;         // 00 04 02 a0
    uint32_t      unknown2;         // 00 00 00 01
    unsigned char hiPacketLen;      // Packet length stored as big endian
    unsigned char loPacketLen ;     // Packet length Low Byte
} ethPacketHeaderL1;

typedef struct
{
    uint32_t      MagicNumber;      // Level 2 packet signature 00 10 60 65
    unsigned char longWords;        // int(PacketLen/4)
    unsigned char ctrl;
} ethPacketHeaderL2;

typedef struct
{
    ethPacketHeaderL1 pcktHdrL1;
    ethPacketHeaderL2 pcktHdrL2;
} ethPacketHeaderL1L2;

typedef struct
{
    unsigned short SUSyID;
    uint32_t       Serial;
    unsigned short Ctrl;
} ethEndpoint;

typedef struct
{
    unsigned char dummy0;
    ethPacketHeaderL2 pcktHdrL2;
    ethEndpoint Destination;
    ethEndpoint Source;
    unsigned short ErrorCode;
    unsigned short FragmentID;  //Count Down
    unsigned short PacketID;    //Count Up
} ethPacket;

typedef struct ArchiveDataRec
{
    time_t datetime;
    unsigned long long totalWh;
} ArchDataRec;

#pragma pack(pop)

typedef unsigned char BYTE;

typedef enum
{
    OperationHealth                 = 0x00214800,   // *08* Condition (aka INV_STATUS)
	CoolsysTmpNom					= 0x00237700,	// *40* Operating condition temperatures
    DcMsWatt                        = 0x00251E00,   // *40* DC power input (aka SPOT_PDC1 / SPOT_PDC2)
    MeteringTotWhOut                = 0x00260100,   // *00* Total yield (aka SPOT_ETOTAL)
    MeteringDyWhOut                 = 0x00262200,   // *00* Day yield (aka SPOT_ETODAY)
    GridMsTotW                      = 0x00263F00,   // *40* Power (aka SPOT_PACTOT)
    BatChaStt                       = 0x00295A00,   // *00* Current battery charge status
    OperationHealthSttOk            = 0x00411E00,   // *00* Nominal power in Ok Mode (aka INV_PACMAX1)
    OperationHealthSttWrn           = 0x00411F00,   // *00* Nominal power in Warning Mode (aka INV_PACMAX2)
    OperationHealthSttAlm           = 0x00412000,   // *00* Nominal power in Fault Mode (aka INV_PACMAX3)
    OperationGriSwStt               = 0x00416400,   // *08* Grid relay/contactor (aka INV_GRIDRELAY)
    OperationRmgTms                 = 0x00416600,   // *00* Waiting time until feed-in
    DcMsVol                         = 0x00451F00,   // *40* DC voltage input (aka SPOT_UDC1 / SPOT_UDC2)
    DcMsAmp                         = 0x00452100,   // *40* DC current input (aka SPOT_IDC1 / SPOT_IDC2)
    MeteringPvMsTotWhOut            = 0x00462300,   // *00* PV generation counter reading
    MeteringGridMsTotWhOut          = 0x00462400,   // *00* Grid feed-in counter reading
    MeteringGridMsTotWhIn           = 0x00462500,   // *00* Grid reference counter reading
    MeteringCsmpTotWhIn             = 0x00462600,   // *00* Meter reading consumption meter
    MeteringGridMsDyWhOut	        = 0x00462700,   // *00* ?
    MeteringGridMsDyWhIn            = 0x00462800,   // *00* ?
    MeteringTotOpTms                = 0x00462E00,   // *00* Operating time (aka SPOT_OPERTM)
    MeteringTotFeedTms              = 0x00462F00,   // *00* Feed-in time (aka SPOT_FEEDTM)
    MeteringGriFailTms              = 0x00463100,   // *00* Power outage
    MeteringWhIn                    = 0x00463A00,   // *00* Absorbed energy
    MeteringWhOut                   = 0x00463B00,   // *00* Released energy
    MeteringPvMsTotWOut             = 0x00463500,   // *40* PV power generated
    MeteringGridMsTotWOut           = 0x00463600,   // *40* Power grid feed-in
    MeteringGridMsTotWIn            = 0x00463700,   // *40* Power grid reference
    MeteringCsmpTotWIn              = 0x00463900,   // *40* Consumer power
    GridMsWphsA                     = 0x00464000,   // *40* Power L1 (aka SPOT_PAC1)
    GridMsWphsB                     = 0x00464100,   // *40* Power L2 (aka SPOT_PAC2)
    GridMsWphsC                     = 0x00464200,   // *40* Power L3 (aka SPOT_PAC3)
    GridMsPhVphsA                   = 0x00464800,   // *00* Grid voltage phase L1 (aka SPOT_UAC1)
    GridMsPhVphsB                   = 0x00464900,   // *00* Grid voltage phase L2 (aka SPOT_UAC2)
    GridMsPhVphsC                   = 0x00464A00,   // *00* Grid voltage phase L3 (aka SPOT_UAC3)
    GridMsAphsA_1                   = 0x00465000,   // *00* Grid current phase L1 (aka SPOT_IAC1)
    GridMsAphsB_1                   = 0x00465100,   // *00* Grid current phase L2 (aka SPOT_IAC2)
    GridMsAphsC_1                   = 0x00465200,   // *00* Grid current phase L3 (aka SPOT_IAC3)
    GridMsAphsA                     = 0x00465300,   // *00* Grid current phase L1 (aka SPOT_IAC1_2)
    GridMsAphsB                     = 0x00465400,   // *00* Grid current phase L2 (aka SPOT_IAC2_2)
    GridMsAphsC                     = 0x00465500,   // *00* Grid current phase L3 (aka SPOT_IAC3_2)
    GridMsHz                        = 0x00465700,   // *00* Grid frequency (aka SPOT_FREQ)
    MeteringSelfCsmpSelfCsmpWh      = 0x0046AA00,   // *00* Energy consumed internally
    MeteringSelfCsmpActlSelfCsmp    = 0x0046AB00,   // *00* Current self-consumption
    MeteringSelfCsmpSelfCsmpInc     = 0x0046AC00,   // *00* Current rise in self-consumption
    MeteringSelfCsmpAbsSelfCsmpInc  = 0x0046AD00,   // *00* Rise in self-consumption
    MeteringSelfCsmpDySelfCsmpInc   = 0x0046AE00,   // *00* Rise in self-consumption today
    BatDiagCapacThrpCnt             = 0x00491E00,   // *40* Number of battery charge throughputs
    BatDiagTotAhIn                  = 0x00492600,   // *00* Amp hours counter for battery charge
    BatDiagTotAhOut                 = 0x00492700,   // *00* Amp hours counter for battery discharge
    BatTmpVal                       = 0x00495B00,   // *40* Battery temperature
    BatVol                          = 0x00495C00,   // *40* Battery voltage
    BatAmp                          = 0x00495D00,   // *40* Battery current
    NameplateLocation               = 0x00821E00,   // *10* Device name (aka INV_NAME)
    NameplateMainModel              = 0x00821F00,   // *08* Device class (aka INV_CLASS)
    NameplateModel                  = 0x00822000,   // *08* Device type (aka INV_TYPE)
    NameplateAvalGrpUsr             = 0x00822100,   // *  * Unknown
    NameplatePkgRev                 = 0x00823400,   // *08* Software package (aka INV_SWVER)
    InverterWLim                    = 0x00832A00,   // *00* Maximum active power device (aka INV_PACMAX1_2) (Some inverters like SB3300/SB1200)
	GridMsPhVphsA2B6100             = 0x00464B00,
	GridMsPhVphsB2C6100             = 0x00464C00,
	GridMsPhVphsC2A6100             = 0x00464D00
} LriDef;

#define NaN_S16	0x8000		                    // "Not a Number" representation for SHORT (converted to 0 by SBFspot)
#define NaN_U16	0xFFFF		                    // "Not a Number" representation for USHORT (converted to 0 by SBFspot)
#define NaN_S32	(int32_t) 0x80000000	        // "Not a Number" representation for LONG (converted to 0 by SBFspot)
#define NaN_U32	(uint32_t)0xFFFFFFFF	        // "Not a Number" representation for ULONG (converted to 0 by SBFspot)
#define NaN_S64	(int64_t) 0x8000000000000000	// "Not a Number" representation for LONGLONG (converted to 0 by SBFspot)
#define NaN_U64	(uint64_t)0xFFFFFFFFFFFFFFFF	// "Not a Number" representation for ULONGLONG (converted to 0 by SBFspot)

#define NA				"N/A"

typedef enum
{
	E_OK			=  0,
	E_NODATA		= -1,	// Bluetooth buffer empty
	E_BADARG		= -2,	// Unknown command line argument
	E_CHKSUM		= -3,	// Invalid Checksum
	E_BUFOVRFLW		= -4,	// Buffer overflow
	E_ARCHNODATA	= -5,	// No archived data found for given timespan
	E_INIT			= -6,	// Unable to initialize
	E_INVPASSW		= -7,	// Invalid password
	E_RETRY			= -8,	// Retry the last action
	E_EOF			= -9,	// End of data
	E_PRIVILEGE		= -10,	// Privilege not held (need installer login)
	E_LOGONFAILED	= -11,	// Logon failed, other than Invalid Password (E_INVPASSW)
	E_COMM			= -12	// General communication error
} E_SBFSPOT;

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
E_SBFSPOT ethInitConnection(InverterData *inverters[], char *IP_Address);
E_SBFSPOT ethInitConnectionMulti(InverterData *inverters[], std::vector<std::string> IPaddresslist);
void CalcMissingSpot(InverterData *invData);
int DaysInMonth(int month, int year);
int getBT_SignalStrength(InverterData *invData);
void freemem(InverterData *inverters[]);
int GetConfig(Config *cfg);
const char *getEventCategory(unsigned short eFlags);
const char *getEventGroup(unsigned long eGroup);
const char *getEventType(unsigned short eventflags);
int getInverterData(InverterData *inverters[], enum getInverterDataType type);
int getInverterIndexByAddress(InverterData *inverters[], unsigned char bt_addr[6]);
int getInverterIndexBySerial(InverterData *inverters[], unsigned short SUSyID, uint32_t Serial);
int getInverterIndexBySerial(InverterData *inverters[], uint32_t Serial);
E_SBFSPOT getPacket(unsigned char senderaddr[6], int wait4Command);
//int get_tzOffset(void);
void HexDump(unsigned char *buf, int count, int radix);
E_SBFSPOT initialiseSMAConnection(const char *BTAddress, InverterData *inverters[], int MIS);
void InvalidArg(char *arg);
int isCrcValid(unsigned char lb, unsigned char hb);
int isValidSender(unsigned char senderaddr[6], unsigned char address[6]);
E_SBFSPOT logonSMAInverter(InverterData *inverters[], long userGroup, char *password);
E_SBFSPOT logoffSMAInverter(InverterData *inverter);
E_SBFSPOT logoffMultigateDevices(InverterData *inverters[]);
int parseCmdline(int argc, char **argv, Config *cfg);
void printHexBytes(BYTE *buf, int num);
void SayHello(int ShowHelp);
E_SBFSPOT SetPlantTime(time_t ndays, time_t lowerlimit = 0, time_t upperlimit = 0);
E_SBFSPOT ethGetPacket(void);
void resetInverterData(InverterData *inv);
void ShowConfig(Config *cfg);
E_SBFSPOT getInverterWMax(InverterData *inv, Rec40S32 &data);
E_SBFSPOT setInverterWMax(InverterData *inv, Rec40S32 &data);
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
extern const int MAX_INVERTERS;

extern const char *IP_Broadcast;
extern const char *IP_Inverter;

extern TagDefs tagdefs;

