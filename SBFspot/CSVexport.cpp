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

#include "CSVexport.h"
#include "EventData.h"
#include <algorithm> // std::max
#include "mppt.h"

//DecimalPoint To Text
const std::string dp2txt(const char dp)
{
    if (dp == '.') return "dot";
    if (dp == ',') return "comma";
    return "?";
}

//Delimiter To Text
const std::string delim2txt(const char delim)
{
    if (delim == ';') return "semicolon";
    if (delim == ',') return "comma";
    return "?";
}

//Linebreak To Text
const std::string linebreak2txt(void)
{
#if defined(_WIN32)
    return "CR/LF";
#endif

#if defined (__linux__)
    return "LF";
#endif
}

// Convert format string like %d/%m/%Y %H:%M:%S to dd/mm/yyyy HH:mm:ss
const std::string DateTimeFormatToDMY(const char *dtf)
{
    std::string DMY;
    for (int i = 0; dtf[i] != 0; i++)
    {
        if (dtf[i] == '%')
        {
            i++;
            switch (dtf[i])
            {
            case 0: // end-of-string
                break;
            case 'y':
                DMY += "yy"; break;
            case 'Y':
                DMY += "yyyy"; break;
            case 'm':
                DMY += "MM"; break;
            case 'd':
                DMY += "dd"; break;
            case 'H':
                DMY += "HH"; break;
            case 'M':
                DMY += "mm"; break;
            case 'S':
                DMY += "ss"; break;
            default:
                DMY += dtf[i];
            }
        }
        else
        {
            DMY += dtf[i];
        }
    }

    return DMY;
}

size_t max_mppt(InverterData* const inverters[])
{
    size_t mppt_max = 0;

    for (size_t inv = 0; inverters[inv] != NULL && inv < MAX_INVERTERS; inv++)
    {
        mppt_max = std::max(inverters[inv]->mpp.size(), mppt_max);
    }

    return mppt_max;
}

int ExportProperties(FILE *csv, const Config *cfg)
{
    return fprintf(csv, "sep=%c\nVersion CSV1|Tool SBFspot%s (%s)|Linebreaks %s|Delimiter %s|Decimalpoint %s|Precision %d\n\n", cfg->delimiter, cfg->prgVersion, OS, linebreak2txt().c_str(), delim2txt(cfg->delimiter).c_str(), dp2txt(cfg->decimalpoint).c_str() , cfg->precision);
}

int ExportMonthDataToCSV(const Config *cfg, InverterData* const inverters[])
{
    char msg[80 + MAX_PATH];
    if (cfg->CSV_Export)
    {
        if (inverters[0]->monthData[0].datetime <= 0)   //invalid date?
        {
            if (!cfg->quiet) puts("ExportMonthDataToCSV: There is no data to export!"); //First day of the month?
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
                if (!cfg->quiet)
                {
                    snprintf(msg, sizeof(msg), "Unable to open output file %s\n", csvpath.str().c_str());
                    print_error(stdout, PROC_ERROR, msg);
                }
                return -1;
            }
            else
            {
                if (cfg->CSV_ExtendedHeader)
                {
                    ExportProperties(csv, cfg);

                    for (uint32_t inv = 0; inverters[inv] != NULL && inv<MAX_INVERTERS; inv++)
                        fprintf(csv, "%c%s%c%s", cfg->delimiter, inverters[inv]->DeviceName.c_str(), cfg->delimiter, inverters[inv]->DeviceName.c_str());
                    fputs("\n", csv);
                    for (uint32_t inv = 0; inverters[inv] != NULL && inv<MAX_INVERTERS; inv++)
                        fprintf(csv, "%c%s%c%s", cfg->delimiter, inverters[inv]->DeviceType.c_str(), cfg->delimiter, inverters[inv]->DeviceType.c_str());
                    fputs("\n", csv);
                    for (uint32_t inv = 0; inverters[inv] != NULL && inv<MAX_INVERTERS; inv++)
                        fprintf(csv, "%c%lu%c%lu", cfg->delimiter, inverters[inv]->Serial, cfg->delimiter, inverters[inv]->Serial);
                    fputs("\n", csv);
                    for (uint32_t inv = 0; inverters[inv] != NULL && inv<MAX_INVERTERS; inv++)
                        fprintf(csv, "%cTotal yield%cDay yield", cfg->delimiter, cfg->delimiter);
                    fputs("\n", csv);
                    for (uint32_t inv = 0; inverters[inv] != NULL && inv<MAX_INVERTERS; inv++)
                        fprintf(csv, "%cCounter%cAnalog", cfg->delimiter, cfg->delimiter);
                    fputs("\n", csv);
                }
                if (cfg->CSV_Header)
                {
                    fprintf(csv, "%s", DateTimeFormatToDMY(cfg->DateFormat).c_str());
                    for (uint32_t inv = 0; inverters[inv] != NULL && inv<MAX_INVERTERS; inv++)
                        fprintf(csv, "%ckWh%ckWh", cfg->delimiter, cfg->delimiter);
                    fputs("\n", csv);
                }
            }

            char FormattedFloat[16];

            for (unsigned int idx = 0; idx<sizeof(inverters[0]->monthData) / sizeof(MonthData); idx++)
            {
                time_t datetime = 0;
                for (uint32_t inv = 0; inverters[inv] != NULL && inv<MAX_INVERTERS; inv++)
                    if (inverters[inv]->monthData[idx].datetime > 0)
                        datetime = inverters[inv]->monthData[idx].datetime;

                if (datetime > 0)
                {
                    fprintf(csv, "%s", strfgmtime_t(cfg->DateFormat, datetime));
                    for (uint32_t inv = 0; inverters[inv] != NULL && inv<MAX_INVERTERS; inv++)
                    {
                        fprintf(csv, "%c%s", cfg->delimiter, FormatDouble(FormattedFloat, (double)inverters[inv]->monthData[idx].totalWh / 1000, 0, cfg->precision, cfg->decimalpoint));
                        fprintf(csv, "%c%s", cfg->delimiter, FormatDouble(FormattedFloat, (double)inverters[inv]->monthData[idx].dayWh / 1000, 0, cfg->precision, cfg->decimalpoint));
                    }
                    fputs("\n", csv);
                }
            }
            fclose(csv);
        }
    }
    return 0;
}

