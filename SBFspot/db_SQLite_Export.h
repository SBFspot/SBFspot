/************************************************************************************************
    SBFspot - Yet another tool to read power production of SMA solar inverters
    (c)2012-2018, SBF

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

#if defined(USE_SQLITE)

#include "db_SQLite.h"
#include <sstream>

extern int quiet;
extern int verbose;

class db_SQL_Export : public db_SQL_Base
{
public:
    int exportDayData(InverterData *inverters[]);
    int exportMonthData(InverterData *inverters[]);
    int exportSpotData(InverterData *inv[], time_t spottime);
    int exportEventData(InverterData *inv[], TagDefs& tags);
    int exportBatteryData(InverterData *inverters[], time_t spottime);

private:
    int insert_battery_data(sqlite3_stmt* pStmt, int32_t tm, int32_t sn, int32_t key, int32_t val);
};

#endif //#if defined(USE_SQLITE)
