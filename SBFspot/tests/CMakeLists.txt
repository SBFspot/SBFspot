#add_executable(pluginmanagertest
#    PluginManagerTest.cpp
#    ../PluginManager.cpp
#    ../ble/BleExport.cpp
#)

#target_include_directories(pluginmanagertest
#PRIVATE
#    ../
#)

#target_link_libraries(pluginmanagertest
#    bluez-dbus-cpp
#)

if (Bluetooth_FOUND)
    add_executable(bluetoothtest
        BluetoothTest.cpp
        ../Bluetooth.cpp
        ../Defines.cpp
        ../Ethernet.cpp
        ../Import.cpp
        ../misc.cpp
        ../sunrise_sunset.cpp
        ../SBFNet.cpp
        ../SBFspot.cpp
        ../Timer.cpp
    )
    target_link_libraries(bluetoothtest
        PkgConfig::Bluetooth
    )
endif()

if (Mosquitto_FOUND AND MessagePack_FOUND)
    add_executable(mqtttest
        MqttTest.cpp
        ../Defines.cpp
        ../Export.cpp
        ../misc.cpp
        ../mqtt.cpp
        ../MqttMsgPackExport.cpp
        ../sunrise_sunset.cpp
        ../Timer.cpp
    )
    target_link_libraries(mqtttest
        Boost::date_time
        PkgConfig::Mosquitto
        PkgConfig::MessagePack
    )
endif()
