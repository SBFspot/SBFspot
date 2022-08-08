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

#include "../SBFspot/osselect.h"
#include "../SBFspot/SBFspot.h"
#include <curl/curl.h>
#include <string>
#include <iostream>
#include <sstream>
#include <vector>
#include <map>

extern bool quiet;
extern int verbose;

class PVOutput
{
public:
    enum
    {
        HTTP_OK = 200,
        HTTP_FORBIDDEN = 403,
        HTTP_NOT_FOUND = 404,
        // More codes at http://en.wikipedia.org/wiki/List_of_HTTP_status_codes
    };

private:
    std::string m_SystemName;
    unsigned int m_SystemSize;
    std::string m_Postcode;
    unsigned int m_NmbrPanels;
    unsigned int m_PanelPower;
    std::string m_PanelBrand;
    unsigned int m_NmbrInverters;
    unsigned int m_InverterPower;
    std::string m_InverterBrand;
    std::string m_Orientation;
    float m_ArrayTilt;
    std::string m_Shade;
    std::string m_InstallDate; //time_t ?
    std::pair<double, double> m_location; //Latitude/Longitude;
    unsigned int m_StatusInterval;
    unsigned int m_NmbrPanels2;
    unsigned int m_PanelPower2;
    std::string m_Orientation2;
    float m_ArrayTilt2;
    float m_ExportTariff;
    float m_ImportPeakTariff;
    float m_ImportOffPeakTariff;
    float m_ImportShoulderTariff;
    float m_ImportHighShoulderTariff;
    float m_ImportDailyCharge;
    std::vector<unsigned int> m_Teams;
    unsigned int m_Donations;
    std::map<int, std::pair<std::string, std::string> > m_ExtData;
    CURLcode m_curlres;
    CURL* m_curl;
    unsigned int m_timeout;
    unsigned int m_SID;
    std::string m_APIkey;
    std::string m_buffer;
    curl_slist *m_http_header;
    long m_http_status;

public:
    PVOutput(unsigned int SID, std::string APIkey, unsigned int timeout);
    ~PVOutput();
    CURLcode downloadURL(std::string URL);
    CURLcode downloadURL(std::string URL, std::string data);
    CURLcode getSystemData(void);
    //Removed in version 3.0
    //bool Export(Config *cfg, InverterData *inverters[]);
    void clearBuffer() { m_buffer.clear(); }
    std::string SystemName() const { return m_SystemName; }
    long HTTP_status() const { return m_http_status; }
    /*
    Add other members if needed...
    */
    CURLcode errcode() const { return m_curlres; }
    std::string errtext() const { return std::string(curl_easy_strerror(m_curlres)); }

    bool isTeamMember() const;
    bool isSupporter() const { return (m_Donations > 0); }
    int batch_statuslimit() const { return m_Donations == 0 ? 30 : 100; }
    int batch_datelimit() const { return m_Donations == 0 ? 14 : 90; }
    int batch_ratelimit() const { return m_Donations == 0 ? 60 : 100; }
    CURLcode addBatchStatus(std::string data, std::string &response);

private:
    static void writeCallback(char *ptr, size_t size, size_t nmemb, void *stream);
    size_t writeCallback_impl(char *ptr, size_t size, size_t nmemb);
    bool isverbose(int level) { return !quiet && (verbose >= level); }
};

