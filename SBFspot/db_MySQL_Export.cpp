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

#if defined(USE_MYSQL)

#include "db_MySQL_Export.h"
#include "mppt.h"

int db_SQL_Export::exportDayData(InverterData *inverters[])
{
    const char *sql = "INSERT INTO DayData(TimeStamp,Serial,TotalYield,Power,PVoutput) VALUES(?,?,?,?,?) ON DUPLICATE KEY UPDATE Serial=Serial";
    int rc = SQL_OK;

    MYSQL_BIND values[5];

    MYSQL_STMT *pStmt = mysql_stmt_init(m_dbHandle);
    if (!pStmt)
    {
        print_error("Out of memory");
        return SQL_ERROR;
    }

    if ((rc = mysql_stmt_prepare(pStmt, sql, strlen(sql))) == SQL_OK)
    {
        exec_query("START TRANSACTION");

        for (uint32_t inv = 0; inverters[inv] != NULL && inv<MAX_INVERTERS; inv++)
        {
            const unsigned int numelements = sizeof(inverters[inv]->dayData) / sizeof(DayData);
            unsigned int first_rec, last_rec;
            // Find first record with production data
            for (first_rec = 0; first_rec < numelements; first_rec++)
            {
                if ((inverters[inv]->dayData[first_rec].datetime == 0) || (inverters[inv]->dayData[first_rec].watt != 0))
                {
                    // Include last zero record, just before production starts
                    if (first_rec > 0) first_rec--;
                    break;
                }
            }

            // Find last record with production data
            for (last_rec = numelements - 1; last_rec > first_rec; last_rec--)
            {
                if ((inverters[inv]->dayData[last_rec].datetime != 0) && (inverters[inv]->dayData[last_rec].watt != 0))
                    break;
            }

            // Include zero record, just after production stopped
            if ((last_rec < numelements - 1) && (inverters[inv]->dayData[last_rec + 1].datetime != 0))
                last_rec++;

            if (first_rec < last_rec) // Production data found or all zero?
            {
                // Store data from first to last record
                for (unsigned int idx = first_rec; idx <= last_rec; idx++)
                {
                    // Invalid dates are not written to db
                    if (inverters[inv]->dayData[idx].datetime != 0)
                    {
                        memset(values, 0, sizeof(values));

                        // Timestamp
                        values[0].buffer_type = MYSQL_TYPE_LONG;
                        values[0].buffer = (void *)&inverters[inv]->dayData[idx].datetime;
                        values[0].is_unsigned = false;

                        // Serial
                        values[1].buffer_type = MYSQL_TYPE_LONG;
                        values[1].buffer = (void *)&inverters[inv]->Serial;
                        values[1].is_unsigned = true;

                        // Total Yield
                        values[2].buffer_type = MYSQL_TYPE_LONGLONG;
                        values[2].buffer = (void *)&inverters[inv]->dayData[idx].totalWh;
                        values[2].is_unsigned = true;

                        // Power
                        values[3].buffer_type = MYSQL_TYPE_LONGLONG;
                        values[3].buffer = (void *)&inverters[inv]->dayData[idx].watt;
                        values[3].is_unsigned = true;

                        // PVOutput
                        values[4].buffer_type = MYSQL_TYPE_NULL;

                        mysql_stmt_bind_param(pStmt, values);

                        rc = mysql_stmt_execute(pStmt);

                        if (rc != SQL_OK)
                        {
                            print_error("[day_data]mysql_stmt_execute() returned");
                            break;
                        }

                        //mysql_stmt_reset(pStmt);

                        rc = SQL_OK;
                    }
                }
            }
        }

        mysql_stmt_close(pStmt);

        if (rc == SQL_OK)
            exec_query("COMMIT");
        else
            exec_query("ROLLBACK");
    }

    return rc;
}