int ExportDayDataToCSV(const Config *cfg, InverterData* const inverters[])
{
    char msg[80 + MAX_PATH];

    FILE *csv;

    //fix 1.3.1 for inverters with BT piggyback (missing interval data in the dark)
    //need to find first valid date in array
    time_t date;
    unsigned int idx = 0;
    do
    {
        date = inverters[0]->dayData[idx++].datetime;
    } while ((idx < sizeof(inverters[0]->dayData) / sizeof(DayData)) && (date == 0));

    // Fix Issue 90: SBFspot still creating 1970 .csv files
    if (date == 0) return 0;	// Nothing to export! Silently exit.

    //Expand date specifiers in config::outputPath
    std::stringstream csvpath;
    csvpath << strftime_t(cfg->outputPath, date);
    CreatePath(csvpath.str().c_str());

    csvpath << FOLDER_SEP << cfg->plantname << "-" << strftime_t("%Y%m%d", date) << ".csv";

    if ((csv = fopen(csvpath.str().c_str(), "w+")) == NULL)
    {
        if (!cfg->quiet)
        {
            snprintf(msg, sizeof(msg), "Unable to open output file %s\n", csvpath.str().c_str());
            print_error(stdout, PROC_ERROR, msg);
        }
        return -1;
    }
    else
    {
        if (cfg->CSV_ExtendedHeader)
        {
            ExportProperties(csv, cfg);

            for (uint32_t inv = 0; inverters[inv] != NULL && inv<MAX_INVERTERS; inv++)
                fprintf(csv, "%c%s%c%s", cfg->delimiter, inverters[inv]->DeviceName.c_str(), cfg->delimiter, inverters[inv]->DeviceName.c_str());
            fputs("\n", csv);
            for (uint32_t inv = 0; inverters[inv] != NULL && inv<MAX_INVERTERS; inv++)
                fprintf(csv, "%c%s%c%s", cfg->delimiter, inverters[inv]->DeviceType.c_str(), cfg->delimiter, inverters[inv]->DeviceType.c_str());
            fputs("\n", csv);
            for (uint32_t inv = 0; inverters[inv] != NULL && inv<MAX_INVERTERS; inv++)
                fprintf(csv, "%c%lu%c%lu", cfg->delimiter, inverters[inv]->Serial, cfg->delimiter, inverters[inv]->Serial);
            fputs("\n", csv);
            for (uint32_t inv = 0; inverters[inv] != NULL && inv<MAX_INVERTERS; inv++)
                fprintf(csv, "%cTotal yield%cPower", cfg->delimiter, cfg->delimiter);
            fputs("\n", csv);
            for (uint32_t inv = 0; inverters[inv] != NULL && inv<MAX_INVERTERS; inv++)
                fprintf(csv, "%cCounter%cAnalog", cfg->delimiter, cfg->delimiter);
            fputs("\n", csv);
        }
        if (cfg->CSV_Header)
        {
            fputs(DateTimeFormatToDMY(cfg->DateTimeFormat).c_str(), csv);
            for (uint32_t inv = 0; inverters[inv] != NULL && inv<MAX_INVERTERS; inv++)
                fprintf(csv, "%ckWh%ckW", cfg->delimiter, cfg->delimiter);
            fputs("\n", csv);
        }
    }

    char FormattedFloat[16];

    for (unsigned int dd = 0; dd < sizeof(inverters[0]->dayData) / sizeof(DayData); dd++)
    {
        time_t datetime = 0;
        unsigned long long totalPower = 0;
        for (uint32_t inv = 0; inverters[inv] != NULL && inv<MAX_INVERTERS; inv++)
            if (inverters[inv]->dayData[dd].datetime > 0)
            {
                datetime = inverters[inv]->dayData[dd].datetime;
                totalPower += inverters[inv]->dayData[dd].watt;
            }

        if (datetime > 0)
        {
            if ((cfg->CSV_SaveZeroPower) || (totalPower > 0))
            {
                fprintf(csv, "%s", strftime_t(cfg->DateTimeFormat, datetime));
                for (uint32_t inv = 0; inverters[inv] != NULL && inv<MAX_INVERTERS; inv++)
                {
                    fprintf(csv, "%c%s", cfg->delimiter, FormatDouble(FormattedFloat, (double)inverters[inv]->dayData[dd].totalWh / 1000, 0, cfg->precision, cfg->decimalpoint));
                    fprintf(csv, "%c%s", cfg->delimiter, FormatDouble(FormattedFloat, (double)inverters[inv]->dayData[dd].watt / 1000, 0, cfg->precision, cfg->decimalpoint));
                }
                fputs("\n", csv);
            }
        }
    }
    fclose(csv);
    return 0;
}

