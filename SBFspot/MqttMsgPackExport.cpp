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

#include "MqttMsgPackExport.h"

#include "Config.h"
#include "SBFspot.h"

#include <chrono>
#include <thread>
#include <msgpack.hpp>
using namespace std::chrono_literals;

MqttMsgPackExport::MqttMsgPackExport(const Config& config)
    : m_config(config)
{
    mosqpp::lib_init();
    if (VERBOSE_HIGH)
        std::cout << "MQTT: Connecting broker: " << m_config.mqtt_host << std::endl;
    if (connect(m_config.mqtt_host.c_str()) != 0)
    {
        std::cout << "MQTT: Failed to connect broker: " << m_config.mqtt_host << std::endl;
        return;
    }

    // Do NOT start loop before being connected.
    loop_start();
}

MqttMsgPackExport::~MqttMsgPackExport()
{
    // Let's sleep a little bit. mosquitto expects to run in an event loop.
    std::this_thread::sleep_for(100ms);
    disconnect();
    loop_stop();
    mosqpp::lib_cleanup();
}

std::string MqttMsgPackExport::name() const
{
    return "MqttMsgPackExport";
}

int MqttMsgPackExport::exportConfig(const std::vector<InverterData>& inverterData)
{
    // Collect PV array config per serial
    std::multimap<uint32_t,ArrayConfig> arrayConfig;
    for (const auto& ac : m_config.arrays)
        arrayConfig.insert({ac.inverterSerial, ac});

    for (const auto& inv : inverterData)
    {
        std::string topic = m_config.mqtt_topic;
        boost::replace_first(topic, "{plantname}", m_config.plantname);
        boost::replace_first(topic, "{serial}", std::to_string(inv.Serial));
        topic += "/config";

        // Pack manually (because a float in map gets stored as double and timestamp is not supported yet).
        msgpack::sbuffer sbuf;
        msgpack::packer<msgpack::sbuffer> packer(sbuf);
        // Map with number of elements
        packer.pack_map(5);
        // 1. Protocol version
        packer.pack_uint8(static_cast<uint8_t>(InverterProperty::Version));
        packer.pack_uint8(0);
        // 2. Latitude
        packer.pack_uint8(static_cast<uint8_t>(InverterProperty::Latitude));
        packer.pack_float(m_config.latitude);
        // 3. Longitude
        packer.pack_uint8(static_cast<uint8_t>(InverterProperty::Longitude));
        packer.pack_float(m_config.longitude);
        // 4. Power Max
        packer.pack_uint8(static_cast<uint8_t>(InverterProperty::PowerMax));
        packer.pack_float(static_cast<float>(inv.Pmax1));
        // 5. Array config
        packer.pack_uint8(static_cast<uint8_t>(InverterProperty::PvArray));
        packer.pack_array(arrayConfig.count(inv.Serial));   // Store an array to provide data for each PV array.

        auto itb = arrayConfig.lower_bound(inv.Serial);
        auto ite = arrayConfig.upper_bound(inv.Serial);
        for (auto it = itb; it != ite; ++it)
        {
            packer.pack_map(4);
            packer.pack_uint8(static_cast<uint8_t>(InverterProperty::PvArrayName));
            packer.pack((*it).second.name);
            packer.pack_uint8(static_cast<uint8_t>(InverterProperty::PvArrayAzimuth));
            packer.pack_float(static_cast<float>((*it).second.azimuth));
            packer.pack_uint8(static_cast<uint8_t>(InverterProperty::PvArrayElevation));
            packer.pack_float(static_cast<float>((*it).second.elevation));
            packer.pack_uint8(static_cast<uint8_t>(InverterProperty::PvArrayPowerMax));
            packer.pack_float(static_cast<float>((*it).second.powerPeak));
        }

        if (VERBOSE_HIGH) std::cout << "MQTT: Publishing topic: " << topic
                                    << ", data size: " << sbuf.size() << std::endl;
        if (publish(nullptr, topic.c_str(), sbuf.size(), sbuf.data(), 1, true) != 0)
        {
            std::cout << "MQTT: Failed to publish topic" << std::endl;
            continue;
        }
    }

    return 0;
}

int MqttMsgPackExport::exportInverterData(const std::vector<InverterData>& inverterData)
{
    for (const auto& inv : inverterData)
    {
        std::string topic = m_config.mqtt_topic;
        boost::replace_first(topic, "{plantname}", m_config.plantname);
        boost::replace_first(topic, "{serial}", std::to_string(inv.Serial));
        topic += "/live";

        // Pack manually (because a float in map gets stored as double and timestamp is not supported yet).
        msgpack::sbuffer sbuf;
        msgpack::packer<msgpack::sbuffer> packer(sbuf);
        // Map with number of elements
        packer.pack_map(6);
        // 1. Protocol version
        packer.pack_uint8(static_cast<uint8_t>(InverterProperty::Version));
        packer.pack_uint8(0);
        // 2. Timestamp
        packer.pack_uint8(static_cast<uint8_t>(InverterProperty::Timestamp));
        auto t = htonl(time(nullptr));
        packer.pack_ext(4, -1); // Timestamp type
        packer.pack_ext_body((const char*)(&t), 4);
        // 3. Yield Total
        packer.pack_uint8(static_cast<uint8_t>(InverterProperty::YieldTotal));
        packer.pack_float(static_cast<float>(inv.ETotal));
        // 4. Yield Today
        packer.pack_uint8(static_cast<uint8_t>(InverterProperty::YieldToday));
        packer.pack_float(static_cast<float>(inv.EToday));
        // 5. Power AC
        packer.pack_uint8(static_cast<uint8_t>(InverterProperty::Power));
        packer.pack_float(static_cast<float>(inv.TotalPac));
        // 6. Power DC
        packer.pack_uint8(static_cast<uint8_t>(InverterProperty::PvArray));
        packer.pack_array(2);   // Store an array to provide data for each Mpp.
        // 6.1 MPP1
        packer.pack_map(1);
        packer.pack_uint8(static_cast<uint8_t>(InverterProperty::Power));
        packer.pack_float(static_cast<float>(inv.Pdc1));
        // 6.2 MPP2
        packer.pack_map(1);
        packer.pack_uint8(static_cast<uint8_t>(InverterProperty::Power));
        packer.pack_float(static_cast<float>(inv.Pdc2));

        if (VERBOSE_HIGH) std::cout << "MQTT: Publishing topic: " << topic
                                    << ", data size: " << sbuf.size() << std::endl;
        if (publish(nullptr, topic.c_str(), sbuf.size(), sbuf.data(), 0, true) != 0)
        {
            std::cout << "MQTT: Failed to publish topic" << std::endl;
            continue;
        }
    }

    return 0;
}
