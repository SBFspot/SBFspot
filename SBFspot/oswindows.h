/************************************************************************************************
	SBFspot - Yet another tool to read power production of SMA® solar inverters
	(c)2012-2019, SBF

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

#ifndef OSWINDOWS_H_INCLUDED
#define OSWINDOWS_H_INCLUDED

#ifndef WIN32
#error Do Not include oswindows.h on non-windows systems
#endif

#define OS "Windows"

// Ignore some of the warnings
#pragma warning(disable:4996)	// 'strnicmp': The POSIX name for this item is deprecated.
#pragma warning(disable:4482)	// nonstandard extension used: enum 'enum' used in qualified name

#define _USE_32BIT_TIME_T

#include <time.h>
#include <string.h>

typedef __int16 int16_t;
typedef unsigned __int16 uint16_t;
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;

typedef int socklen_t;				

#define sleep(sec) Sleep((sec) * 1000)

char *strptime (const char *buf, const char *format, struct tm *timeptr);

#include <direct.h>	// _mkdir
#include <io.h>		// filelength

typedef unsigned char BYTE;

#define SYM_DEGREE "\370"

#define FOLDER_SEP "\\"

#endif // OSWINDOWS_H_INCLUDED