int db_SQL_Export::exportMonthData(InverterData *inverters[])
{
    const char *sql = "INSERT INTO MonthData(TimeStamp,Serial,TotalYield,DayYield) VALUES(?,?,?,?)";

    int rc = SQL_OK;

    MYSQL_BIND values[4];

    MYSQL_STMT *pStmt = mysql_stmt_init(m_dbHandle);
    if (!pStmt)
    {
        print_error("Out of memory");
        return SQL_ERROR;
    }

    if ((rc = mysql_stmt_prepare(pStmt, sql, strlen(sql))) == SQL_OK)
    {
        exec_query("START TRANSACTION");

        for (uint32_t inv = 0; inverters[inv] != NULL && inv<MAX_INVERTERS; inv++)
        {
            // Fix #74 / #701: Double data in Monthdata table
            tm *ptm = localtime(&inverters[inv]->monthData[0].datetime);

            std::stringstream rmvsql;
            rmvsql << "DELETE FROM MonthData WHERE Serial=" << inverters[inv]->Serial << " AND DATE_FORMAT(CONVERT_TZ(FROM_UNIXTIME(TimeStamp),@@time_zone,'+00:00'), '%Y-%m')='" << std::put_time(ptm, "%Y-%m") << "';";

            if ((rc = exec_query(rmvsql.str())) != SQL_OK)
            {
                print_error("exec_query() returned", rmvsql.str());
                break;
            }

            for (unsigned int idx = 0; idx < sizeof(inverters[inv]->monthData) / sizeof(MonthData); idx++)
            {
                if (inverters[inv]->monthData[idx].datetime != 0)
                {
                    memset(values, 0, sizeof(values));

                    // Timestamp
                    values[0].buffer_type = MYSQL_TYPE_LONG;
                    values[0].buffer = (void *)&inverters[inv]->monthData[idx].datetime;
                    values[0].is_unsigned = false;

                    // Serial
                    values[1].buffer_type = MYSQL_TYPE_LONG;
                    values[1].buffer = (void *)&inverters[inv]->Serial;
                    values[1].is_unsigned = true;

                    // Total Yield
                    values[2].buffer_type = MYSQL_TYPE_LONGLONG;
                    values[2].buffer = (void *)&inverters[inv]->monthData[idx].totalWh;
                    values[2].is_unsigned = true;

                    // Day Yield
                    values[3].buffer_type = MYSQL_TYPE_LONGLONG;
                    values[3].buffer = (void *)&inverters[inv]->monthData[idx].dayWh;
                    values[3].is_unsigned = true;

                    mysql_stmt_bind_param(pStmt, values);

                    rc = mysql_stmt_execute(pStmt);

                    if (rc != SQL_OK)
                    {
                        print_error("[month_data]mysql_stmt_execute() returned");
                        break;
                    }

                    //mysql_stmt_reset(pStmt);

                    rc = SQL_OK;
                }
            }
        }

        mysql_stmt_close(pStmt);

        if (rc == SQL_OK)
            exec_query("COMMIT");
        else
            exec_query("ROLLBACK");
    }

    return rc;
}

int db_SQL_Export::exportSpotData(InverterData *inv[], time_t spottime)
{
    std::stringstream sql;
    int rc = SQL_OK;

    for (uint32_t i = 0; inv[i] != NULL && i<MAX_INVERTERS; i++)
    {
        sql.str("");
        sql << "INSERT INTO SpotData VALUES(" <<
            spottime << ',' <<
            inv[i]->Serial << ',' <<
            inv[i]->mpp.at(1).Pdc() << ',' <<
            inv[i]->mpp.at(2).Pdc() << ',' <<
            (float)inv[i]->mpp.at(1).Idc() / 1000 << ',' <<
            (float)inv[i]->mpp.at(2).Idc() / 1000 << ',' <<
            (float)inv[i]->mpp.at(1).Udc() / 100 << ',' <<
            (float)inv[i]->mpp.at(2).Udc() / 100 << ',' <<
            inv[i]->Pac1 << ',' <<
            inv[i]->Pac2 << ',' <<
            inv[i]->Pac3 << ',' <<
            (float)inv[i]->Iac1 / 1000 << ',' <<
            (float)inv[i]->Iac2 / 1000 << ',' <<
            (float)inv[i]->Iac3 / 1000 << ',' <<
            (float)inv[i]->Uac1 / 100 << ',' <<
            (float)inv[i]->Uac2 / 100 << ',' <<
            (float)inv[i]->Uac3 / 100 << ',' <<
            inv[i]->EToday << ',' <<
            inv[i]->ETotal << ',' <<
            (float)inv[i]->GridFreq / 100 << ',' <<
            (double)inv[i]->OperationTime / 3600 << ',' <<
            (double)inv[i]->FeedInTime / 3600 << ',' <<
            (float)inv[i]->BT_Signal << ',' <<
            s_quoted(status_text(inv[i]->DeviceStatus)) << ',' <<
            s_quoted(status_text(inv[i]->GridRelayStatus)) << ',' <<
            null_if_nan(inv[i]->Temperature, 2) <<
            ')';

        if ((rc = exec_query(sql.str())) != SQL_OK)
        {
            print_error("[spot_data]exec_query() returned", sql.str());
            break;
        }

        // If inverter has more than 2 mppt, use SpotDataX table to store the data
        if (inv[i]->mpp.size() > 2)
        {
            const char* INSERT_SpotDataX = "INSERT INTO SpotDataX VALUES(";
            sql.str("");
            for (const auto &mpp : inv[i]->mpp)
            {
                sql << INSERT_SpotDataX << spottime << ',' << inv[i]->Serial << ',' << (LriDef::DcMsWatt | mpp.first) << ',' << mpp.second.Pdc() << ");";
                sql << INSERT_SpotDataX << spottime << ',' << inv[i]->Serial << ',' << (LriDef::DcMsVol | mpp.first) << ',' << mpp.second.Udc() << ");";
                sql << INSERT_SpotDataX << spottime << ',' << inv[i]->Serial << ',' << (LriDef::DcMsAmp | mpp.first) << ',' << mpp.second.Idc() << ");";
            }

            if ((rc = exec_query_multi(sql.str())) != SQL_OK)
            {
                print_error("[spot_data]exec_query() returned", sql.str());
                break;
            }
        }
    }

    return rc;
}