int WriteStandardHeader(FILE *csv, const Config *cfg, DEVICECLASS devclass, const size_t num_mppt)
{
    std::string exthdr("|||");
    std::string stdhdr("|DeviceName|DeviceType|Serial");

    if (devclass == BatteryInverter)
    {
        exthdr += "|Watt|Watt|Watt|Amp|Amp|Amp|Volt|Volt|Volt|Watt|kWh|kWh|Hz|hours|hours|Status|%|degC|Volt|Amp|Watt|Watt\n";
        stdhdr += "|Pac1|Pac2|Pac3|Iac1|Iac2|Iac3|Uac1|Uac2|Uac3|PacTot|EToday|ETotal|Frequency|OperatingTime|FeedInTime|Condition|SOC|Tempbatt|Ubatt|Ibatt|TotWOut|TotWIn\n";
    }
    else
    {
        for (size_t n = 1; n <= num_mppt; ++n) exthdr += "|Watt";
        for (size_t n = 1; n <= num_mppt; ++n) exthdr += "|Amp";
        for (size_t n = 1; n <= num_mppt; ++n) exthdr += "|Volt";
        exthdr += "|Watt|Watt|Watt|Amp|Amp|Amp|Volt|Volt|Volt|Watt|Watt|%|kWh|kWh|Hz|Hours|Hours|%|Status|Status|degC\n";

        for (size_t n = 1; n <= num_mppt; ++n) stdhdr += "|Pdc" + std::to_string(n);
        for (size_t n = 1; n <= num_mppt; ++n) stdhdr += "|Idc" + std::to_string(n);
        for (size_t n = 1; n <= num_mppt; ++n) stdhdr += "|Udc" + std::to_string(n);
        stdhdr += "|Pac1|Pac2|Pac3|Iac1|Iac2|Iac3|Uac1|Uac2|Uac3|PdcTot|PacTot|Efficiency|EToday|ETotal|Frequency|OperatingTime|FeedInTime|BT_Signal|Condition|GridRelay|Temperature\n";
    }

    if (cfg->CSV_ExtendedHeader)
    {
        ExportProperties(csv, cfg);
        std::replace(exthdr.begin(), exthdr.end(), '|', cfg->delimiter);
        fputs(exthdr.c_str(), csv);
    }

    if (cfg->CSV_Header)
    {
        fputs(DateTimeFormatToDMY(cfg->DateTimeFormat).c_str(), csv);
        std::replace(stdhdr.begin(), stdhdr.end(), '|', cfg->delimiter);
        fputs(stdhdr.c_str(), csv);
    }

    return 0;
}

