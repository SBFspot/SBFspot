/************************************************************************************************
    SBFspot - Yet another tool to read power production of SMA solar inverters
    (c)2012-2021, SBF

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

#include "osselect.h" // #define _USE_32BIT_TIME_T on windows
#include "mqtt.h"
#include "SBFspot.h"
#include <boost/algorithm/string.hpp>
#include "mppt.h"

MqttExport::MqttExport(const Config& config)
    : m_config(config)
{
}

MqttExport::~MqttExport()
{
}

int MqttExport::exportInverterData(const std::vector<InverterData>& inverterData)
{
    int rc = 0;

    // Split message body
    std::vector<std::string> items;
    boost::split(items, m_config.mqtt_publish_data, boost::is_any_of(","));

    std::stringstream mqtt_message;
    std::string key;
    char value[80];
    int prec = m_config.precision;
    char dp = '.';

    for (const auto& inv : inverterData)
    {
#if defined(_WIN32)
        std::string mqtt_command_line = "\"\"" + m_config.mqtt_publish_exe + "\" " + m_config.mqtt_publish_args + "\"";
#else
        std::string mqtt_command_line = m_config.mqtt_publish_exe + " " + m_config.mqtt_publish_args;
        // On Linux, message must be inside single quotes
        boost::replace_all(mqtt_command_line, "\"", "'");
#endif

        // Fill host/port/topic
        boost::replace_first(mqtt_command_line, "{host}", m_config.mqtt_host);
        boost::replace_first(mqtt_command_line, "{port}", m_config.mqtt_port);
        boost::replace_first(mqtt_command_line, "{topic}", m_config.mqtt_topic);

        mqtt_message.str("");

        for (const auto& item : items)
            {
            bool add_to_msg = true;
            time_t timestamp = time(NULL);
            key = item;
            memset(value, 0, sizeof(value));
            std::transform((key).begin(), (key).end(), (key).begin(), ::tolower);
            if (key == "timestamp")
                snprintf(value, sizeof(value) - 1, "\"%s\"", strftime_t(m_config.DateTimeFormat, timestamp));
            else if (key == "sunrise") {
                std::tm *sunrise = std::localtime(&timestamp);
                sunrise->tm_sec = 0;
                sunrise->tm_hour = (int)m_config.sunrise;
                sunrise->tm_min = (int)((m_config.sunrise - (int)m_config.sunrise) * 60);
                snprintf(value, sizeof(value) - 1, "\"%s\"", strftime_t(m_config.DateTimeFormat, std::mktime(sunrise)));
            }
            else if (key == "sunset") {
                std::tm *sunset = std::localtime(&timestamp);
                sunset->tm_sec = 0;
                sunset->tm_hour = (int)m_config.sunset;
                sunset->tm_min = (int)((m_config.sunset - (int)m_config.sunset) * 60);
                snprintf(value, sizeof(value) - 1, "\"%s\"", strftime_t(m_config.DateTimeFormat, std::mktime(sunset)));
            }
            else if (key == "invserial")        snprintf(value, sizeof(value) - 1, "%lu", inv.Serial);
            else if (key == "invname")          snprintf(value, sizeof(value) - 1, "\"%s\"", inv.DeviceName);
            else if (key == "invclass")         snprintf(value, sizeof(value) - 1, "\"%s\"", inv.DeviceClass.c_str());
            else if (key == "invtype")          snprintf(value, sizeof(value) - 1, "\"%s\"", inv.DeviceType.c_str());
            else if (key == "invswver")         snprintf(value, sizeof(value) - 1, "\"%s\"", inv.SWVersion.c_str());
            else if (key == "invtime")          snprintf(value, sizeof(value) - 1, "\"%s\"", strftime_t(m_config.DateTimeFormat, inv.InverterDatetime));
            else if (key == "invstatus")        snprintf(value, sizeof(value) - 1, "\"%s\"", tagdefs.getDesc(inv.DeviceStatus, "?").c_str());
            else if (key == "invtemperature")   FormatFloat(value, is_NaN(inv.Temperature) ? 0.0f : (float)inv.Temperature / 100, 0, prec, dp);
            else if (key == "invgridrelay")     snprintf(value, sizeof(value) - 1, "\"%s\"", tagdefs.getDesc(inv.GridRelayStatus, "?").c_str());
            else if (key == "pdc1")             FormatFloat(value, (float)inv.Pdc1, 0, prec, dp);
            else if (key == "pdc2")             FormatFloat(value, (float)inv.Pdc2, 0, prec, dp);
            else if (key == "pdctot")           FormatFloat(value, (float)inv.calPdcTot, 0, prec, dp);
            else if (key == "idc1")             FormatFloat(value, (float)inv.Idc1 / 1000, 0, prec, dp);
            else if (key == "idc2")             FormatFloat(value, (float)inv.Idc2 / 1000, 0, prec, dp);
            else if (key == "udc1")             FormatFloat(value, (float)inv.Udc1 / 100, 0, prec, dp);
            else if (key == "udc2")             FormatFloat(value, (float)inv.Udc2 / 100, 0, prec, dp);
            else if (key == "etotal")           FormatDouble(value, (double)inv.ETotal / 1000, 0, prec, dp);
            else if (key == "etoday")           FormatDouble(value, (double)inv.EToday / 1000, 0, prec, dp);
            else if (key == "pactot")           FormatFloat(value, (float)inv.TotalPac, 0, prec, dp);
            else if (key == "pac1")             FormatFloat(value, (float)inv.Pac1, 0, prec, dp);
            else if (key == "pac2")             FormatFloat(value, (float)inv.Pac2, 0, prec, dp);
            else if (key == "pac3")             FormatFloat(value, (float)inv.Pac3, 0, prec, dp);
            else if (key == "uac1")             FormatFloat(value, (float)inv.Uac1 / 100, 0, prec, dp);
            else if (key == "uac2")             FormatFloat(value, (float)inv.Uac2 / 100, 0, prec, dp);
            else if (key == "uac3")             FormatFloat(value, (float)inv.Uac3 / 100, 0, prec, dp);
            else if (key == "iac1")             FormatFloat(value, (float)inv.Iac1 / 1000, 0, prec, dp);
            else if (key == "iac2")             FormatFloat(value, (float)inv.Iac2 / 1000, 0, prec, dp);
            else if (key == "iac3")             FormatFloat(value, (float)inv.Iac3 / 1000, 0, prec, dp);
            else if (key == "gridfreq")         FormatFloat(value, (float)inv.GridFreq / 100, 0, prec, dp);
            else if (key == "opertm")           FormatDouble(value, (double)inv.OperationTime / 3600, 0, prec, dp);
            else if (key == "feedtm")           FormatDouble(value, (double)inv.FeedInTime / 3600, 0, prec, dp);
            else if (key == "btsignal")         FormatFloat(value, inv.BT_Signal, 0, prec, dp);
            else if (key == "battmpval")        FormatFloat(value, ((float)inv.BatTmpVal) / 10, 0, prec, dp);
            else if (key == "batvol")           FormatFloat(value, ((float)inv.BatVol) / 100, 0, prec, dp);
            else if (key == "batamp")           FormatFloat(value, ((float)inv.BatAmp) / 1000, 0, prec, dp);
            else if (key == "batchastt")        FormatFloat(value, ((float)inv.BatChaStt), 0, prec, dp);
            else if (key == "invwakeuptm")      snprintf(value, sizeof(value) - 1, "\"%s\"", strftime_t(m_config.DateTimeFormat, inv.WakeupTime));
            else if (key == "invsleeptm")       snprintf(value, sizeof(value) - 1, "\"%s\"", strftime_t(m_config.DateTimeFormat, inv.SleepTime));
            else if (key == "pdc")
                for (const auto& dc : inv.mpp)
                {
                    FormatFloat(value, (float)dc.second.Pdc(), 0, prec, dp);
                    mqtt_message << to_keyvalue(std::string("PDC") + std::to_string(dc.first), value);
                    add_to_msg = false;
                }
            else if (key == "idc")
                for (const auto& dc : inv.mpp)
                {
                    FormatFloat(value, (float)dc.second.Idc() / 1000, 0, prec, dp);
                    mqtt_message << to_keyvalue(std::string("IDC") + std::to_string(dc.first), value);
                    add_to_msg = false;
                }
            else if (key == "udc")
                for (const auto& dc : inv.mpp)
                {
                    FormatFloat(value, (float)dc.second.Udc() / 100, 0, prec, dp);
                    mqtt_message << to_keyvalue(std::string("UDC") + std::to_string(dc.first), value);
                    add_to_msg = false;
                }
            else // None of the above, so it's an unhandled item or a typo...
            {
                add_to_msg = false;
                if (VERBOSE_NORMAL) std::cout << "MQTT: Don't know what to do with '" << item << "'" << std::endl;
            }

            if (add_to_msg)
                mqtt_message << to_keyvalue(item, value);
        }

        boost::replace_first(mqtt_command_line, "{plantname}", m_config.plantname);
        boost::replace_first(mqtt_command_line, "{serial}", std::to_string(inv.Serial));
        boost::replace_first(mqtt_command_line, "{message}", mqtt_message.str().substr(1));

        if (VERBOSE_NORMAL) std::cout << "MQTT: Publishing (" << m_config.mqtt_topic << ") " << mqtt_message.str().substr(1) << std::endl;

        int system_rc = ::system(mqtt_command_line.c_str());

        if (system_rc != 0) // Error
        {
            std::cout << "MQTT: Failed te execute '" << m_config.mqtt_publish_exe << "' mosquitto client installed?" << std::endl;
            rc = system_rc;
        }
    }

    return rc;
}

std::string MqttExport::to_keyvalue(const std::string key, const std::string value) const
{
    std::string key_value = m_config.mqtt_item_delimiter + m_config.mqtt_item_format;
    boost::replace_all(key_value, "{key}", key);
    boost::replace_first(key_value, "{value}", value);

    boost::replace_all(key_value, "\"\"", "\"");
#if defined(_WIN32)
    boost::replace_all(key_value, "\"", "\"\"");
#endif

    return key_value;
}
