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

#include "mqtt.h"

#include <string>
#include <vector>
#include <boost/algorithm/string.hpp>
#include "CSVexport.h" // FormatFloat(), FormatDouble()

int mqtt_publish(const Config *cfg, InverterData *inverters[])
{
	int rc = 0;

	// Split message body
	std::vector<std::string> items;
	boost::split(items, cfg->mqtt_publish_data, boost::is_any_of(","));

	std::stringstream mqtt_message;
	std::string key;
	char value[80];
	int prec = cfg->precision;
	char dp = '.';

	for (int inv = 0; inverters[inv] != NULL && inv < MAX_INVERTERS; inv++)
	{
#if defined(WIN32)
		std::string mqtt_command_line = "\"\"" + cfg->mqtt_publish_exe + "\" " + cfg->mqtt_publish_args + "\"";
#else
		std::string mqtt_command_line = cfg->mqtt_publish_exe + " " + cfg->mqtt_publish_args;
		// On Linux, message must be inside single quotes
		boost::replace_all(mqtt_command_line, "\"", "'");
#endif

		// Fill host/port/topic
		boost::replace_first(mqtt_command_line, "{host}", cfg->mqtt_host);
		boost::replace_first(mqtt_command_line, "{port}", cfg->mqtt_port);
		boost::replace_first(mqtt_command_line, "{topic}", cfg->mqtt_topic);

		mqtt_message.str("");

		for (std::vector<std::string>::iterator it = items.begin(); it != items.end(); ++it)
		{
			time_t timestamp = time(NULL);
			key = *it;
			memset(value, 0, sizeof(value));
			std::transform((key).begin(), (key).end(), (key).begin(), ::tolower);
			if (key == "timestamp")				snprintf(value, sizeof(value) - 1, "\"%s\"", strftime_t(cfg->DateTimeFormat, timestamp));
			else if (key == "sunrise")			snprintf(value, sizeof(value) - 1, "\"%s %02d:%02d:00\"", strftime_t(cfg->DateFormat, timestamp), (int)cfg->sunrise, (int)((cfg->sunrise - (int)cfg->sunrise) * 60));
			else if (key == "sunset")			snprintf(value, sizeof(value) - 1, "\"%s %02d:%02d:00\"", strftime_t(cfg->DateFormat, timestamp), (int)cfg->sunset, (int)((cfg->sunset - (int)cfg->sunset) * 60));
			else if (key == "invserial")		snprintf(value, sizeof(value) - 1, "%lu", inverters[inv]->Serial);
			else if (key == "invname")			snprintf(value, sizeof(value) - 1, "\"%s\"", inverters[inv]->DeviceName);
			else if (key == "invclass")			snprintf(value, sizeof(value) - 1, "\"%s\"", inverters[inv]->DeviceClass);
			else if (key == "invtype")			snprintf(value, sizeof(value) - 1, "\"%s\"", inverters[inv]->DeviceType);
			else if (key == "invswver")			snprintf(value, sizeof(value) - 1, "\"%s\"", inverters[inv]->SWVersion);
			else if (key == "invtime")			snprintf(value, sizeof(value) - 1, "\"%s\"", strftime_t(cfg->DateTimeFormat, inverters[inv]->InverterDatetime));
			else if (key == "invstatus")		snprintf(value, sizeof(value) - 1, "\"%s\"", tagdefs.getDesc(inverters[inv]->DeviceStatus, "?").c_str());
			else if (key == "invtemperature")	FormatFloat(value, (float)inverters[inv]->Temperature / 100, 0, prec, dp);
			else if (key == "invgridrelay")		snprintf(value, sizeof(value) - 1, "\"%s\"", tagdefs.getDesc(inverters[inv]->GridRelayStatus, "?").c_str());
			else if (key == "pdc1")				FormatFloat(value, (float)inverters[inv]->Pdc1, 0, prec, dp);
			else if (key == "pdc2")				FormatFloat(value, (float)inverters[inv]->Pdc2, 0, prec, dp);
			else if (key == "idc1")				FormatFloat(value, (float)inverters[inv]->Idc1 / 1000, 0, prec, dp);
			else if (key == "idc2")				FormatFloat(value, (float)inverters[inv]->Idc2 / 1000, 0, prec, dp);
			else if (key == "udc1")				FormatFloat(value, (float)inverters[inv]->Udc1 / 100, 0, prec, dp);
			else if (key == "udc2")				FormatFloat(value, (float)inverters[inv]->Udc2 / 100, 0, prec, dp);
			else if (key == "etotal")			FormatDouble(value, (double)inverters[inv]->ETotal / 1000, 0, prec, dp);
			else if (key == "etoday")			FormatDouble(value, (double)inverters[inv]->EToday / 1000, 0, prec, dp);
			else if (key == "pactot")			FormatFloat(value, (float)inverters[inv]->TotalPac, 0, prec, dp);
			else if (key == "pac1")				FormatFloat(value, (float)inverters[inv]->Pac1, 0, prec, dp);
			else if (key == "pac2")				FormatFloat(value, (float)inverters[inv]->Pac1, 0, prec, dp);
			else if (key == "pac3")				FormatFloat(value, (float)inverters[inv]->Pac1, 0, prec, dp);
			else if (key == "uac1")				FormatFloat(value, (float)inverters[inv]->Uac1 / 100, 0, prec, dp);
			else if (key == "uac2")				FormatFloat(value, (float)inverters[inv]->Uac1 / 100, 0, prec, dp);
			else if (key == "uac3")				FormatFloat(value, (float)inverters[inv]->Uac1 / 100, 0, prec, dp);
			else if (key == "iac1")				FormatFloat(value, (float)inverters[inv]->Iac1 / 1000, 0, prec, dp);
			else if (key == "iac2")				FormatFloat(value, (float)inverters[inv]->Iac1 / 1000, 0, prec, dp);
			else if (key == "iac3")				FormatFloat(value, (float)inverters[inv]->Iac1 / 1000, 0, prec, dp);
			else if (key == "gridfreq")			FormatFloat(value, (float)inverters[inv]->GridFreq / 100, 0, prec, dp);
			else if (key == "opertm")			FormatDouble(value, (double)inverters[inv]->OperationTime / 3600, 0, prec, dp);
			else if (key == "feedtm")			FormatDouble(value, (double)inverters[inv]->FeedInTime / 3600, 0, prec, dp);
			else if (key == "battmpval")		FormatFloat(value, ((float)inverters[inv]->BatTmpVal) / 10, 0, prec, dp);
			else if (key == "batvol")			FormatFloat(value, ((float)inverters[inv]->BatVol) / 100, 0, prec, dp);
			else if (key == "batamp")			FormatFloat(value, ((float)inverters[inv]->BatAmp) / 1000, 0, prec, dp);
			else if (key == "batchastt")		FormatFloat(value, ((float)inverters[inv]->BatChaStt), 0, prec, dp);

			// None of the above, so it's an unhandled item or a typo...
			else if (VERBOSE_NORMAL) std::cout << "MQTT: Don't know what to do with '" << *it << "'" << std::endl;

			std::string key_value = cfg->mqtt_item_format;
			boost::replace_all(key_value, "{key}", (*it));
			boost::replace_first(key_value, "{value}", value);

			boost::replace_all(key_value, "\"\"", "\"");
#if defined(WIN32)
			boost::replace_all(key_value, "\"", "\"\"");
#endif

			// Append delimiter, except for first item
			if (mqtt_message.str() != "")
			{
				mqtt_message << cfg->mqtt_item_delimiter;
			}

			mqtt_message << key_value;
		}

		if (VERBOSE_NORMAL) std::cout << "MQTT: Publishing (" << cfg->mqtt_topic << ") " << mqtt_message.str() << std::endl;

		std::stringstream serial;
		serial.str("");
		serial << inverters[inv]->Serial;
		boost::replace_first(mqtt_command_line, "{serial}", serial.str());
		boost::replace_first(mqtt_command_line, "{message}", mqtt_message.str());

		int system_rc = ::system(mqtt_command_line.c_str());

		if (system_rc != 0) // Error
		{
			std::cout << "MQTT: Failed te execute '" << cfg->mqtt_publish_exe << "' mosquitto clients installed?" << std::endl;
			rc = system_rc;
		}
	}

	return rc;
}