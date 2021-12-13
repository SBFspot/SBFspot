#define VERSION "3.9.0"

/*
V3.8.2
    Fix #455 Voltages and Currents in .csv file swapped for battery inverter
    Fix #428 SBFspotUploadDaemon crash
    Fix #459 Incorrect Calculated EToday
    Fix #330 From Sqlite3 to MySQL
    Fix #457 Serial number too big for defined field

V3.8.3
    Fix #476 V3.8.2 fails to compile on Fedora 34
    Merged #467 Let executable return errors in Inverter::process()
    Fix #482 Some data missing on 2nd Inverter

V3.8.4
    Fix 384/137/381/313/... Bad request 400: Power value too high for system size
    Improved event decoding
    Cleanup verbose/debug messages

V3.9.0
    Fix #3 Three string SMA inverters
    Fix 401 Multiple strings support
    Fix 421 SMA core2 has 12 strings
*/

/***
*
* Complete modification history can be found at:
* https://github.com/SBFspot/SBFspot/wiki/Modification-History
*
***/