int WriteWebboxHeader(FILE *csv, const Config *cfg, InverterData* const inverters[], const size_t num_mppt)
{
    std::string hdr1, hdr2, hdr3;

    if (inverters[0]->DevClass == BatteryInverter)
    {
        hdr1 = "|GridMs.W.phsA|GridMs.W.phsB|GridMs.W.phsC|GridMs.A.phsA|GridMs.A.phsB|GridMs.A.phsC|GridMs.PhV.phsA|GridMs.PhV.phsB|GridMs.PhV.phsC|GridMs.TotW|Metering.DykWh|Metering.TotWhOut|GridMs.Hz|Metering.TotOpTms|Metering.TotFeedTms|Operation.Health|Bat.ChaStt|Bat.TmpVal|Bat.Vol|Bat.Amp|Metering.GridMs.TotWOut|Metering.GridMs.TotWIn";
        hdr2 = "|Analog|Analog|Analog|Analog|Analog|Analog|Analog|Analog|Analog|Analog|Counter|Counter|Analog|Counter|Counter|Status|Analog|Analog|Analog|Analog|Analog|Analog";
        hdr3 = "|Watt|Watt|Watt|Amp|Amp|Amp|Volt|Volt|Volt|Watt|kWh|kWh|Hz|Hours|Hours||%|degC|Volt|Amp|Watt|Watt";
    }
    else
    {
        for (size_t n = 1; n <= num_mppt; ++n) hdr1 += "|DcMs.Watt[" + std::string(1, '@' + (char)n) + ']';
        for (size_t n = 1; n <= num_mppt; ++n) hdr1 += "|DcMs.Amp[" + std::string(1, '@' + (char)n) + ']';
        for (size_t n = 1; n <= num_mppt; ++n) hdr1 += "|DcMs.Vol[" + std::string(1, '@' + (char)n) + ']';
        hdr1 += "|GridMs.W.phsA|GridMs.W.phsB|GridMs.W.phsC|GridMs.A.phsA|GridMs.A.phsB|GridMs.A.phsC|GridMs.PhV.phsA|GridMs.PhV.phsB|GridMs.PhV.phsC|SS_PdcTot|GridMs.TotW|SBFspot.Efficiency|Metering.DykWh|Metering.TotWhOut|GridMs.Hz|Metering.TotOpTms|Metering.TotFeedTms|SBFspot.BTSignal|Operation.Health|Operation.GriSwStt|Coolsys.TmpNom";

        for (size_t n = 1; n <= num_mppt * 3; ++n) hdr2 += "|Analog";
        hdr2 += "|Analog|Analog|Analog|Analog|Analog|Analog|Analog|Analog|Analog|Counter|Counter|Analog|Counter|Counter|Analog|Counter|Counter|Analog|Status|Status|Analog";

        for (size_t n = 1; n <= num_mppt; ++n) hdr3 += "|Watt";
        for (size_t n = 1; n <= num_mppt; ++n) hdr3 += "|Amp";
        for (size_t n = 1; n <= num_mppt; ++n) hdr3 += "|Volt";
        hdr3 += "|Watt|Watt|Watt|Amp|Amp|Amp|Volt|Volt|Volt|Watt|Watt|%|kWh|kWh|Hz|Hours|Hours|%|||degC";
    }

    if (cfg->CSV_ExtendedHeader)
    {
        ExportProperties(csv, cfg);

        auto colcnt = std::count(hdr1.begin(), hdr1.end(), '|');

        std::replace(hdr1.begin(), hdr1.end(), '|', cfg->delimiter);

        for (uint32_t inv = 0; inverters[inv] != NULL && inv < MAX_INVERTERS; inv++)
        {
            for (int i = 0; i < colcnt; i++)
                fprintf(csv, "%c%s", cfg->delimiter, inverters[inv]->DeviceName.c_str());
        }

        fputs("\n", csv);

        for (uint32_t inv = 0; inverters[inv] != NULL && inv < MAX_INVERTERS; inv++)
        {
            for (int i = 0; i < colcnt; i++)
                fprintf(csv, "%c%s", cfg->delimiter, inverters[inv]->DeviceType.c_str());
        }
        
        fputs("\n", csv);

        for (uint32_t inv = 0; inverters[inv] != NULL && inv < MAX_INVERTERS; inv++)
        {
            for (int i = 0; i < colcnt; i++)
                fprintf(csv, "%c%lu", cfg->delimiter, inverters[inv]->Serial);
        }
        
        fputs("\n", csv);
    }

    if (cfg->CSV_Header)
    {
        fputs("TimeStamp", csv);
        
        for (uint32_t inv = 0; inverters[inv] != NULL && inv<MAX_INVERTERS; inv++)
            fputs(hdr1.c_str(), csv);
        
        fputs("\n", csv);
    }


    if (cfg->CSV_ExtendedHeader)
    {
        std::replace(hdr2.begin(), hdr2.end(), '|', cfg->delimiter);

        for (uint32_t inv = 0; inverters[inv] != NULL && inv<MAX_INVERTERS; inv++)
            fputs(hdr2.c_str(), csv);

        fputs("\n", csv);

        fputs(DateTimeFormatToDMY(cfg->DateTimeFormat).c_str(), csv);

        std::replace(hdr3.begin(), hdr3.end(), '|', cfg->delimiter);

        for (uint32_t inv = 0; inverters[inv] != NULL && inv<MAX_INVERTERS; inv++)
            fputs(hdr3.c_str(), csv);

        fputs("\n", csv);
    }

    return 0;
}

