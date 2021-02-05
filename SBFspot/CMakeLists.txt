project(SBFspot)

cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig)
find_package(Boost REQUIRED COMPONENTS date_time)

pkg_check_modules(Bluetooth IMPORTED_TARGET bluez)
pkg_check_modules(MessagePack IMPORTED_TARGET msgpack)
pkg_check_modules(Mosquitto IMPORTED_TARGET libmosquittopp)

set(CMAKE_CXX_FLAGS "-Wno-deprecated-declarations")

set(DB "SQLITE" CACHE STRING "DB backend to use: NOSQL, SQLITE, MYSQL, MARIADB")

set(COMMON_SOURCES
    ArchData.cpp
    Config.cpp
    CSVexport.cpp
    Defines.cpp
    Ethernet.cpp
    EventData.cpp
    Export.cpp
    Import.cpp
    Inverter.cpp
    SBFNet.cpp
    SBFspot.cpp
    TagDefs.cpp
    Timer.cpp
    boost_ext.cpp
    main.cpp
    misc.cpp
    mqtt.cpp
    sunrise_sunset.cpp
)

FILE(GLOB SQLITE_RESOURCES *SQLite.sql)
FILE(GLOB MYSQL_RESOURCES *MySQL.sql)
FILE(GLOB COMMON_RESOURCES TagList*.txt)
set(COMMON_RESOURCES
    ${COMMON_RESOURCES}
    date_time_zonespec.csv
)

if (Bluetooth_FOUND)
list(APPEND COMMON_SOURCES
    Bluetooth.cpp
)
endif()

if(DB STREQUAL "MYSQL")
    list(APPEND COMMON_SOURCES
        db_MySQL.cpp
        db_MySQL_Export.cpp
    )
    list(APPEND COMMON_RESOURCES
        ${MYSQL_RESOURCES}
    )
    SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -DUSE_MYSQL")
    SET(COMMON_LIBRARIES "mysqlclient")
elseif(DB STREQUAL "SQLITE")
    list(APPEND COMMON_SOURCES
        db_SQLite.cpp
        db_SQLite_Export.cpp
    )
    list(APPEND COMMON_RESOURCES
        ${SQLITE_RESOURCES}
    )
    SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -DUSE_SQLITE")
    SET(COMMON_LIBRARIES "sqlite3")
elseif(DB STREQUAL "MARIADB")
    list(APPEND COMMON_SOURCES
        db_MySQL.cpp
        db_MySQL_Export.cpp
    )
    list(APPEND COMMON_RESOURCES
        ${MYSQL_RESOURCES}
    )
    SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -DUSE_MYSQL")
    SET(COMMON_LIBRARIES "mariadbclient")
endif()

add_executable(${PROJECT_NAME}
    ${COMMON_SOURCES}
)

# Optional libs
if (Bluetooth_FOUND)
    add_compile_definitions(BLUETOOTH_FOUND)
    target_link_libraries(${PROJECT_NAME} PkgConfig::Bluetooth)
endif()
if (MessagePack_FOUND)
    add_compile_definitions(MSGPACK_FOUND MSGPACK_USE_BOOST)
    target_link_libraries(${PROJECT_NAME} PkgConfig::MessagePack)
endif()
if (Mosquitto_FOUND)
    add_compile_definitions(MOSQUITTO_FOUND)
    target_link_libraries(${PROJECT_NAME} PkgConfig::Mosquitto)
endif()

add_subdirectory(tests)

target_link_libraries(${PROJECT_NAME}
    Boost::date_time
    ${COMMON_LIBRARIES}
)

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin/sbfspot.3
)

install(FILES
    ${COMMON_RESOURCES}
    DESTINATION bin/sbfspot.3
)

install(FILES
    SBFspot.cfg
    DESTINATION bin/sbfspot.3
    RENAME SBFspot.default.cfg
)
