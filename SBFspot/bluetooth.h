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

#ifndef _BLUETOOTH_H_
#define _BLUETOOTH_H_

#include "osselect.h"

#ifdef WIN32

// Ignore warning C4127: conditional expression is constant
#pragma warning(disable: 4127)

/*
When you get a few warnings C4068 (Unknown Pragma), read this:
http://www.curlybrace.com/words/2011/01/17/bluetoothapis-h-broken-in-windows-sdk/
*/
#define deprecate deprecated	// Get rid of warnings C4068 (Unknown Pragma)

//Order is important: WinSock2/BluetoothAPIs/ws2bth.h
#include <WinSock2.h>
#include <BluetoothAPIs.h>
#include <ws2bth.h>

#include <ws2tcpip.h>

//Windows Sockets Error Codes
//http://msdn.microsoft.com/en-us/library/ms740668(v=vs.85).aspx

typedef ULONGLONG BT_ADDR, *PBT_ADDR;

#endif	/* WIN32 */

#ifdef linux
#include <sys/select.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h> //SpeedWire
#include <arpa/inet.h>  //SpeedWire
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>

#endif	/* linux */

#include <stdio.h>
#include <ctype.h>
#include <iostream>

unsigned char char2dec(char ch);
unsigned char hexbyte2dec(char *hex);

#define BT_NUMRETRY 10
#define BT_TIMEOUT  10

extern int packetposition;
extern int MAX_CommBuf;

extern int debug;
extern int verbose;

//Function prototypes
int bthConnect(char *btAddr);
int bthClose();
int bthRead(unsigned char *buf, unsigned int bufsize);
int bthSend(unsigned char *btbuffer);
int setBlockingMode();
int setNonBlockingMode();
void bthClear();

#ifdef WIN32
int str2ba(const char *straddr, BTH_ADDR *btaddr);
int bthSearchDevices();
#endif	/* WIN32 */

#endif /* _BLUETOOTH_H_ */