int ExportSpotDataToCSV(const Config *cfg, InverterData* const inverters[])
{
    char msg[80 + MAX_PATH];
    FILE *csv;

    // Take time from computer instead of inverter
    time_t spottime = cfg->SpotTimeSource ? time(nullptr) : inverters[0]->InverterDatetime;

    //Expand date specifiers in config::outputPath
    std::stringstream csvpath;
    csvpath << strftime_t(cfg->outputPath, spottime);
    CreatePath(csvpath.str().c_str());

    csvpath << FOLDER_SEP << cfg->plantname << "-Spot-" << strftime_t("%Y%m%d", spottime) << ".csv";

    if ((csv = fopen(csvpath.str().c_str(), "a+")) == NULL)
    {
        if (!cfg->quiet)
        {
            snprintf(msg, sizeof(msg), "Unable to open output file %s\n", csvpath.str().c_str());
            print_error(stdout, PROC_ERROR, msg);
        }
        return -1;
    }
    else
    {
        size_t maxmppt = max_mppt(inverters);   // Max mppt of plant

        //Write header when new file has been created
#if defined(_WIN32)
        if (filelength(fileno(csv)) == 0)
#else
        struct stat fStat;
        stat(csvpath.str().c_str(), &fStat);
        if (fStat.st_size == 0)
#endif
        {
            if (cfg->SpotWebboxHeader)
                WriteWebboxHeader(csv, cfg, inverters, maxmppt);
            else
                WriteStandardHeader(csv, cfg, SolarInverter, maxmppt);
        }

        char FormattedFloat[32];
        const char *strout = "%c%s";

        if (cfg->SpotWebboxHeader)
            fputs(strftime_t(cfg->DateTimeFormat, spottime), csv);

        for (uint32_t inv = 0; inverters[inv] != NULL && inv < MAX_INVERTERS; inv++)
        {
            if (inverters[inv]->DevClass == SolarInverter)
            {
                if (!cfg->SpotWebboxHeader)
                {
                    fputs(strftime_t(cfg->DateTimeFormat, spottime), csv);
                    fprintf(csv, strout, cfg->delimiter, inverters[inv]->DeviceName.c_str());
                    fprintf(csv, strout, cfg->delimiter, inverters[inv]->DeviceType.c_str());
                    fprintf(csv, "%c%lu", cfg->delimiter, inverters[inv]->Serial);
                }

                // Fix #520
                // If inverter has less mppt than max mppt in plant, reserve space in CSV for missing ones
                std::string missing_mppt(maxmppt - inverters[inv]->mpp.size(), cfg->delimiter);

                for (const auto &mpp : inverters[inv]->mpp)
                {
                    fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, mpp.second.Watt(), 0, cfg->precision, cfg->decimalpoint));
                }
                fputs(missing_mppt.c_str(), csv);

                for (const auto &mpp : inverters[inv]->mpp)
                {
                    fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, mpp.second.Amp(), 0, cfg->precision, cfg->decimalpoint));
                }
                fputs(missing_mppt.c_str(), csv);

                for (const auto &mpp : inverters[inv]->mpp)
                {
                    fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, mpp.second.Volt(), 0, cfg->precision, cfg->decimalpoint));
                }
                fputs(missing_mppt.c_str(), csv);

                fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, (float)inverters[inv]->Pac1, 0, cfg->precision, cfg->decimalpoint));
                fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, (float)inverters[inv]->Pac2, 0, cfg->precision, cfg->decimalpoint));
                fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, (float)inverters[inv]->Pac3, 0, cfg->precision, cfg->decimalpoint));
                fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, (float)inverters[inv]->Iac1 / 1000, 0, cfg->precision, cfg->decimalpoint));
                fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, (float)inverters[inv]->Iac2 / 1000, 0, cfg->precision, cfg->decimalpoint));
                fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, (float)inverters[inv]->Iac3 / 1000, 0, cfg->precision, cfg->decimalpoint));
                fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, (float)inverters[inv]->Uac1 / 100, 0, cfg->precision, cfg->decimalpoint));
                fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, (float)inverters[inv]->Uac2 / 100, 0, cfg->precision, cfg->decimalpoint));
                fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, (float)inverters[inv]->Uac3 / 100, 0, cfg->precision, cfg->decimalpoint));
                fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, (float)inverters[inv]->calPdcTot, 0, cfg->precision, cfg->decimalpoint));
                fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, (float)inverters[inv]->TotalPac, 0, cfg->precision, cfg->decimalpoint));
                fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, inverters[inv]->calEfficiency, 0, cfg->precision, cfg->decimalpoint));
                fprintf(csv, strout, cfg->delimiter, FormatDouble(FormattedFloat, (double)inverters[inv]->EToday / 1000, 0, cfg->precision, cfg->decimalpoint));
                fprintf(csv, strout, cfg->delimiter, FormatDouble(FormattedFloat, (double)inverters[inv]->ETotal / 1000, 0, cfg->precision, cfg->decimalpoint));
                fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, (float)inverters[inv]->GridFreq / 100, 0, cfg->precision, cfg->decimalpoint));
                fprintf(csv, strout, cfg->delimiter, FormatDouble(FormattedFloat, (double)inverters[inv]->OperationTime / 3600, 0, cfg->precision, cfg->decimalpoint));
                fprintf(csv, strout, cfg->delimiter, FormatDouble(FormattedFloat, (double)inverters[inv]->FeedInTime / 3600, 0, cfg->precision, cfg->decimalpoint));
                if (inverters[inv]->BT_Signal == 0)
                    fprintf(csv, strout, cfg->delimiter, NA);
                else
                    fprintf(csv, strout, cfg->delimiter, FormatDouble(FormattedFloat, inverters[inv]->BT_Signal, 0, cfg->precision, cfg->decimalpoint));
                fprintf(csv, strout, cfg->delimiter, tagdefs.getDesc(inverters[inv]->DeviceStatus, "?").c_str());
                fprintf(csv, strout, cfg->delimiter, tagdefs.getDesc(inverters[inv]->GridRelayStatus, "?").c_str());
                if (is_NaN(inverters[inv]->Temperature))
                    fprintf(csv, strout, cfg->delimiter, NA);
                else
                    fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, (float)inverters[inv]->Temperature / 100, 0, cfg->precision, cfg->decimalpoint));
                if (!cfg->SpotWebboxHeader)
                    fputs("\n", csv);
            }
            if (cfg->SpotWebboxHeader)
                fputs("\n", csv);
        }
        fclose(csv);
    }
    return 0;
}