int db_SQL_Export::exportEventData(InverterData *inv[], TagDefs& tags)
{
    const char *sql = "INSERT INTO EventData(EntryID,TimeStamp,Serial,SusyID,EventCode,EventType,Category,EventGroup,Tag,OldValue,NewValue,UserGroup) VALUES(?,?,?,?,?,?,?,?,?,?,?,?) ON DUPLICATE KEY UPDATE Serial=Serial";
    int rc = SQL_OK;

    MYSQL_BIND values[12];

    MYSQL_STMT *pStmt = mysql_stmt_init(m_dbHandle);
    if (!pStmt)
    {
        print_error("Out of memory");
        return SQL_ERROR;
    }

    if ((rc = mysql_stmt_prepare(pStmt, sql, strlen(sql))) == SQL_OK)
    {
        exec_query("START TRANSACTION");

        for (uint32_t i = 0; inv[i] != NULL && i<MAX_INVERTERS; i++)
        {
            for (const auto &event : inv[i]->eventData)
            {
                std::string grp = tags.getDesc(event.Group());
                std::string desc = event.EventDescription();
                std::string usrgrp = tags.getDesc(event.UserGroupTagID());
                std::stringstream oldval;
                std::stringstream newval;

                switch (event.DataType())
                {
                case DT_STATUS:
                    oldval << tags.getDesc(event.OldVal() & 0xFFFF);
                    newval << tags.getDesc(event.NewVal() & 0xFFFF);
                    break;

                case DT_STRING:
                    newval << event.EventStrPara();
                    break;

                default:
                    oldval << event.OldVal();
                    newval << event.NewVal();
                }

                memset(values, 0, sizeof(values));

                // Entry ID
                uint16_t EntryID = event.EntryID();
                values[0].buffer_type = MYSQL_TYPE_SHORT;
                values[0].buffer = &EntryID;
                values[0].is_unsigned = true;

                // Timestamp
                int32_t DateTime = (int32_t)event.DateTime();
                values[1].buffer_type = MYSQL_TYPE_LONG;
                values[1].buffer = &DateTime;
                values[1].is_unsigned = false;

                // Serial
                uint32_t SerNo = event.SerNo();
                values[2].buffer_type = MYSQL_TYPE_LONG;
                values[2].buffer = &SerNo;
                values[2].is_unsigned = true;

                // SUSy ID
                uint16_t SUSyID = event.SUSyID();
                values[3].buffer_type = MYSQL_TYPE_SHORT;
                values[3].buffer = &SUSyID;
                values[3].is_unsigned = true;

                // Event Code
                uint16_t EventCode = event.EventCode();
                values[4].buffer_type = MYSQL_TYPE_SHORT;
                values[4].buffer = &EventCode;
                values[4].is_unsigned = true;

                // Event Type
                std::string EventType = event.EventType();
                values[5].buffer_type = MYSQL_TYPE_STRING;
                values[5].buffer = (char *)EventType.c_str();
                values[5].buffer_length = EventType.size();

                // Event Category
                std::string EventCategory = event.EventCategory();
                values[6].buffer_type = MYSQL_TYPE_STRING;
                values[6].buffer = (char *)EventCategory.c_str();
                values[6].buffer_length = EventCategory.size();

                // Event Group
                values[7].buffer_type = MYSQL_TYPE_STRING;
                values[7].buffer = (char *)grp.c_str();
                values[7].buffer_length = grp.size();

                // Event Tag
                values[8].buffer_type = MYSQL_TYPE_STRING;
                values[8].buffer = (char *)desc.c_str();
                values[8].buffer_length = desc.size();

                // Old Value
                std::string OldValue = oldval.str();
                if (OldValue.empty())
                    values[9].buffer_type = MYSQL_TYPE_NULL; // Fix #545/#548
                else
                {
                    values[9].buffer_type = MYSQL_TYPE_STRING;
                    values[9].buffer = (char *)OldValue.c_str();
                    values[9].buffer_length = OldValue.size();
                }

                // New Value
                std::string NewValue = newval.str();
                if (NewValue.empty())
                    values[10].buffer_type = MYSQL_TYPE_NULL; // Fix #545/#548
                else
                {
                    values[10].buffer_type = MYSQL_TYPE_STRING;
                    values[10].buffer = (char *)NewValue.c_str();
                    values[10].buffer_length = NewValue.size();
                }

                // User Group
                values[11].buffer_type = MYSQL_TYPE_STRING;
                values[11].buffer = (char *)usrgrp.c_str();
                values[11].buffer_length = usrgrp.size();

                mysql_stmt_bind_param(pStmt, values);

                rc = mysql_stmt_execute(pStmt);

                if (rc != SQL_OK)
                {
                    print_error("[event_data]mysql_stmt_execute() returned");
                    break;
                }

                //mysql_stmt_reset(pStmt);

                rc = SQL_OK;
            }
        }

        mysql_stmt_close(pStmt);

        if (rc == SQL_OK)
            exec_query("COMMIT");
        else
            exec_query("ROLLBACK");
    }
    else
        print_error("[event_data]mysql_stmt_prepare() returned");

    return rc;
}

