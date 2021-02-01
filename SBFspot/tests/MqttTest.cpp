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

#include "../Config.h"
#include "../Defines.h"
#include "../Inverter.h"
#include "../mqtt.h"
#include "../Timer.h"

#include <thread>

int main(int argc, char **argv)
{
    debug = 5;
    verbose = 5;

    Config config;
    config.loop = true;
    config.liveInterval = 1;
    config.mqtt_host = "broker.hivemq.com";
    config.plantname = "testplant";
    config.mqtt_topic = "sbfspot_{plantname}/sma_{serial}";
    config.mqtt_item_format = "MSGPACK";
    Timer timer(config);

    InverterData inverterData;
    inverterData.Serial = 1234;
    inverterData.Pmax1 = 10000;

    do
    {
        auto timePoint = timer.nextTimePoint();
        inverterData.ETotal = std::chrono::system_clock::to_time_t(timePoint)/10;
        inverterData.EToday = std::chrono::system_clock::to_time_t(timePoint)%100000;
        inverterData.Pdc1 = std::chrono::system_clock::to_time_t(timePoint)%11000;
        inverterData.Pdc2 = std::chrono::system_clock::to_time_t(timePoint)%7000;
        inverterData.TotalPac = inverterData.Pdc1 + inverterData.Pdc2;
        std::this_thread::sleep_until(timePoint);

        MqttExport mqtt(config);
        mqtt.exportConfig({inverterData});
        mqtt.exportInverterData({inverterData});
    }
    while(true);

    return 0;
}