int ExportEventsToCSV(const Config *cfg, InverterData* const inverters[], std::string dt_range_csv)
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
        if (!cfg->quiet)
        {
            snprintf(msg, sizeof(msg), "Unable to open output file %s\n", csvpath.str().c_str());
            print_error(stdout, PROC_ERROR, msg);
        }
        return -1;
    }
    else
    {
        //Write header when new file has been created
#if defined(_WIN32)
        if (filelength(fileno(csv)) == 0)
#else
        struct stat fStat;
        stat(csvpath.str().c_str(), &fStat);
        if (fStat.st_size == 0)
#endif
        {
            if (cfg->CSV_ExtendedHeader)
            {
                ExportProperties(csv, cfg);
            }
            if (cfg->CSV_Header)
            {
                std::string Header("DeviceType|DeviceLocation|SusyId|SerNo|TimeStamp|EntryId|EventCode|EventType|Category|Group|Tag|OldValue|NewValue|UserGroup\n");
                std::replace(Header.begin(), Header.end(), '|', cfg->delimiter);
                fputs(Header.c_str(), csv);
            }
        }

        for (uint32_t inv = 0; inverters[inv] != NULL && inv<MAX_INVERTERS; inv++)
        {
            // Sort events on ascending Entry_ID
            std::sort(inverters[inv]->eventData.begin(), inverters[inv]->eventData.end(), SortEntryID_Asc);

            for (const auto &event : inverters[inv]->eventData)
            {

                fprintf(csv, "%s%c", inverters[inv]->DeviceType.c_str(), cfg->delimiter);
                fprintf(csv, "%s%c", inverters[inv]->DeviceName.c_str(), cfg->delimiter);
                fprintf(csv, "%d%c", event.SUSyID(), cfg->delimiter);
                fprintf(csv, "%u%c", event.SerNo(), cfg->delimiter);
                fprintf(csv, "%s%c", strftime_t(cfg->DateTimeFormat, event.DateTime()), cfg->delimiter);
                fprintf(csv, "%d%c", event.EntryID(), cfg->delimiter);
                fprintf(csv, "%d%c", event.EventCode(), cfg->delimiter);
                fprintf(csv, "%s%c", event.EventType().c_str(), cfg->delimiter);
                fprintf(csv, "%s%c", event.EventCategory().c_str(), cfg->delimiter);
                fprintf(csv, "%s%c", tagdefs.getDesc(event.Group()).c_str(), cfg->delimiter);
                fprintf(csv, "%s%c", event.EventDescription().c_str(), cfg->delimiter);

                switch (event.DataType())
                {
                case DT_STATUS:
                    fprintf(csv, "%s%c", tagdefs.getDesc(event.OldVal() & 0xFFFF).c_str(), cfg->delimiter);
                    fprintf(csv, "%s%c", tagdefs.getDesc(event.NewVal() & 0xFFFF).c_str(), cfg->delimiter);
                    break;

                case DT_ULONG:
                    fprintf(csv, "%u%c", event.OldVal(), cfg->delimiter);
                    fprintf(csv, "%u%c", event.NewVal(), cfg->delimiter);
                    break;

                case DT_SLONG:
                    fprintf(csv, "%d%c", event.OldVal(), cfg->delimiter);
                    fprintf(csv, "%d%c", event.NewVal(), cfg->delimiter);
                    break;

                case DT_STRING:
                    fprintf(csv, "%s%c", event.EventStrPara().c_str(), cfg->delimiter);
                    fprintf(csv, "%c", cfg->delimiter);
                    break;

                default:
                    fprintf(csv, "%c%c", cfg->delimiter, cfg->delimiter);
                }

                fprintf(csv, "%s\n", tagdefs.getDesc(event.UserGroupTagID()).c_str());
            }
        }

        fclose(csv);
    }

    return 0;
}

int ExportBatteryDataToCSV(const Config *cfg, InverterData* const inverters[])
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
        if (!cfg->quiet)
        {
            snprintf(msg, sizeof(msg), "Unable to open output file %s\n", csvpath.str().c_str());
            print_error(stdout, PROC_ERROR, msg);
        }
        return -1;
    }
    else
    {
        //Write header when new file has been created
#if defined(_WIN32)
        if (filelength(fileno(csv)) == 0)
#else
        struct stat fStat;
        stat(csvpath.str().c_str(), &fStat);
        if (fStat.st_size == 0)
#endif
        {
            if (cfg->SpotWebboxHeader)
                WriteWebboxHeader(csv, cfg, inverters, 0);
            else
                WriteStandardHeader(csv, cfg, BatteryInverter, 0);
        }

        char FormattedFloat[32];
        const char *strout = "%c%s";

        if (cfg->SpotWebboxHeader)
            fputs(strftime_t(cfg->DateTimeFormat, spottime), csv);

        for (uint32_t inv = 0; inverters[inv] != NULL && inv < MAX_INVERTERS; inv++)
        {
            if (inverters[inv]->DevClass == BatteryInverter)
            {
                if (!cfg->SpotWebboxHeader)
                {
                    fputs(strftime_t(cfg->DateTimeFormat, spottime), csv);
                    fprintf(csv, strout, cfg->delimiter, inverters[inv]->DeviceName.c_str());
                    fprintf(csv, strout, cfg->delimiter, inverters[inv]->DeviceType.c_str());
                    fprintf(csv, "%c%lu", cfg->delimiter, inverters[inv]->Serial);
                }

                fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, ((float)inverters[inv]->Pac1), 0, cfg->precision, cfg->decimalpoint));
                fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, ((float)inverters[inv]->Pac2), 0, cfg->precision, cfg->decimalpoint));
                fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, ((float)inverters[inv]->Pac3), 0, cfg->precision, cfg->decimalpoint));
                fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, ((float)inverters[inv]->Iac1) / 1000, 0, cfg->precision, cfg->decimalpoint));
                fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, ((float)inverters[inv]->Iac2) / 1000, 0, cfg->precision, cfg->decimalpoint));
                fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, ((float)inverters[inv]->Iac3) / 1000, 0, cfg->precision, cfg->decimalpoint));
                fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, ((float)inverters[inv]->Uac1) / 100, 0, cfg->precision, cfg->decimalpoint));
                fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, ((float)inverters[inv]->Uac2) / 100, 0, cfg->precision, cfg->decimalpoint));
                fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, ((float)inverters[inv]->Uac3) / 100, 0, cfg->precision, cfg->decimalpoint));
                fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, ((float)inverters[inv]->TotalPac), 0, cfg->precision, cfg->decimalpoint));
                fprintf(csv, strout, cfg->delimiter, FormatDouble(FormattedFloat, ((double)inverters[inv]->EToday) / 1000, 0, cfg->precision, cfg->decimalpoint));
                fprintf(csv, strout, cfg->delimiter, FormatDouble(FormattedFloat, ((double)inverters[inv]->ETotal) / 1000, 0, cfg->precision, cfg->decimalpoint));
                fprintf(csv, strout, cfg->delimiter, FormatDouble(FormattedFloat, ((double)inverters[inv]->GridFreq) / 100, 0, cfg->precision, cfg->decimalpoint));
                fprintf(csv, strout, cfg->delimiter, FormatDouble(FormattedFloat, ((double)inverters[inv]->OperationTime) / 3600, 0, cfg->precision, cfg->decimalpoint));
                fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, ((float)inverters[inv]->FeedInTime) / 3600, 0, cfg->precision, cfg->decimalpoint));
                fprintf(csv, strout, cfg->delimiter, tagdefs.getDesc(inverters[inv]->DeviceStatus, "?").c_str());
                fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, ((float)inverters[inv]->BatChaStt), 0, cfg->precision, cfg->decimalpoint));
                fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, ((float)inverters[inv]->BatTmpVal) / 10, 0, cfg->precision, cfg->decimalpoint));
                fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, ((float)inverters[inv]->BatVol) / 100, 0, cfg->precision, cfg->decimalpoint));
                fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, ((float)inverters[inv]->BatAmp) / 1000, 0, cfg->precision, cfg->decimalpoint));
                fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, ((float)inverters[inv]->MeteringGridMsTotWOut), 0, cfg->precision, cfg->decimalpoint));
                fprintf(csv, strout, cfg->delimiter, FormatFloat(FormattedFloat, ((float)inverters[inv]->MeteringGridMsTotWIn), 0, cfg->precision, cfg->decimalpoint));
                if (!cfg->SpotWebboxHeader)
                    fputs("\n", csv);
            }
            if (cfg->SpotWebboxHeader)
                fputs("\n", csv);
        }
        fclose(csv);
    }
    return 0;
}

