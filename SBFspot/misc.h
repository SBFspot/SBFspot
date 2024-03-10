/************************************************************************************************
    SBFspot - Yet another tool to read power production of SMA solar inverters
    (c)2012-2024, SBF

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
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string>
#include <vector>

#define COMMBUFSIZE 2048 // Size of Communications Buffer (Bluetooth/Ethernet)

#ifndef MAX_PATH
#define MAX_PATH          260
#endif

std::string strfgmtime_t(const char *format, const time_t rawtime);
std::string strftime_t(const char *format, const time_t rawtime);
char *rtrim(char *txt);
int get_tzOffset(/*OUT*/int *isDST);
int CreatePath(const char *dir);
void HexDump(uint8_t *buf, int count, int radix);
char *FormatFloat(char *str, float value, int width, int precision, char decimalpoint);
char *FormatDouble(char *str, double value, int width, int precision, char decimalpoint);
std::string realpath(const char *path);

#define DEBUG_LOW (debug >= 1)
#define DEBUG_NORMAL (debug >= 2)
#define DEBUG_HIGH (debug >= 3)
#define DEBUG_VERYHIGH (debug >= 4)
#define DEBUG_HIGHEST (debug >= 5)

#define VERBOSE_LOW (verbose >= 1)
#define VERBOSE_NORMAL (verbose >= 2)
#define VERBOSE_HIGH (verbose >= 3)
#define VERBOSE_VERYHIGH (verbose >= 4)
#define VERBOSE_HIGHEST (verbose >= 5)

enum ERRORLEVEL
{
    PROC_INFO       = 0,
    PROC_WARNING    = 1,
    PROC_ERROR      = 2,
    PROC_CRITICAL   = 3
};

void print_error(FILE *error_file, ERRORLEVEL error_level, const char *error_msg);
