#define VERSION "3.6.0 beta"

// V3.5.1
// Fixed #230: Unknown Inverter Type SBS3.7-10
// Fixed #261: Unknown Inverter Type STP4.0-3AV-40
// Fixed #269: SMA STP6.0-3AV-40 no readout with SBFspot?
// Fixed #279: Unknown inverter type SMA STP6.0-3AV-40
// Fixed #281: SBFspotUploadDaemon not uploading
// Fixed #287: Unknown Inverter Type: STP5.0-3AV-40

// V3.5.2
// Fixed #297: 0x000024BA and Inverter Type=SB3.6-1AV-41

// V3.5.3
// Fixed #302: Unknown Inverter Type - SMA Sunny Tripower 3.0
// Fixed #309: Unknown Inverter Type - SMA Sunny Tripower 8.0
// Fix warnings [-Wformat-truncation=] Raspbian Buster (gcc 8.3)

// V3.5.4
// Fixed #???: Error! Could not open file C\?\C:\PathToConfig\SBFspot.cfg (Windows only - reported by mail)
// Fixed #327: Unknown inverter: Sunny Boy Storage 5.0 (SBS5.0-10) & Sunny Tripower 10.0 (STP10.0-3AV-40)
// Fixed #328: Unknown inverter type Sunny Boy Storage 6.0

// V3.6.0
// MQTT support

// V3.6.1 beta
// Added support for SB 240 & Multigate-10
// Fix SB240 not detected when multigate/SB240 comined with other inverters
// Consolidate day/monthdata of micro-inverters into multigate
// Fix Logoff Multigate
// By merging 3.2.1 and 3.6

/***
*
* Complete modification history can be found at:
* https://github.com/SBFspot/SBFspot/wiki/Modification-History
*
***/