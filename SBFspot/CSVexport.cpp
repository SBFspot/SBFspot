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

#include "CSVexport.h"
#include "EventData.h"

using namespace std;

//DecimalPoint To Text
const char *dp2txt(const char dp)
{
	if (dp == '.') return "dot"; //Fix Issue 84
    if (dp == ',') return "comma";
    return "?";
}

//Delimiter To Text
const char *delim2txt(const char delim)
{
    if (delim == ';') return "semicolon";
    if (delim == ',') return "comma";
    return "?";
}

//Linebreak To Text
const char *linebreak2txt(void)
{
#if defined (WIN32)
	return "CR/LF";
#endif

#if defined (linux)
	return "LF";
#endif
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

// Convert format string like %d/%m/%Y %H:%M:%S to dd/mm/yyyy HH:mm:ss
// Returns a pointer to DMY string
// Caller is responsible to free the memory
char *DateTimeFormatToDMY(const char *dtf)
{
	char DMY[80] = {0};
	char ch[2];
	for (int i = 0; (dtf[i] != 0) && (strlen(DMY) < sizeof(DMY)-4); i++)
	{
		if (dtf[i] == '%')
		{
			i++;
			switch (dtf[i])
			{
			case 0:	// end-of-string
				i--; break;
			case 'y':
				strcat(DMY, "yy"); break;
			case 'Y':
				strcat(DMY, "yyyy"); break;
			case 'm':
				strcat(DMY, "MM"); break;
			case 'd':
				strcat(DMY, "dd"); break;
			case 'H':
				strcat(DMY, "HH"); break;
			case 'M':
				strcat(DMY, "mm"); break;
			case 'S':
				strcat(DMY, "ss"); break; //Fix Issue 84
			default:
				ch[0] = dtf[i]; ch[1] = 0;
				strcat(DMY, ch);
			}
		}
		else
		{
				ch[0] = dtf[i]; ch[1] = 0;
				strcat(DMY, ch);
		}
	}

	return strdup(DMY);
}

int ExportMonthDataToCSV(const Config *cfg, InverterData *inverters[])
{
	char msg[80 + MAX_PATH];
	if (cfg->CSV_Export == 1)
	{
		if (VERBOSE_NORMAL) puts("ExportMonthDataToCSV()");

        if (inverters[0]->monthData[0].datetime <= 0)   //invalid date?
        {
            if (!quiet) puts("There is no data to export!"); //First day of the month?
        }
        else
        {
			FILE *csv;

			//Expand date specifiers in config::outputPath
			std::stringstream csvpath;
			csvpath << strftime_t(cfg->outputPath, inverters[0]->monthData[0].datetime);
			CreatePath(csvpath.str().c_str());

			csvpath << FOLDER_SEP << cfg->plantname << "-" << strfgmtime_t("%Y%m", inverters[0]->monthData[0].datetime) << ".csv";
			
			if ((csv = fopen(csvpath.str().c_str(), "w+")) == NULL)
			{
				if (cfg->quiet == 0)
				{
					snprintf(msg, sizeof(msg), "Unable to open output file %s\n", csvpath.str().c_str());
					print_error(stdout, PROC_ERROR, msg);
				}
				return -1;
			}
			else
			{
				if (cfg->CSV_ExtendedHeader == 1)
				{
					fprintf(csv, "sep=%c\n", cfg->delimiter);
					fprintf(csv, "Version CSV1|Tool SBFspot%s (%s)|Linebreaks %s|Delimiter %s|Decimalpoint %s|Precision %d\n\n", cfg->prgVersion, OS, linebreak2txt(), delim2txt(cfg->delimiter), dp2txt(cfg->decimalpoint), cfg->precision);
					for (int inv=0; inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
						fprintf(csv, "%c%s%c%s", cfg->delimiter, inverters[inv]->DeviceName, cfg->delimiter, inverters[inv]->DeviceName);
					fputs("\n", csv);
					for (int inv=0; inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
						fprintf(csv, "%c%s%c%s", cfg->delimiter, inverters[inv]->DeviceType, cfg->delimiter, inverters[inv]->DeviceType);
					fputs("\n", csv);
					for (int inv=0; inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
						fprintf(csv, "%c%lu%c%lu", cfg->delimiter, inverters[inv]->Serial, cfg->delimiter, inverters[inv]->Serial);
					fputs("\n", csv);
					for (int inv=0; inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
						fprintf(csv, "%cTotal yield%cDay yield", cfg->delimiter, cfg->delimiter);
					fputs("\n", csv);
					for (int inv=0; inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
						fprintf(csv, "%cCounter%cAnalog", cfg->delimiter, cfg->delimiter);
					fputs("\n", csv);
				}
				if (cfg->CSV_Header == 1)
				{
					char *DMY = DateTimeFormatToDMY(cfg->DateFormat);
					fprintf(csv, "%s", DMY);
					free(DMY);
					for (int inv=0; inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
						fprintf(csv, "%ckWh%ckWh", cfg->delimiter, cfg->delimiter);
					fputs("\n", csv);
				}
			}

			char FormattedFloat[16];

			for (unsigned int idx=0; idx<sizeof(inverters[0]->monthData)/sizeof(MonthData); idx++)
			{
				time_t datetime = 0;
				for (int inv=0; inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
					if (inverters[inv]->monthData[idx].datetime > 0)
						datetime = inverters[inv]->monthData[idx].datetime;

				if (datetime > 0)
				{
					fprintf(csv, "%s", strfgmtime_t(cfg->DateFormat, datetime));
					for (int inv=0; inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
					{
						fprintf(csv, "%c%s", cfg->delimiter, FormatDouble(FormattedFloat, (double)inverters[inv]->monthData[idx].totalWh/1000, 0, cfg->precision, cfg->decimalpoint));
						fprintf(csv, "%c%s", cfg->delimiter, FormatDouble(FormattedFloat, (double)inverters[inv]->monthData[idx].dayWh/1000, 0, cfg->precision, cfg->decimalpoint));
					}
					fputs("\n", csv);
				}
			}
			fclose(csv);
		}
	}
    return 0;
}

int ExportDayDataToCSV(const Config *cfg, InverterData *inverters[])
{
	char msg[80 + MAX_PATH];
	if (VERBOSE_NORMAL) puts("ExportDayDataToCSV()");

	FILE *csv;

	//fix 1.3.1 for inverters with BT piggyback (missing interval data in the dark)
	//need to find first valid date in array
	time_t date;
	unsigned int idx = 0;
	do
	{
		date = inverters[0]->dayData[idx++].datetime;
	} while ((idx < sizeof(inverters[0]->dayData)/sizeof(DayData)) && (date == 0));

	// Fix Issue 90: SBFspot still creating 1970 .csv files
	if (date == 0) return 0;	// Nothing to export! Silently exit.

	//Expand date specifiers in config::outputPath
	std::stringstream csvpath;
	csvpath << strftime_t(cfg->outputPath, date);
	CreatePath(csvpath.str().c_str());

	csvpath << FOLDER_SEP << cfg->plantname << "-" << strftime_t("%Y%m%d", date) << ".csv";

	if ((csv = fopen(csvpath.str().c_str(), "w+")) == NULL)
	{
		if (cfg->quiet == 0)
		{
			snprintf(msg, sizeof(msg), "Unable to open output file %s\n", csvpath.str().c_str());
			print_error(stdout, PROC_ERROR, msg);
		}
		return -1;
	}
	else
	{
		if (cfg->CSV_ExtendedHeader == 1)
		{
			fprintf(csv, "sep=%c\n", cfg->delimiter);
			fprintf(csv, "Version CSV1|Tool SBFspot%s (%s)|Linebreaks %s|Delimiter %s|Decimalpoint %s|Precision %d\n\n", cfg->prgVersion, OS, linebreak2txt(), delim2txt(cfg->delimiter), dp2txt(cfg->decimalpoint), cfg->precision);
			for (int inv=0; inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
				fprintf(csv, "%c%s%c%s", cfg->delimiter, inverters[inv]->DeviceName, cfg->delimiter, inverters[inv]->DeviceName);
			fputs("\n", csv);
			for (int inv=0; inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
				fprintf(csv, "%c%s%c%s", cfg->delimiter, inverters[inv]->DeviceType, cfg->delimiter, inverters[inv]->DeviceType);
			fputs("\n", csv);
			for (int inv=0; inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
				fprintf(csv, "%c%lu%c%lu", cfg->delimiter, inverters[inv]->Serial, cfg->delimiter, inverters[inv]->Serial);
			fputs("\n", csv);
			for (int inv=0; inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
				fprintf(csv, "%cTotal yield%cPower", cfg->delimiter, cfg->delimiter);
			fputs("\n", csv);
			for (int inv=0; inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
				fprintf(csv, "%cCounter%cAnalog", cfg->delimiter, cfg->delimiter);
			fputs("\n", csv);
		}
		if (cfg->CSV_Header == 1)
		{
			char *DMY = DateTimeFormatToDMY(cfg->DateTimeFormat);
			fputs(DMY, csv);
			free(DMY);
			for (int inv=0; inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
				fprintf(csv, "%ckWh%ckW", cfg->delimiter, cfg->delimiter);
			fputs("\n", csv);
		}
	}

	char FormattedFloat[16];

	for (unsigned int dd = 0; dd < sizeof(inverters[0]->dayData)/sizeof(DayData); dd++)
	{
		time_t datetime = 0;
		unsigned long long totalPower = 0;
		for (int inv=0; inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
			if (inverters[inv]->dayData[dd].datetime > 0)
			{
				datetime = inverters[inv]->dayData[dd].datetime;
				totalPower += inverters[inv]->dayData[dd].watt;
			}

		if (datetime > 0)
		{
			if ((cfg->CSV_SaveZeroPower == 1) || (totalPower > 0))
			{
				fprintf(csv, "%s", strftime_t(cfg->DateTimeFormat, datetime));
				for (int inv=0; inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
				{
					fprintf(csv, "%c%s", cfg->delimiter, FormatDouble(FormattedFloat, (double)inverters[inv]->dayData[dd].totalWh/1000, 0, cfg->precision, cfg->decimalpoint));
					fprintf(csv, "%c%s", cfg->delimiter, FormatDouble(FormattedFloat, (double)inverters[inv]->dayData[dd].watt/1000, 0, cfg->precision, cfg->decimalpoint));
				}
				fputs("\n", csv);
			}
		}
	}
	fclose(csv);
    return 0;
}

int WriteStandardHeader(FILE *csv, const Config *cfg, DEVICECLASS devclass)
{
	char *Header1, *Header2;

	char solar1[] = "||||Watt|Watt|Amp|Amp|Volt|Volt|Watt|Watt|Watt|Amp|Amp|Amp|Volt|Volt|Volt|Watt|Watt|%|kWh|kWh|Hz|Hours|Hours|%|Status|Status|degC\n";
	char solar2[] = "|DeviceName|DeviceType|Serial|Pdc1|Pdc2|Idc1|Idc2|Udc1|Udc2|Pac1|Pac2|Pac3|Iac1|Iac2|Iac3|Uac1|Uac2|Uac3|PdcTot|PacTot|Efficiency|EToday|ETotal|Frequency|OperatingTime|FeedInTime|BT_Signal|Condition|GridRelay|Temperature\n";

	char batt1[] = "||||Watt|Watt|Watt|Amp|Amp|Amp|Volt|Volt|Volt|Watt|kWh|kWh|Hz|hours|hours|Status|%|degC|Volt|Amp|Watt|Watt\n";
	char batt2[] = "|DeviceName|DeviceType|Serial|Pac1|Pac2|Pac3|Iac1|Iac2|Iac3|Uac1|Uac2|Uac3|PacTot|EToday|ETotal|Frequency|OperatingTime|FeedInTime|Condition|SOC|Tempbatt|Ubatt|Ibatt|TotWOut|TotWIn\n";

	if (devclass == BatteryInverter)
	{
		Header1 = batt1;
		Header2 = batt2;
	}
	else
	{
		Header1 = solar1;
		Header2 = solar2;
	}

	if (cfg->CSV_ExtendedHeader == 1)
	{
		fprintf(csv, "sep=%c\n", cfg->delimiter);
		fprintf(csv, "Version CSV1|Tool SBFspot%s (%s)|Linebreaks %s|Delimiter %s|Decimalpoint %s|Precision %d\n\n", cfg->prgVersion, OS, linebreak2txt(), delim2txt(cfg->delimiter), dp2txt(cfg->decimalpoint), cfg->precision);

		for (int i=0; Header1[i]!=0; i++)
			if (Header1[i]=='|') Header1[i]=cfg->delimiter;

		fputs(Header1, csv);
	}

	if (cfg->CSV_Header == 1)
	{
		char *DMY = DateTimeFormatToDMY(cfg->DateTimeFormat); //Caller must free the allocated memory
		fputs(DMY, csv);
		free(DMY);

		for (int i=0; Header2[i]!=0; i++)
			if (Header2[i]=='|') Header2[i]=cfg->delimiter;
		fputs(Header2, csv);
	}
	return 0;
}

int WriteWebboxHeader(FILE *csv, const Config *cfg, InverterData *inverters[])
{	
	char *Header1, *Header2, *Header3;
	
	char solar1[] = "|DcMs.Watt[A]|DcMs.Watt[B]|DcMs.Amp[A]|DcMs.Amp[B]|DcMs.Vol[A]|DcMs.Vol[B]|GridMs.W.phsA|GridMs.W.phsB|GridMs.W.phsC|GridMs.A.phsA|GridMs.A.phsB|GridMs.A.phsC|GridMs.PhV.phsA|GridMs.PhV.phsB|GridMs.PhV.phsC|SS_PdcTot|GridMs.TotW|SBFspot.Efficiency|Metering.DykWh|Metering.TotWhOut|GridMs.Hz|Metering.TotOpTms|Metering.TotFeedTms|SBFspot.BTSignal|Operation.Health|Operation.GriSwStt|Coolsys.TmpNom";
	char solar2[] = "|Analog|Analog|Analog|Analog|Analog|Analog|Analog|Analog|Analog|Analog|Analog|Analog|Analog|Analog|Analog|Counter|Counter|Analog|Counter|Counter|Analog|Counter|Counter|Analog|Status|Status|Analog";
	char solar3[] = "|Watt|Watt|Amp|Amp|Volt|Volt|Watt|Watt|Watt|Amp|Amp|Amp|Volt|Volt|Volt|Watt|Watt|%|kWh|kWh|Hz|Hours|Hours|%|||degC";
	
	char batt1[] = "|GridMs.W.phsA|GridMs.W.phsB|GridMs.W.phsC|GridMs.A.phsA|GridMs.A.phsB|GridMs.A.phsC|GridMs.PhV.phsA|GridMs.PhV.phsB|GridMs.PhV.phsC|GridMs.TotW|Metering.DykWh|Metering.TotWhOut|GridMs.Hz|Metering.TotOpTms|Metering.TotFeedTms|Operation.Health|Bat.ChaStt|Bat.TmpVal|Bat.Vol|Bat.Amp|Metering.GridMs.TotWOut|Metering.GridMs.TotWIn";
	char batt2[] = "|Analog|Analog|Analog|Analog|Analog|Analog|Analog|Analog|Analog|Analog|Counter|Counter|Analog|Counter|Counter|Status|Analog|Analog|Analog|Analog|Analog|Analog";
	char batt3[] = "|Watt|Watt|Watt|Amp|Amp|Amp|Volt|Volt|Volt|Watt|kWh|kWh|Hz|Hours|Hours||%|degC|Volt|Amp|Watt|Watt";

	if (inverters[0]->DevClass == BatteryInverter)
	{
		Header1 = batt1;
		Header2 = batt2;
		Header3 = batt3;
	}
	else
	{
		Header1 = solar1;
		Header2 = solar2;
		Header3 = solar3;
	}

	if (cfg->CSV_ExtendedHeader == 1)
	{
		fprintf(csv, "sep=%c\n", cfg->delimiter);
		fprintf(csv, "Version CSV1|Tool SBFspot%s (%s)|Linebreaks %s|Delimiter %s|Decimalpoint %s|Precision %d\n\n", cfg->prgVersion, OS, linebreak2txt(), delim2txt(cfg->delimiter), dp2txt(cfg->decimalpoint), cfg->precision);

		int colcnt = 0;
		for (int i = 0; Header1[i] != 0; i++)
			if (Header1[i]=='|')
			{
				Header1[i]=cfg->delimiter;
				colcnt++;
			}

		for (int inv=0; inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
			for (int i = 0; i<colcnt; i++)
				fprintf(csv, "%c%s", cfg->delimiter, inverters[inv]->DeviceName);
		fputs("\n", csv);

		for (int inv=0; inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
			for (int i = 0; i < colcnt; i++)
				fprintf(csv, "%c%s", cfg->delimiter, inverters[inv]->DeviceType);
		fputs("\n", csv);

		for (int inv=0; inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
			for (int i = 0; i < colcnt; i++)
				fprintf(csv, "%c%lu", cfg->delimiter, inverters[inv]->Serial);
		fputs("\n", csv);
	}

	if (cfg->CSV_Header == 1)
	{
		fputs("TimeStamp", csv);
		for (int inv=0; inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
			fputs(Header1, csv);
		fputs("\n", csv);
	}


	if (cfg->CSV_ExtendedHeader == 1)
	{
		for (int i=0; Header2[i]!=0; i++)
			if (Header2[i]=='|') Header2[i]=cfg->delimiter;

		for (int inv=0; inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
			fputs(Header2, csv);
		fputs("\n", csv);

		char *DMY = DateTimeFormatToDMY(cfg->DateTimeFormat); //Caller must free the allocated memory
		fputs(DMY, csv);
		free(DMY);

		for (int i=0; Header3[i]!=0; i++)
			if (Header3[i]=='|') Header3[i]=cfg->delimiter;
		for (int inv=0; inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
			fputs(Header3, csv);
		fputs("\n", csv);
	}
	return 0;
}

int ExportSpotDataToCSV(const Config *cfg, InverterData *inverters[])
{
	/*
		As from version 2.0.6 there is a new header for the spotdata.csv
		It *should* be more compatible with SMA headers
	*/

	char msg[80 + MAX_PATH];
	if (VERBOSE_NORMAL) puts("ExportSpotDataToCSV()");

	FILE *csv;

	// Take time from computer instead of inverter
	time_t spottime = cfg->SpotTimeSource == 0 ? inverters[0]->InverterDatetime : time(NULL);

	//Expand date specifiers in config::outputPath
	std::stringstream csvpath;
	csvpath << strftime_t(cfg->outputPath, spottime);
	CreatePath(csvpath.str().c_str());

	csvpath << FOLDER_SEP << cfg->plantname << "-Spot-" << strftime_t("%Y%m%d", spottime) << ".csv";

	if ((csv = fopen(csvpath.str().c_str(), "a+")) == NULL)
	{
		if (cfg->quiet == 0)
		{
			snprintf(msg, sizeof(msg), "Unable to open output file %s\n", csvpath.str().c_str());
			print_error(stdout, PROC_ERROR, msg);
		}
		return -1;
	}
	else
	{
		//Write header when new file has been created
		#ifdef WIN32
		if (filelength(fileno(csv)) == 0)
		#else
		struct stat fStat;
		stat(csvpath.str().c_str(), &fStat);
		if (fStat.st_size == 0)
		#endif
		{
			if (cfg->SpotWebboxHeader == 0)
				WriteStandardHeader(csv, cfg, SolarInverter);
			else
				WriteWebboxHeader(csv, cfg, inverters);
		}

		char FormattedFloat[32];
		const char *strout = "%c%s";

		if (cfg->SpotWebboxHeader == 1)
			fputs(strftime_t(cfg->DateTimeFormat, spottime), csv);

		for (int inv=0; inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
		{
			if (cfg->SpotWebboxHeader == 0)
			{
				fputs(strftime_t(cfg->DateTimeFormat, spottime), csv);
				fprintf(csv, strout, cfg->delimiter, inverters[inv]->DeviceName);
				fprintf(csv, strout, cfg->delimiter, inverters[inv]->DeviceType);
				fprintf(csv, "%c%lu", cfg->delimiter, inverters[inv]->Serial);
			}

			fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, (float)inverters[inv]->Pdc1, 0, cfg->precision, cfg->decimalpoint));
			fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, (float)inverters[inv]->Pdc2, 0, cfg->precision, cfg->decimalpoint));
			fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, (float)inverters[inv]->Idc1/1000, 0, cfg->precision, cfg->decimalpoint));
			fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, (float)inverters[inv]->Idc2/1000, 0, cfg->precision, cfg->decimalpoint));
			fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, (float)inverters[inv]->Udc1/100, 0, cfg->precision, cfg->decimalpoint));
			fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, (float)inverters[inv]->Udc2/100, 0, cfg->precision, cfg->decimalpoint));
			fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, (float)inverters[inv]->Pac1, 0, cfg->precision, cfg->decimalpoint));
			fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, (float)inverters[inv]->Pac2, 0, cfg->precision, cfg->decimalpoint));
			fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, (float)inverters[inv]->Pac3, 0, cfg->precision, cfg->decimalpoint));
			fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, (float)inverters[inv]->Iac1/1000, 0, cfg->precision, cfg->decimalpoint));
			fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, (float)inverters[inv]->Iac2/1000, 0, cfg->precision, cfg->decimalpoint));
			fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, (float)inverters[inv]->Iac3/1000, 0, cfg->precision, cfg->decimalpoint));
			fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, (float)inverters[inv]->Uac1/100, 0, cfg->precision, cfg->decimalpoint));
			fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, (float)inverters[inv]->Uac2/100, 0, cfg->precision, cfg->decimalpoint));
			fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, (float)inverters[inv]->Uac3/100, 0, cfg->precision, cfg->decimalpoint));
			fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, (float)inverters[inv]->calPdcTot, 0, cfg->precision, cfg->decimalpoint));
			fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, (float)inverters[inv]->TotalPac, 0, cfg->precision, cfg->decimalpoint));
			fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, inverters[inv]->calEfficiency, 0, cfg->precision, cfg->decimalpoint));
			fprintf(csv, strout, cfg->delimiter, FormatDouble(FormattedFloat, (double)inverters[inv]->EToday/1000, 0, cfg->precision, cfg->decimalpoint));
			fprintf(csv, strout, cfg->delimiter, FormatDouble(FormattedFloat, (double)inverters[inv]->ETotal/1000, 0, cfg->precision, cfg->decimalpoint));
			fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, (float)inverters[inv]->GridFreq/100, 0, cfg->precision, cfg->decimalpoint));
			fprintf(csv, strout, cfg->delimiter, FormatDouble(FormattedFloat, (double)inverters[inv]->OperationTime/3600, 0, cfg->precision, cfg->decimalpoint));
			fprintf(csv, strout, cfg->delimiter, FormatDouble(FormattedFloat, (double)inverters[inv]->FeedInTime/3600, 0, cfg->precision, cfg->decimalpoint));
			fprintf(csv, strout, cfg->delimiter, FormatDouble(FormattedFloat, inverters[inv]->BT_Signal, 0, cfg->precision, cfg->decimalpoint));
			fprintf(csv, strout, cfg->delimiter, tagdefs.getDesc(inverters[inv]->DeviceStatus, "?").c_str());
			fprintf(csv, strout, cfg->delimiter, tagdefs.getDesc(inverters[inv]->GridRelayStatus, "?").c_str());
			fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, (float)inverters[inv]->Temperature/100, 0, cfg->precision, cfg->decimalpoint));
			if (cfg->SpotWebboxHeader == 0)
				fputs("\n", csv);
		}

		if (cfg->SpotWebboxHeader == 1)
			fputs("\n", csv);

		fclose(csv);
	}
	return 0;
}

