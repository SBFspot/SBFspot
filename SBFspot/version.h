#define VERSION "3.7.0"

// V3.7.0
// PR367: Added support for SB 240 & Multigate-10 by merging 3.2.1 into 3.6.0 (Thanks to Rob Meerwijk)
//		Fix: SB240 not detected when multigate/SB240 comined with other inverters
//		Add: Consolidate day/monthdata of micro-inverters into multigate
//		Fix: Logoff Multigate
// PR371: Some bugfixes with a SMA Home Manager 2.0 in the network
// PR374: Fixed bug where efficiency was always 0 in ExportSpotDataToCSV
// PR361: Openwrt
// Fix #337: Unknown Inverter Type SB7.7-1SP-US-41
// Fix #352: Unknown Inverter Type SBS6.0-US-10
// Added Edimax Smart Plugs and some newer inverters
// Renamed Optical Global Peak to SMA ShadeFix
// Fix #350 Limitation for MQTT keywords?
// Fix #391 Unknown Inverter: SMA Sunny Island 4.4M-13
// Fix #390 sbfspot and mysql/mariadb
// Fix #334 MQTT JSON Publish - different attribute notation on multiple instances

/***
*
* Complete modification history can be found at:
* https://github.com/SBFspot/SBFspot/wiki/Modification-History
*
***/