int db_SQL_Export::exportBatteryData(InverterData *inverters[], time_t spottime)
{
    const char *sql = "INSERT INTO SpotDataX(`TimeStamp`,`Serial`,`Key`,`Value`) VALUES(?,?,?,?)";
    int rc = SQL_OK;

    MYSQL_STMT *pStmt = mysql_stmt_init(m_dbHandle);
    if (!pStmt)
    {
        print_error("Out of memory");
        return SQL_ERROR;
    }

    if ((rc = mysql_stmt_prepare(pStmt, sql, strlen(sql))) == SQL_OK)
    {
        exec_query("START TRANSACTION");

        for (uint32_t inv = 0; inverters[inv] != NULL && inv<MAX_INVERTERS; inv++)
        {
            InverterData* id = inverters[inv];
            if (id->hasBattery)
            {
                if ((rc = insert_battery_data(pStmt, (int32_t)spottime, id->Serial, BatChaStt >> 8, id->BatChaStt)) != SQL_OK) break;
                if ((rc = insert_battery_data(pStmt, (int32_t)spottime, id->Serial, BatTmpVal >> 8, id->BatTmpVal)) != SQL_OK) break;
                if ((rc = insert_battery_data(pStmt, (int32_t)spottime, id->Serial, BatVol >> 8, id->BatVol)) != SQL_OK) break;
                if ((rc = insert_battery_data(pStmt, (int32_t)spottime, id->Serial, BatAmp >> 8, id->BatAmp)) != SQL_OK) break;
                //if ((rc = insert_battery_data(pStmt, (int32_t)spottime, id->Serial, BatDiagCapacThrpCnt >> 8, id->BatDiagCapacThrpCnt)) != SQL_OK) break;
                //if ((rc = insert_battery_data(pStmt, (int32_t)spottime, id->Serial, BatDiagTotAhIn >> 8, id->BatDiagTotAhIn)) != SQL_OK) break;
                //if ((rc = insert_battery_data(pStmt, (int32_t)spottime, id->Serial, BatDiagTotAhOut >> 8, id->BatDiagTotAhOut)) != SQL_OK) break;
                if ((rc = insert_battery_data(pStmt, (int32_t)spottime, id->Serial, MeteringGridMsTotWIn >> 8, id->MeteringGridMsTotWIn)) != SQL_OK) break;
                if ((rc = insert_battery_data(pStmt, (int32_t)spottime, id->Serial, MeteringGridMsTotWOut >> 8, id->MeteringGridMsTotWOut)) != SQL_OK) break;
            }
        }

        mysql_stmt_close(pStmt);

        if (rc == SQL_OK)
            exec_query("COMMIT");
        else
            exec_query("ROLLBACK");
    }
    else
    {
        print_error("[battery_data]mysql_stmt_prepare() returned");
    }

    return rc;
}

int db_SQL_Export::insert_battery_data(MYSQL_STMT *pStmt, int32_t tm, int32_t sn, int32_t key, int32_t val)
{
    int rc = SQL_OK;

    MYSQL_BIND values[4];

    memset(values, 0, sizeof(values));

    // Timestamp
    values[0].buffer_type = MYSQL_TYPE_LONG;
    values[0].buffer = (void *)&tm;
    values[0].is_unsigned = false;

    // Serial
    values[1].buffer_type = MYSQL_TYPE_LONG;
    values[1].buffer = (void *)&sn;
    values[1].is_unsigned = true;

    // Key
    values[2].buffer_type = MYSQL_TYPE_LONG;
    values[2].buffer = (void *)&key;
    values[2].is_unsigned = false;

    // Value
    values[3].buffer_type = MYSQL_TYPE_LONG;
    values[3].buffer = (void *)&val;
    values[3].is_unsigned = false;

    mysql_stmt_bind_param(pStmt, values);

    rc = mysql_stmt_execute(pStmt);

    if (rc != SQL_OK)
    {
        print_error("[battery_data]mysql_stmt_execute() returned");
    }

    return rc;
}

#endif