int ExportEventsToCSV(const Config *cfg, InverterData *inverters[], std::string dt_range_csv)
{
	char msg[80 + MAX_PATH];
	if (VERBOSE_NORMAL) puts("ExportEventsToCSV()");

	FILE *csv;

	//Expand date specifiers in config::outputPath_Events
	std::stringstream csvpath;
	csvpath << strftime_t(cfg->outputPath_Events, time(NULL));
	CreatePath(csvpath.str().c_str());

	csvpath << FOLDER_SEP << cfg->plantname << "-" << (cfg->userGroup == UG_USER ? "User" : "Installer") << "-Events-" << dt_range_csv.c_str() << ".csv";

	if ((csv = fopen(csvpath.str().c_str(), "w+")) == NULL)
	{
		if (cfg->quiet == 0)
		{
			snprintf(msg, sizeof(msg), "Unable to open output file %s\n", csvpath.str().c_str());
			print_error(stdout, PROC_ERROR, msg);
		}
		return -1;
	}
	else
	{
		//Write header when new file has been created
		#ifdef WIN32
		if (filelength(fileno(csv)) == 0)
		#else
		struct stat fStat;
		stat(csvpath.str().c_str(), &fStat);
		if (fStat.st_size == 0)
		#endif
		{
			if (cfg->CSV_ExtendedHeader == 1)
			{
				fprintf(csv, "sep=%c\n", cfg->delimiter);
				fprintf(csv, "Version CSV1|Tool SBFspot%s (%s)|Linebreaks %s|Delimiter %s\n\n", cfg->prgVersion, OS, linebreak2txt(), delim2txt(cfg->delimiter));
			}
			if (cfg->CSV_Header == 1)
			{
				char Header[] = "DeviceType|DeviceLocation|SusyId|SerNo|TimeStamp|EntryId|EventCode|EventType|Category|Group|Tag|OldValue|NewValue|UserGroup\n";
				for (int i = 0; Header[i] != 0; i++)
					if (Header[i]=='|')
						Header[i]=cfg->delimiter;
				fputs(Header, csv);
			}
		}

		for (int inv=0; inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
		{
			// Sort events on ascending Entry_ID
			std::sort(inverters[inv]->eventData.begin(), inverters[inv]->eventData.end(), SortEntryID_Asc);

			for (vector<EventData>::iterator it=inverters[inv]->eventData.begin(); it!=inverters[inv]->eventData.end(); ++it)
			{

				fprintf(csv, "%s%c", inverters[inv]->DeviceType, cfg->delimiter);
				fprintf(csv, "%s%c", inverters[inv]->DeviceName, cfg->delimiter);
				fprintf(csv, "%d%c", it->SUSyID(), cfg->delimiter);
				fprintf(csv, "%u%c", it->SerNo(), cfg->delimiter);
				fprintf(csv, "%s%c", strftime_t(cfg->DateTimeFormat, it->DateTime()), cfg->delimiter);
				fprintf(csv, "%d%c", it->EntryID(), cfg->delimiter);
				fprintf(csv, "%d%c", it->EventCode(), cfg->delimiter);
				fprintf(csv, "%s%c", it->EventType().c_str(), cfg->delimiter);
				fprintf(csv, "%s%c", it->EventCategory().c_str(), cfg->delimiter);
				fprintf(csv, "%s%c", tagdefs.getDesc(it->Group()).c_str(), cfg->delimiter);
				string EventDescription = tagdefs.getDesc(it->Tag());

				// If description contains "%s", replace it with localized parameter
				if (EventDescription.find("%s"))
					fprintf(csv, EventDescription.c_str(), tagdefs.getDescForLRI(it->Parameter()).c_str());
				else
					fprintf(csv, "%s", EventDescription.c_str());

				fprintf(csv, "%c", cfg->delimiter);

				// As an extra: export old and new values
				// This is "forgotten" in Sunny Explorer
				switch (it->DataType())
				{
				case 0x08: // Status
					fprintf(csv, "%s%c", tagdefs.getDesc(it->OldVal() & 0xFFFF).c_str(), cfg->delimiter);
					fprintf(csv, "%s%c", tagdefs.getDesc(it->NewVal() & 0xFFFF).c_str(), cfg->delimiter);
					break;

				case 0x00: // Unsigned int
					fprintf(csv, "%u%c", it->OldVal(), cfg->delimiter);
					fprintf(csv, "%u%c", it->NewVal(), cfg->delimiter);
					break;

				case 0x40: // Signed int
					fprintf(csv, "%d%c", it->OldVal(), cfg->delimiter);
					fprintf(csv, "%d%c", it->NewVal(), cfg->delimiter);
					break;

				case 0x10: // String
					fprintf(csv, "%08X%c", it->OldVal(), cfg->delimiter);
					fprintf(csv, "%08X%c", it->NewVal(), cfg->delimiter);
					break;

				default:
					fprintf(csv, "%c%c", cfg->delimiter, cfg->delimiter);
				}

				// As an extra: User or Installer Event
				fprintf(csv, "%s\n", tagdefs.getDesc(it->UserGroupTagID()).c_str());
			}
		}

		fclose(csv);
	}

	return 0;
}

int ExportBatteryDataToCSV(Config *cfg, InverterData *inverters[])
{
	char msg[80 + MAX_PATH];
	if (VERBOSE_NORMAL) puts("ExportBatteryDataToCSV()");

	FILE *csv;
	
	time_t spottime = time(NULL);

	//Expand date specifiers in config::outputPath
	std::stringstream csvpath;
	csvpath << strftime_t(cfg->outputPath, spottime);
	CreatePath(csvpath.str().c_str());

	csvpath << FOLDER_SEP << cfg->plantname << "-Battery-" << strftime_t("%Y%m%d", spottime) << ".csv";

	if ((csv = fopen(csvpath.str().c_str(), "a+")) == NULL)
	{
		if (cfg->quiet == 0)
		{
			snprintf(msg, sizeof(msg), "Unable to open output file %s\n", csvpath.str().c_str());
			print_error(stdout, PROC_ERROR, msg);
		}
		return -1;
	}
	else
	{
		//Write header when new file has been created
		#ifdef WIN32
		if (filelength(fileno(csv)) == 0)
		#else
		struct stat fStat;
		stat(csvpath.str().c_str(), &fStat);
		if (fStat.st_size == 0)
		#endif
		{
			if (cfg->SpotWebboxHeader == 0)
				WriteStandardHeader(csv, cfg, BatteryInverter);
			else
				WriteWebboxHeader(csv, cfg, inverters);
		}

		char FormattedFloat[32];
		const char *strout = "%c%s";

		if (cfg->SpotWebboxHeader == 1)
			fputs(strftime_t(cfg->DateTimeFormat, spottime), csv);

		for (int inv=0; inverters[inv]!=NULL && inv<MAX_INVERTERS; inv++)
		{
			if (cfg->SpotWebboxHeader == 0)
			{
				fputs(strftime_t(cfg->DateTimeFormat, spottime), csv);
				fprintf(csv, strout, cfg->delimiter, inverters[inv]->DeviceName);
				fprintf(csv, strout, cfg->delimiter, inverters[inv]->DeviceType);
				fprintf(csv, "%c%lu", cfg->delimiter, inverters[inv]->Serial);
			}

			fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, ((float)inverters[inv]->Pac1), 0, cfg->precision, cfg->decimalpoint));
			fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, ((float)inverters[inv]->Pac2), 0, cfg->precision, cfg->decimalpoint));
			fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, ((float)inverters[inv]->Pac3), 0, cfg->precision, cfg->decimalpoint));
			fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, ((float)inverters[inv]->Uac1)/100, 0, cfg->precision, cfg->decimalpoint));
			fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, ((float)inverters[inv]->Uac2)/100, 0, cfg->precision, cfg->decimalpoint));
			fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, ((float)inverters[inv]->Uac3)/100, 0, cfg->precision, cfg->decimalpoint));
			fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, ((float)inverters[inv]->Iac1)/1000, 0, cfg->precision, cfg->decimalpoint));
			fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, ((float)inverters[inv]->Iac2)/1000, 0, cfg->precision, cfg->decimalpoint));
			fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, ((float)inverters[inv]->Iac3)/1000, 0, cfg->precision, cfg->decimalpoint));
			fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, ((float)inverters[inv]->TotalPac), 0, cfg->precision, cfg->decimalpoint));
			fprintf(csv, strout, cfg->delimiter, FormatDouble(FormattedFloat, ((double)inverters[inv]->EToday)/1000, 0, cfg->precision, cfg->decimalpoint));
			fprintf(csv, strout, cfg->delimiter, FormatDouble(FormattedFloat, ((double)inverters[inv]->ETotal)/1000, 0, cfg->precision, cfg->decimalpoint));
			fprintf(csv, strout, cfg->delimiter, FormatDouble(FormattedFloat, ((double)inverters[inv]->GridFreq)/100, 0, cfg->precision, cfg->decimalpoint));
			fprintf(csv, strout, cfg->delimiter, FormatDouble(FormattedFloat, ((double)inverters[inv]->OperationTime)/3600, 0, cfg->precision, cfg->decimalpoint));
			fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, ((float)inverters[inv]->FeedInTime)/3600, 0, cfg->precision, cfg->decimalpoint));
			fprintf(csv, strout, cfg->delimiter, tagdefs.getDesc(inverters[inv]->DeviceStatus, "?").c_str());
			fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, ((float)inverters[inv]->BatChaStt), 0, cfg->precision, cfg->decimalpoint));
			fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, ((float)inverters[inv]->BatTmpVal)/10, 0, cfg->precision, cfg->decimalpoint));
			fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, ((float)inverters[inv]->BatVol)/100, 0, cfg->precision, cfg->decimalpoint));
			fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, ((float)inverters[inv]->BatAmp)/1000, 0, cfg->precision, cfg->decimalpoint));
			fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, ((float)inverters[inv]->MeteringGridMsTotWOut), 0, cfg->precision, cfg->decimalpoint));
			fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, ((float)inverters[inv]->MeteringGridMsTotWIn), 0, cfg->precision, cfg->decimalpoint));
			if (cfg->SpotWebboxHeader == 0)
				fputs("\n", csv);
		}
		if (cfg->SpotWebboxHeader == 1)
			fputs("\n", csv);

		fclose(csv);
	}
	return 0;
}




