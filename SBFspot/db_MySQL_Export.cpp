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

#if defined(USE_MYSQL)

#include "db_MySQL_Export.h"

using namespace std;

int db_SQL_Export::day_data(InverterData *inverters[])
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

		for (int inv=0; inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
		{
			const unsigned int numelements = sizeof(inverters[inv]->dayData)/sizeof(DayData);
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
			for (last_rec = numelements-1; last_rec > first_rec; last_rec--)
			{
				if ((inverters[inv]->dayData[last_rec].datetime != 0) && (inverters[inv]->dayData[last_rec].watt != 0))
					break;
			}

			if (first_rec < last_rec) // Production data found or all zero?
			{
				// Store data from first to last record
		        for (unsigned int idx = first_rec; idx <= last_rec; idx++)
				{
					// Invalid dates are not written to db
					if (inverters[inv]->dayData[idx].datetime > 0)
					{
						memset(values, 0, sizeof(values));

						// Timestamp
						values[0].buffer_type	= MYSQL_TYPE_LONG;
						values[0].buffer		= (void *) &inverters[inv]->dayData[idx].datetime;
						values[0].is_unsigned	= false;

						// Serial
						values[1].buffer_type	= MYSQL_TYPE_LONG;
						values[1].buffer		= (void *) &inverters[inv]->Serial;
						values[1].is_unsigned	= true;

						// Total Yield
						values[2].buffer_type	= MYSQL_TYPE_LONGLONG;
						values[2].buffer		= (void *) &inverters[inv]->dayData[idx].totalWh;
						values[2].is_unsigned	= true;

						// Power
						values[3].buffer_type	= MYSQL_TYPE_LONGLONG;
						values[3].buffer		= (void *) &inverters[inv]->dayData[idx].watt;
						values[3].is_unsigned	= true;

						// PVOutput
						values[4].buffer_type	= MYSQL_TYPE_NULL;

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

int db_SQL_Export::month_data(InverterData *inverters[])
{
	const char *sql = "INSERT INTO MonthData(TimeStamp,Serial,TotalYield,DayYield) VALUES(?,?,?,?) ON DUPLICATE KEY UPDATE Serial=Serial";
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

		for (int inv=0; inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
		{
			for (unsigned int idx = 0; idx < sizeof(inverters[inv]->monthData)/sizeof(MonthData); idx++)
			{
				if (inverters[inv]->monthData[idx].datetime > 0)
				{
					memset(values, 0, sizeof(values));

					// Timestamp
					values[0].buffer_type	= MYSQL_TYPE_LONG;
					values[0].buffer		= (void *) &inverters[inv]->monthData[idx].datetime;
					values[0].is_unsigned	= false;

					// Serial
					values[1].buffer_type	= MYSQL_TYPE_LONG;
					values[1].buffer		= (void *) &inverters[inv]->Serial;
					values[1].is_unsigned	= true;

					// Total Yield
					values[2].buffer_type	= MYSQL_TYPE_LONGLONG;
					values[2].buffer		= (void *) &inverters[inv]->monthData[idx].totalWh;
					values[2].is_unsigned	= true;

					// Day Yield
					values[3].buffer_type	= MYSQL_TYPE_LONGLONG;
					values[3].buffer		= (void *) &inverters[inv]->monthData[idx].dayWh;
					values[3].is_unsigned	= true;

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

int db_SQL_Export::spot_data(InverterData *inv[], time_t spottime)
{
	stringstream sql;
	int rc = SQL_OK;

	for (int i=0; inv[i]!=NULL && i<MAX_INVERTERS; i++)
	{
		sql.str("");
		sql << "INSERT INTO SpotData VALUES(" <<
		spottime << ',' <<
		inv[i]->Serial << ',' <<
		inv[i]->Pdc1 << ',' <<
		inv[i]->Pdc2 << ',' <<
		(float)inv[i]->Idc1/1000 << ',' <<
		(float)inv[i]->Idc2/1000 << ',' <<
		(float)inv[i]->Udc1/100 << ',' <<
		(float)inv[i]->Udc2/100 << ',' <<
		inv[i]->Pac1 << ',' <<
		inv[i]->Pac2 << ',' <<
		inv[i]->Pac3 << ',' <<
		(float)inv[i]->Iac1/1000 << ',' <<
		(float)inv[i]->Iac2/1000 << ',' <<
		(float)inv[i]->Iac3/1000 << ',' <<
		(float)inv[i]->Uac1/100 << ',' <<
		(float)inv[i]->Uac2/100 << ',' <<
		(float)inv[i]->Uac3/100 << ',' <<
		inv[i]->EToday << ',' <<
		inv[i]->ETotal << ',' <<
		(float)inv[i]->GridFreq/100 << ',' <<
		(double)inv[i]->OperationTime/3600 << ',' <<
		(double)inv[i]->FeedInTime/3600 << ',' <<
		(float)inv[i]->BT_Signal << ',' <<
		s_quoted(status_text(inv[i]->DeviceStatus)) << ',' <<
		s_quoted(status_text(inv[i]->GridRelayStatus)) << ',' <<
		(float)inv[i]->Temperature/100 << ")";

		if ((rc = exec_query(sql.str())) != SQL_OK)
		{
			print_error("[spot_data]exec_query() returned", sql.str());
			break;
		}
	}

	return rc;
}

int db_SQL_Export::event_data(InverterData *inv[], TagDefs& tags)
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

		for (int i=0; inv[i]!=NULL && i<MAX_INVERTERS; i++)
		{
			for (vector<EventData>::iterator it=inv[i]->eventData.begin(); it!=inv[i]->eventData.end(); ++it)
			{
				string grp = tags.getDesc(it->Group());
				string tag = tags.getDesc(it->Tag());

				// If description contains "%s", replace it with localized parameter
				size_t start_pos = tag.find("%s");
				if (start_pos != string::npos)
					tag.replace(start_pos, 2, tags.getDescForLRI(it->Parameter()));

				string usrgrp = tags.getDesc(it->UserGroupTagID());
				stringstream oldval;
				stringstream newval;

				switch (it->DataType())
				{
					case DT_STATUS:
						oldval << tags.getDesc(it->OldVal() & 0xFFFF);
						newval << tags.getDesc(it->NewVal() & 0xFFFF);
						break;

					case DT_STRING:
						oldval.width(8); oldval.fill('0');
						oldval << it->OldVal();
						newval.width(8); newval.fill('0');
						newval << it->NewVal();
						break;

					default:
						oldval << it->OldVal();
						newval << it->NewVal();
				}

				memset(values, 0, sizeof(values));

				// Entry ID
				uint16_t EntryID = it->EntryID();
				values[0].buffer_type	= MYSQL_TYPE_SHORT;
				values[0].buffer		= &EntryID;
				values[0].is_unsigned	= true;

				// Timestamp
				int32_t DateTime = it->DateTime();
				values[1].buffer_type	= MYSQL_TYPE_LONG;
				values[1].buffer		= &DateTime;
				values[1].is_unsigned	= false;

				// Serial
				uint32_t SerNo = it->SerNo();
				values[2].buffer_type	= MYSQL_TYPE_LONG;
				values[2].buffer		= &SerNo;
				values[2].is_unsigned	= true;

				// SUSy ID
				uint16_t SUSyID = it->SUSyID();
				values[3].buffer_type	= MYSQL_TYPE_SHORT;
				values[3].buffer		= &SUSyID;
				values[3].is_unsigned	= true;

				// Event Code
				uint16_t EventCode = it->EventCode();
				values[4].buffer_type	= MYSQL_TYPE_SHORT;
				values[4].buffer		= &EventCode;
				values[4].is_unsigned	= true;

				// Event Type
				string EventType = it->EventType();
				values[5].buffer_type	= MYSQL_TYPE_STRING;
				values[5].buffer		= (char *)EventType.c_str();
				values[5].buffer_length = EventType.size();

				// Event Category
				string EventCategory = it->EventCategory();
				values[6].buffer_type	= MYSQL_TYPE_STRING;
				values[6].buffer		= (char *)EventCategory.c_str();
				values[6].buffer_length = EventCategory.size();

				// Event Group
				values[7].buffer_type	= MYSQL_TYPE_STRING;
				values[7].buffer		= (char *)grp.c_str();
				values[7].buffer_length = grp.size();

				// Event Tag
				values[8].buffer_type	= MYSQL_TYPE_STRING;
				values[8].buffer		= (char *)tag.c_str();
				values[8].buffer_length = tag.size();

				// Old Value
				string OldValue = oldval.str();
				values[9].buffer_type	= MYSQL_TYPE_STRING;
				values[9].buffer		= (char *)OldValue.c_str();
				values[9].buffer_length = OldValue.size();

				// New Value
				string NewValue = newval.str();
				values[10].buffer_type	= MYSQL_TYPE_STRING;
				values[10].buffer		= (char *)NewValue.c_str();
				values[10].buffer_length = NewValue.size();

				// User Group
				values[11].buffer_type	= MYSQL_TYPE_STRING;
				values[11].buffer		= (char *)usrgrp.c_str();
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

	return rc;
}

int db_SQL_Export::battery_data(InverterData *inverters[], time_t spottime)
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

		for (int inv=0; inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
		{
			InverterData* id = inverters[inv];
		    if ((id->DevClass == BatteryInverter) || (id->hasBattery))
			{
				if ((rc = insert_battery_data(pStmt, spottime, id->Serial, BatChaStt >> 8, id->BatChaStt)) != SQL_OK) break;
				if ((rc = insert_battery_data(pStmt, spottime, id->Serial, BatTmpVal >> 8, id->BatTmpVal)) != SQL_OK) break;
				if ((rc = insert_battery_data(pStmt, spottime, id->Serial, BatVol >> 8, id->BatVol)) != SQL_OK) break;
				if ((rc = insert_battery_data(pStmt, spottime, id->Serial, BatAmp >> 8, id->BatAmp)) != SQL_OK) break;
				//if ((rc = insert_battery_data(pStmt, spottime, id->Serial, BatDiagCapacThrpCnt >> 8, id->BatDiagCapacThrpCnt)) != SQL_OK) break;
				//if ((rc = insert_battery_data(pStmt, spottime, id->Serial, BatDiagTotAhIn >> 8, id->BatDiagTotAhIn)) != SQL_OK) break;
				//if ((rc = insert_battery_data(pStmt, spottime, id->Serial, BatDiagTotAhOut >> 8, id->BatDiagTotAhOut)) != SQL_OK) break;
				if ((rc = insert_battery_data(pStmt, spottime, id->Serial, MeteringGridMsTotWIn >> 8, id->MeteringGridMsTotWIn)) != SQL_OK) break;
				if ((rc = insert_battery_data(pStmt, spottime, id->Serial, MeteringGridMsTotWOut >> 8, id->MeteringGridMsTotWOut)) != SQL_OK) break;
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
	values[0].buffer_type	= MYSQL_TYPE_LONG;
	values[0].buffer		= (void *) &tm;
	values[0].is_unsigned	= false;

	// Serial
	values[1].buffer_type	= MYSQL_TYPE_LONG;
	values[1].buffer		= (void *) &sn;
	values[1].is_unsigned	= true;

	// Key
	values[2].buffer_type	= MYSQL_TYPE_LONG;
	values[2].buffer		= (void *) &key;
	values[2].is_unsigned	= false;

	// Value
	values[3].buffer_type	= MYSQL_TYPE_LONG;
	values[3].buffer		= (void *) &val;
	values[3].is_unsigned	= false;

	mysql_stmt_bind_param(pStmt, values);

	rc = mysql_stmt_execute(pStmt);

	if (rc != SQL_OK)
	{
		print_error("[battery_data]mysql_stmt_execute() returned");
	}

	return rc;
}

#endif
