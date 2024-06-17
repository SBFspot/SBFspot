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

#include "osselect.h"
#include "misc.h"
#include "SBFspot.h"
#include <string.h>
#include <stdio.h>
#if defined(_WIN32)
#include "decoder.h"
#endif
#if defined(__linux__)
#include <limits.h>
#endif

extern int debug;

//print time as UTC time
std::string strfgmtime_t(const char *format, const time_t rawtime)
{
    const tm *timeinfo = gmtime(&rawtime);

    std::ostringstream os;
    os << std::put_time(timeinfo, format);
    return os.str();
}

//Print time as local time
std::string strftime_t(const char *format, const time_t rawtime)
{
    const tm *timeinfo = localtime(&rawtime);

    std::ostringstream os;
    os << std::put_time(timeinfo, format);
    return os.str();
}

char *rtrim(char *txt)
{
    if ((txt != NULL) && (*txt != 0))
    {
        char *ptr = txt;

        // Find end-of-string
        while (*ptr != 0) ptr++;
        ptr--;

        while (ptr >= txt && (*ptr == ' ' || *ptr == '\t' || *ptr == '\r' || *ptr == '\n'))
            ptr--;
        *(++ptr) = 0;
    }

    return txt;
}

#if defined(__linux__)
int get_tzOffset(/*OUT*/int *isDST)
{
    time_t curtime;
    time(&curtime);

    struct tm *loctime = localtime(&curtime);

    if (isDST)  // Valid pointer?
        *isDST = loctime->tm_isdst;

    return loctime->tm_gmtoff;
}
#endif

#if defined(_WIN32)
//Get timezone in seconds
//Windows doesn't have tm_gmtoff member in tm struct
//We try to calculate it
int get_tzOffset(/*OUT*/int *isDST)
{
    time_t curtime;
    time(&curtime);

    // gmtime() and localtime() share the same static tm structure, so we have to make a copy
    // http://www.cplusplus.com/reference/ctime/localtime/

    struct tm loctime;  //Local Time
    memcpy(&loctime, localtime(&curtime), sizeof(loctime));

    struct tm utctime;  //GMT time
    memcpy(&utctime, gmtime(&curtime), sizeof(utctime));

    int tzOffset = (loctime.tm_hour - utctime.tm_hour) * 3600 + (loctime.tm_min - utctime.tm_min) * 60;

    if((loctime.tm_year > utctime.tm_year) || (loctime.tm_mon > utctime.tm_mon) || (loctime.tm_mday > utctime.tm_mday))
        tzOffset += 86400;

    if((loctime.tm_year < utctime.tm_year) || (loctime.tm_mon < utctime.tm_mon) || (loctime.tm_mday < utctime.tm_mday))
        tzOffset -= 86400;

    if (isDST)  // Valid pointer?
        *isDST = loctime.tm_isdst;

    return tzOffset;
}
#endif

// Create a full directory
int CreatePath(const char *dir)
{
    char fullPath[MAX_PATH];
    int rc = 0;
#if defined(_WIN32)
    _fullpath(fullPath, dir, sizeof(fullPath));
    //Terminate path with backslash
    char c = fullPath[strlen(fullPath)-1];
    if ((c != '/') && (c != '\\')) strcat(fullPath, "\\");
#else //Linux
    //temp fix for issue 33: buffer overflow detected
    //realpath(dir, fullPath);
    strcpy(fullPath, dir);
    //Terminate path with slash
    char c = fullPath[strlen(fullPath)-1];
    if (c != '/') strcat(fullPath, "/");
#endif

    int idx = 0;
    while (fullPath[idx] != 0)
    {
        c = fullPath[idx++];
        if ((c == '/') || (c == '\\'))
        {
            c = fullPath[idx];
            fullPath[idx] = 0;
            //Create directory
            //Ignore error here, only the last one will be returned
#if defined(_WIN32)
            rc = _mkdir(fullPath);
#else //Linux
            rc = mkdir(fullPath, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
#endif
            fullPath[idx] = c;
        }
    }

    if (rc == 0)
        return rc;
    else
        return errno;
}

void print_error(FILE *error_file, ERRORLEVEL error_level, const char *error_msg)
{
    const char *error_list[4] = {"INFO", "WARNING", "ERROR", "CRITICAL"};
    char my_time[32];
    time_t tim = time(nullptr);
    strcpy(my_time, ctime(&tim));
    size_t size = strlen(my_time);
    my_time[size - 1] = '\0';	//Remove newline char
    fprintf(error_file,"%s: %s: %s", my_time, error_list[error_level], error_msg);
    fflush(error_file);
}

void HexDump(uint8_t *buf, int count, int radix)
{
#if defined(_WIN32)
    if (decodeSMAData)
        decodeSMAData(buf, count, tagdefs);
    else
#endif
    {
        int i, j;
        printf("--------:");
        for (i = 0; i < radix; i++)
        {
            printf(" %02X", i);
        }
        for (i = 0, j = 0; i < count; i++)
        {
            if (j % radix == 0)
            {
                if (radix == 16)
                    printf("\n%08X: ", j);
                else
                    printf("\n%08d: ", j);
            }
            printf("%02X ", buf[i]);
            j++;
        }
        printf("\n");
        fflush(stdout);
    }
}

char *FormatFloat(char *str, float value, int width, int precision, char decimalpoint)
{
    sprintf(str, "%*.*f", width, precision, value);
    char *dppos = strrchr(str, '.');
    if (dppos != NULL) *dppos = decimalpoint;
    return str;
}

char *FormatDouble(char *str, double value, int width, int precision, char decimalpoint)
{
    sprintf(str, "%*.*f", width, precision, value);
    char *dppos = strrchr(str, '.');
    if (dppos != NULL) *dppos = decimalpoint;
    return str;
}

/*
//TODO: Implement this for Linux/Windows
#if defined(__linux__)
int getOSVersion(char *VersionString)
{
    return 0;
}

#elif defined(_WIN32)
int getOSVersion(char *VersionString)
{
    OSVERSIONINFO vi;
    vi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&vi);

    return 0;
}
#endif
*/

/*
realpath() expands all symbolic links and resolves references to /./, /../ and
extra '/' characters in the null-terminated string named by path to produce
a canonicalized absolute pathname.
*/
std::string realpath(const char *path)
{
#if defined(_WIN32)
    char pathbuf[MAX_PATH];
    pathbuf[0] = 0;
    std::string RealPath(path);

    OFSTRUCT of;
    HANDLE file = (HANDLE)OpenFile(path, &of, OF_READ);

    DWORD rc = GetFinalPathNameByHandleA(file, pathbuf, sizeof(pathbuf), FILE_NAME_OPENED);
    CloseHandle(file);

    if (rc < sizeof(pathbuf))
    {
        RealPath = pathbuf;
        if (RealPath.substr(0, 8).compare("\\\\?\\UNC\\") == 0)
            RealPath = "\\" + RealPath.substr(7);
        else if (RealPath.substr(0, 4).compare("\\\\?\\") == 0)
            RealPath = RealPath.substr(4);
    }

    return RealPath;
#endif

#if defined(__linux__)
    char pathbuf[PATH_MAX];
    pathbuf[0] = 0;
    if (realpath(path, pathbuf))
        return std::string(pathbuf);
    else
        return std::string(path);
#endif
}