// Undocumented - For WebSolarLog usage only
// WSL needs unlocalized strings
const char *WSL_AttributeToText(int attribute)
{
	switch (attribute)
	{
		//Grid Relay Status
		case 311: return "Open";
		case 51: return "Closed";

		//Device Status
		case 307: return "OK";
		case 455: return "Warning";
		case 35: return "Fault";

		default: return "?";
	}
}

//Undocumented - For WebSolarLog usage only
//TODO:
//Version 2.1.0 (multi inverter plant) takes only values from first inverter
//Must be calculated for ALL inverters - Align with WSL developers
int ExportSpotDataToWSL(const Config *cfg, InverterData *inverters[])
{
	if (VERBOSE_NORMAL) puts("ExportSpotDataToWSL()");

	time_t spottime = inverters[0]->InverterDatetime;
	if (cfg->SpotTimeSource == 1) // Take time from computer instead of inverter
		time(&spottime);


	char FormattedFloat[32];
	const char *strout = "%s%c";

	printf(strout,"WSL_START", cfg->delimiter);
	printf(strout, strftime_t(cfg->DateTimeFormat, spottime), cfg->delimiter);
	printf(strout, FormatFloat(FormattedFloat, (float)inverters[0]->Pdc1, 0, cfg->precision, cfg->decimalpoint), cfg->delimiter);
	printf(strout, FormatFloat(FormattedFloat, (float)inverters[0]->Pdc2, 0, cfg->precision, cfg->decimalpoint), cfg->delimiter);
	printf(strout, FormatFloat(FormattedFloat, (float)inverters[0]->Idc1/1000, 0, cfg->precision, cfg->decimalpoint), cfg->delimiter);
	printf(strout, FormatFloat(FormattedFloat, (float)inverters[0]->Idc2/1000, 0, cfg->precision, cfg->decimalpoint), cfg->delimiter);
	printf(strout, FormatFloat(FormattedFloat, (float)inverters[0]->Udc1/100, 0, cfg->precision, cfg->decimalpoint), cfg->delimiter);
	printf(strout, FormatFloat(FormattedFloat, (float)inverters[0]->Udc2/100, 0, cfg->precision, cfg->decimalpoint), cfg->delimiter);
	printf(strout, FormatFloat(FormattedFloat, (float)inverters[0]->Pac1, 0, cfg->precision, cfg->decimalpoint), cfg->delimiter);
	printf(strout, FormatFloat(FormattedFloat, (float)inverters[0]->Pac2, 0, cfg->precision, cfg->decimalpoint), cfg->delimiter);
	printf(strout, FormatFloat(FormattedFloat, (float)inverters[0]->Pac3, 0, cfg->precision, cfg->decimalpoint), cfg->delimiter);
	printf(strout, FormatFloat(FormattedFloat, (float)inverters[0]->Iac1/1000, 0, cfg->precision, cfg->decimalpoint), cfg->delimiter);
	printf(strout, FormatFloat(FormattedFloat, (float)inverters[0]->Iac2/1000, 0, cfg->precision, cfg->decimalpoint), cfg->delimiter);
	printf(strout, FormatFloat(FormattedFloat, (float)inverters[0]->Iac3/1000, 0, cfg->precision, cfg->decimalpoint), cfg->delimiter);
	printf(strout, FormatFloat(FormattedFloat, (float)inverters[0]->Uac1/100, 0, cfg->precision, cfg->decimalpoint), cfg->delimiter);
	printf(strout, FormatFloat(FormattedFloat, (float)inverters[0]->Uac2/100, 0, cfg->precision, cfg->decimalpoint), cfg->delimiter);
	printf(strout, FormatFloat(FormattedFloat, (float)inverters[0]->Uac3/100, 0, cfg->precision, cfg->decimalpoint), cfg->delimiter);
	printf(strout, FormatFloat(FormattedFloat, (float)inverters[0]->calPdcTot, 0, cfg->precision, cfg->decimalpoint), cfg->delimiter);
	printf(strout, FormatFloat(FormattedFloat, (float)inverters[0]->TotalPac, 0, cfg->precision, cfg->decimalpoint), cfg->delimiter);
	printf(strout, FormatFloat(FormattedFloat, inverters[0]->calEfficiency, 0, cfg->precision, cfg->decimalpoint), cfg->delimiter);
	printf(strout, FormatDouble(FormattedFloat, (double)inverters[0]->EToday/1000, 0, cfg->precision, cfg->decimalpoint), cfg->delimiter);
	printf(strout, FormatDouble(FormattedFloat, (double)inverters[0]->ETotal/1000, 0, cfg->precision, cfg->decimalpoint), cfg->delimiter);
	printf(strout, FormatFloat(FormattedFloat, (float)inverters[0]->GridFreq/100, 0, cfg->precision, cfg->decimalpoint), cfg->delimiter);
	printf(strout, FormatDouble(FormattedFloat, (double)inverters[0]->OperationTime/3600, 0, cfg->precision, cfg->decimalpoint), cfg->delimiter);
	printf(strout, FormatDouble(FormattedFloat, (double)inverters[0]->FeedInTime/3600, 0, cfg->precision, cfg->decimalpoint), cfg->delimiter);
	printf(strout, FormatDouble(FormattedFloat, inverters[0]->BT_Signal, 0, cfg->precision, cfg->decimalpoint), cfg->delimiter);
	printf(strout, tagdefs.getDesc(inverters[0]->DeviceStatus, "?").c_str(), cfg->delimiter);
	printf(strout, tagdefs.getDesc(inverters[0]->GridRelayStatus, "?").c_str(), cfg->delimiter);
	printf("%s\n", "WSL_END");
	return 0;
}