//Undocumented - For 123Solar Web Solar logger usage only)
int ExportSpotDataTo123s(const Config *cfg, InverterData* const inverters[])
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
    float calPdcTot = (float)(invdata->mpp.at(1).Pdc() + invdata->mpp.at(2).Pdc());

    //Calculated AC Side Values (Sum of Power & Current / Maximum of Voltage)
    float calPacTot = (float)(invdata->Pac1 + invdata->Pac2 + invdata->Pac3);
    float calIacTot = (float)(invdata->Iac1 + invdata->Iac2 + invdata->Iac3);
    float calUacMax = (float)std::max(std::max(invdata->Uac1, invdata->Uac2), invdata->Uac3);

    //Calculated Inverter Efficiency
    float calEfficiency = calPdcTot == 0 ? 0 : calPacTot / calPdcTot * 100;

    //Select Between Computer & Inverter Time (As Per .cfg Setting)
    time_t spottime = invdata->InverterDatetime;
    if (cfg->SpotTimeSource) // Take time from computer instead of inverter
        time(&spottime);

    //Send Spot Data Frame to 123Solar

    // $SDTE = Date & Time ( YYYYMMDD-HH:MM:SS )
    printf(strout, strftime_t(s123_dt_format, spottime), *s123_delimiter);
    // $G1V  = GridMs.PhV.phsA
    printf(strout, FormatFloat(FormattedFloat, (float)invdata->Uac1 / 100, 0, cfg->precision, *s123_decimalpoint), *s123_delimiter);
    // $G1A  = GridMs.A.phsA
    printf(strout, FormatFloat(FormattedFloat, (float)invdata->Iac1 / 1000, 0, cfg->precision, *s123_decimalpoint), *s123_delimiter);
    // $G1P  = GridMs.W.phsA
    printf(strout, FormatFloat(FormattedFloat, (float)invdata->Pac1, 0, cfg->precision, *s123_decimalpoint), *s123_delimiter);
    // $G2V  = GridMs.PhV.phsB
    printf(strout, FormatFloat(FormattedFloat, (float)invdata->Uac2 / 100, 0, cfg->precision, *s123_decimalpoint), *s123_delimiter);
    // $G2A  = GridMs.A.phsB
    printf(strout, FormatFloat(FormattedFloat, (float)invdata->Iac2 / 1000, 0, cfg->precision, *s123_decimalpoint), *s123_delimiter);
    // $G2P  = GridMs.W.phsB
    printf(strout, FormatFloat(FormattedFloat, (float)invdata->Pac2, 0, cfg->precision, *s123_decimalpoint), *s123_delimiter);
    // $G3V  = GridMs.PhV.phsC
    printf(strout, FormatFloat(FormattedFloat, (float)invdata->Uac3 / 100, 0, cfg->precision, *s123_decimalpoint), *s123_delimiter);
    // $G3A  = GridMs.A.phsC
    printf(strout, FormatFloat(FormattedFloat, (float)invdata->Iac3 / 1000, 0, cfg->precision, *s123_decimalpoint), *s123_delimiter);
    // $G3P  = GridMs.W.phsC
    printf(strout, FormatFloat(FormattedFloat, (float)invdata->Pac3, 0, cfg->precision, *s123_decimalpoint), *s123_delimiter);
    // $FRQ = GridMs.Hz
    printf(strout, FormatFloat(FormattedFloat, (float)invdata->GridFreq / 100, 0, cfg->precision, *s123_decimalpoint), *s123_delimiter);
    // $EFF = Value computed by SBFspot
    printf(strout, FormatFloat(FormattedFloat, calEfficiency, 0, cfg->precision, *s123_decimalpoint), *s123_delimiter);
    // $INVT = Inverter temperature 
    printf(strout, FormatFloat(FormattedFloat, (float)invdata->Temperature / 100, 0, cfg->precision, *s123_decimalpoint), *s123_delimiter);
    // $BOOT = Booster temperature - n/a for SMA inverters
    printf(strout, FormatFloat(FormattedFloat, 0., 0, cfg->precision, *s123_decimalpoint), *s123_delimiter);
    // $KWHT = Metering.TotWhOut (kWh)
    printf(strout, FormatDouble(FormattedFloat, (double)invdata->ETotal / 1000, 0, cfg->precision, *s123_decimalpoint), *s123_delimiter);
    // $I1V = DcMs.Vol[A]
    printf(strout, FormatFloat(FormattedFloat, (float)invdata->mpp.at(1).Udc() / 100, 0, cfg->precision, *s123_decimalpoint), *s123_delimiter);
    // $I1A = DcMs.Amp[A]
    printf(strout, FormatFloat(FormattedFloat, (float)invdata->mpp.at(1).Idc() / 1000, 0, cfg->precision, *s123_decimalpoint), *s123_delimiter);
    // $I1P = DcMs.Watt[A]
    printf(strout, FormatFloat(FormattedFloat, (float)invdata->mpp.at(1).Pdc(), 0, cfg->precision, *s123_decimalpoint), *s123_delimiter);
    // $I2V = DcMs.Vol[B]
    printf(strout, FormatFloat(FormattedFloat, (float)invdata->mpp.at(2).Udc() / 100, 0, cfg->precision, *s123_decimalpoint), *s123_delimiter);
    // $I2A = DcMs.Amp[B]
    printf(strout, FormatFloat(FormattedFloat, (float)invdata->mpp.at(2).Idc() / 1000, 0, cfg->precision, *s123_decimalpoint), *s123_delimiter);
    // $I2P = DcMs.Watt[B]
    printf(strout, FormatFloat(FormattedFloat, (float)invdata->mpp.at(2).Pdc(), 0, cfg->precision, *s123_decimalpoint), *s123_delimiter);
    // $GV = Was grid voltage in single phase 123Solar - For backwards compatibility
    printf(strout, FormatFloat(FormattedFloat, (float)calUacMax / 100, 0, cfg->precision, *s123_decimalpoint), *s123_delimiter);
    // $GA = Was grid current in single phase 123Solar - For backwards compatibility
    printf(strout, FormatFloat(FormattedFloat, (float)calIacTot / 1000, 0, cfg->precision, *s123_decimalpoint), *s123_delimiter);
    // $GP = Was grid power in single phase 123Solar - For backwards compatibility
    printf(strout, FormatFloat(FormattedFloat, (float)calPacTot, 0, cfg->precision, *s123_decimalpoint), *s123_delimiter);
    // OK
    printf("%s\n", ">>>S123:OK");
    return 0;
}