//Undocumented - For 123Solar Web Solar logger usage only)
int ExportSpotDataTo123s(Config *cfg, InverterData *inverters[])
{
	if (VERBOSE_NORMAL) puts("ExportSpotDataTo123s()");

	// Currently, only data of first inverter is exported
	InverterData *invdata = inverters[0];

	char FormattedFloat[32];
	const char *strout = "%s%c";
	const char *s123_dt_format = "%Y%m%d-%H:%M:%S";
	const char *s123_delimiter = " ";
	const char *s123_decimalpoint = ".";

	//Calculated DC Power Values (Sum of Power per string)
	float calPdcTot = (float)(invdata->Pdc1 + invdata->Pdc2);

	//Calculated AC Side Values (Sum of Power & Current / Maximum of Voltage)
	float calPacTot = (float)(invdata->Pac1 + invdata->Pac2 + invdata->Pac3);
	float calIacTot = (float)(invdata->Iac1 + invdata->Iac2 + invdata->Iac3);
	float calUacMax = (float)max(max(invdata->Uac1, invdata->Uac2), invdata->Uac3);

	//Calculated Inverter Efficiency
	float calEfficiency = calPdcTot == 0 ? 0 : calPacTot / calPdcTot * 100;

	//Select Between Computer & Inverter Time (As Per .cfg Setting)
	time_t spottime = invdata->InverterDatetime;
	if (cfg->SpotTimeSource == 1) // Take time from computer instead of inverter
		time(&spottime);

	//Send Spot Data Frame to 123Solar

	// $SDTE = Date & Time ( YYYYMMDD-HH:MM:SS )
	printf(strout, strftime_t(s123_dt_format, spottime), *s123_delimiter);
	// $G1V  = GridMs.PhV.phsA
	printf(strout, FormatFloat(FormattedFloat, (float)invdata->Uac1/100, 0, cfg->precision, *s123_decimalpoint), *s123_delimiter);
	// $G1A  = GridMs.A.phsA
	printf(strout, FormatFloat(FormattedFloat, (float)invdata->Iac1/1000, 0, cfg->precision, *s123_decimalpoint), *s123_delimiter);
	// $G1P  = GridMs.W.phsA
	printf(strout, FormatFloat(FormattedFloat, (float)invdata->Pac1, 0, cfg->precision, *s123_decimalpoint), *s123_delimiter);
	// $G2V  = GridMs.PhV.phsB
	printf(strout, FormatFloat(FormattedFloat, (float)invdata->Uac2/100, 0, cfg->precision, *s123_decimalpoint), *s123_delimiter);
	// $G2A  = GridMs.A.phsB
	printf(strout, FormatFloat(FormattedFloat, (float)invdata->Iac2/1000, 0, cfg->precision, *s123_decimalpoint), *s123_delimiter);
	// $G2P  = GridMs.W.phsB
	printf(strout, FormatFloat(FormattedFloat, (float)invdata->Pac2, 0, cfg->precision, *s123_decimalpoint), *s123_delimiter);
	// $G3V  = GridMs.PhV.phsC
	printf(strout, FormatFloat(FormattedFloat, (float)invdata->Uac3/100, 0, cfg->precision, *s123_decimalpoint), *s123_delimiter);
	// $G3A  = GridMs.A.phsC
	printf(strout, FormatFloat(FormattedFloat, (float)invdata->Iac3/1000, 0, cfg->precision, *s123_decimalpoint), *s123_delimiter);
	// $G3P  = GridMs.W.phsC
	printf(strout, FormatFloat(FormattedFloat, (float)invdata->Pac3, 0, cfg->precision, *s123_decimalpoint), *s123_delimiter);
	// $FRQ = GridMs.Hz
	printf(strout, FormatFloat(FormattedFloat, (float)invdata->GridFreq/100, 0, cfg->precision, *s123_decimalpoint), *s123_delimiter);
	// $EFF = Value computed by SBFspot
	printf(strout, FormatFloat(FormattedFloat, calEfficiency, 0, cfg->precision, *s123_decimalpoint), *s123_delimiter);
	// $INVT = Inverter temperature 
	printf(strout, FormatFloat(FormattedFloat, (float)invdata->Temperature/100, 0, cfg->precision, *s123_decimalpoint), *s123_delimiter);
	// $BOOT = Booster temperature - n/a for SMA inverters
	printf(strout, FormatFloat(FormattedFloat, 0., 0, cfg->precision, *s123_decimalpoint), *s123_delimiter);
	// $KWHT = Metering.TotWhOut (kWh)
	printf(strout, FormatDouble(FormattedFloat, (double)invdata->ETotal/1000, 0, cfg->precision, *s123_decimalpoint), *s123_delimiter);
	// $I1V = DcMs.Vol[A]
	printf(strout, FormatFloat(FormattedFloat, (float)invdata->Udc1/100, 0, cfg->precision, *s123_decimalpoint), *s123_delimiter);
	// $I1A = DcMs.Amp[A]
	printf(strout, FormatFloat(FormattedFloat, (float)invdata->Idc1/1000, 0, cfg->precision, *s123_decimalpoint), *s123_delimiter);
	// $I1P = DcMs.Watt[A]
	printf(strout, FormatFloat(FormattedFloat, (float)invdata->Pdc1, 0, cfg->precision, *s123_decimalpoint), *s123_delimiter);
	// $I2V = DcMs.Vol[B]
	printf(strout, FormatFloat(FormattedFloat, (float)invdata->Udc2/100, 0, cfg->precision, *s123_decimalpoint), *s123_delimiter);
	// $I2A = DcMs.Amp[B]
	printf(strout, FormatFloat(FormattedFloat, (float)invdata->Idc2/1000, 0, cfg->precision, *s123_decimalpoint), *s123_delimiter);
	// $I2P = DcMs.Watt[B]
	printf(strout, FormatFloat(FormattedFloat, (float)invdata->Pdc2, 0, cfg->precision, *s123_decimalpoint), *s123_delimiter);
	// $GV = Was grid voltage in single phase 123Solar - For backwards compatibility
	printf(strout, FormatFloat(FormattedFloat, (float)calUacMax/100, 0, cfg->precision, *s123_decimalpoint), *s123_delimiter);
	// $GA = Was grid current in single phase 123Solar - For backwards compatibility
	printf(strout, FormatFloat(FormattedFloat, (float)calIacTot/1000, 0, cfg->precision, *s123_decimalpoint), *s123_delimiter);
	// $GP = Was grid power in single phase 123Solar - For backwards compatibility
	printf(strout, FormatFloat(FormattedFloat, (float)calPacTot, 0, cfg->precision, *s123_decimalpoint), *s123_delimiter);
	// OK
	printf("%s\n", ">>>S123:OK");
	return 0;
}

//Undocumented - For 123Solar Web Solar logger usage only)
int ExportInformationDataTo123s(Config *cfg, InverterData *inverters[])
{
	if (VERBOSE_NORMAL) puts("ExportInformationDataTo123s()");

	// Currently, only data of first inverter is exported
	InverterData *invdata = inverters[0];

	const char *s123_dt_format = "%Y%m%d-%H:%M:%S";
	const char *s123_delimiter = "\n";

	//Send Inverter Information File to 123Solar

	// Reader program name & version
	printf("Reader: SBFspot %s%c", cfg->prgVersion, *s123_delimiter);
	// Inverter bluetooth address
	printf("Bluetooth Address: %s%c", cfg->BT_Address, *s123_delimiter);
	// Inverter bluetooth NetID
	printf("Bluetooth NetID: %02X%c", invdata->NetID, *s123_delimiter);
	// Inverter time ( YYYYMMDD-HH:MM:SS )
	printf("Time: %s%c", strftime_t(s123_dt_format, invdata->InverterDatetime), *s123_delimiter);
	// Inverter device name
	printf("Device Name: %s%c", invdata->DeviceName, *s123_delimiter);
	// Inverter device class
	printf("Device Class: %s%c", invdata->DeviceClass, *s123_delimiter);
	// Inverter device type
	printf("Device Type: %s%c", invdata->DeviceType, *s123_delimiter);
	// Inverter serial number
	printf("Serial Number: %lu%c", invdata->Serial, *s123_delimiter);
	// Inverter firmware version
	printf("Firmware: %s%c", invdata->SWVersion, *s123_delimiter);
	// Inverter phase maximum Pac
	if (invdata->Pmax1 > 0) printf("Pac Max Phase 1: %luW%c", invdata->Pmax1, *s123_delimiter);
	if (invdata->Pmax2 > 0) printf("Pac Max Phase 2: %luW%c", invdata->Pmax2, *s123_delimiter);
	if (invdata->Pmax3 > 0) printf("Pac Max Phase 3: %luW%c", invdata->Pmax3, *s123_delimiter);
	// Inverter wake-up & sleep times
	printf("Wake-Up Time: %s%c", strftime_t(s123_dt_format, invdata->WakeupTime), *s123_delimiter);
	printf("Sleep Time: %s\n", strftime_t(s123_dt_format, invdata->SleepTime));
	return 0;
}