//Undocumented - For 123Solar Web Solar logger usage only)
int ExportInformationDataTo123s(const Config *cfg, InverterData* const inverters[])
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
    printf("Device Name: %s%c", invdata->DeviceName.c_str(), *s123_delimiter);
    // Inverter device class
    printf("Device Class: %s%c", invdata->DeviceClass.c_str(), *s123_delimiter);
    // Inverter device type
    printf("Device Type: %s%c", invdata->DeviceType.c_str(), *s123_delimiter);
    // Inverter serial number
    printf("Serial Number: %lu%c", invdata->Serial, *s123_delimiter);
    // Inverter firmware version
    printf("Firmware: %s%c", invdata->SWVersion.c_str(), *s123_delimiter);
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
int ExportStateDataTo123s(const Config *cfg, InverterData* const inverters[])
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
    printf("Device Name: %s%c", invdata->DeviceName.c_str(), *s123_delimiter);
    // Device status
    printf("Device Status: %s%c", tagdefs.getDesc(invdata->DeviceStatus, "?").c_str(), *s123_delimiter);
    // Grid relay status
    printf("GridRelay Status: %s%c", tagdefs.getDesc(invdata->GridRelayStatus, "?").c_str(), *s123_delimiter);
    // Operation time
    printf("Operation Time: %s%c", FormatDouble(FormattedFloat, (double)invdata->OperationTime / 3600, 0, cfg->precision, *s123_decimalpoint), *s123_delimiter);
    // Feed-in time
    printf("Feed-In Time: %s%c", FormatDouble(FormattedFloat, (double)invdata->FeedInTime / 3600, 0, cfg->precision, *s123_decimalpoint), *s123_delimiter);
    // Bluetooth signal
    printf("Bluetooth Signal: %s\n", FormatDouble(FormattedFloat, invdata->BT_Signal, 0, cfg->precision, *s123_decimalpoint));
    return 0;
}