//Undocumented - For 123Solar Web Solar logger usage only)
int ExportStateDataTo123s(Config *cfg, InverterData *inverters[])
{
	if (VERBOSE_NORMAL) puts("ExportStateDataTo123s()");

	// Currently, only data of first inverter is exported
	InverterData *invdata = inverters[0];

	char FormattedFloat[32];
	const char *s123_dt_format = "%Y%m%d-%H:%M:%S";
	const char *s123_delimiter = "\n";
	const char *s123_decimalpoint = ".";

	//Send Inverter State Data to 123Solar

	// Inverter time ( YYYYMMDD-HH:MM:SS )
	printf("Inverter Time: %s%c", strftime_t(s123_dt_format, invdata->InverterDatetime), *s123_delimiter);
	// Inverter device name
	printf("Device Name: %s%c", invdata->DeviceName, *s123_delimiter);
	// Device status
	printf("Device Status: %s%c", tagdefs.getDesc(invdata->DeviceStatus, "?").c_str(), *s123_delimiter);
	// Grid relay status
	printf("GridRelay Status: %s%c", tagdefs.getDesc(invdata->GridRelayStatus, "?").c_str(), *s123_delimiter);
	// Operation time
	printf("Operation Time: %s%c", FormatDouble(FormattedFloat, (double)invdata->OperationTime/3600, 0, cfg->precision, *s123_decimalpoint), *s123_delimiter);
	// Feed-in time
	printf("Feed-In Time: %s%c", FormatDouble(FormattedFloat, (double)invdata->FeedInTime/3600, 0, cfg->precision, *s123_decimalpoint), *s123_delimiter);
	// Bluetooth signal
	printf("Bluetooth Signal: %s\n", FormatDouble(FormattedFloat, invdata->BT_Signal, 0, cfg->precision, *s123_decimalpoint));
	return 0;
}
