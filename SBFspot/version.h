#define VERSION "3.9.9"

/*
V3.9.9
    Fix #532 SBFspotUpload service stops when remote SQL server is temporarily down
    Added SqlQueryInterval to Upload service/daemon config option
    Fix Y2038 issue (See #688 Not able to set inverter time)
    Cleanup of #include <time.h>
    Fix RootDeviceAddress empty when MIS_Enabled=0
    Fix #692 Compile failure on Debian 12 - 64 bit
    Fix #442 Failed to get current plant time

V3.9.8
    MQTT stream to Custom CSV
    Fail on incompatible firmware version when MIS_ENABLED=0 (See #37 #535 #678)
    Fix #578 Corrupted/Future data from Inverter
    Fix #635 The mystery of the sole 2.5 min interval datapoint... and it's corrupt

V3.9.7
    Fix #567 Segmentation fault when connection address is missing
    Merge #599 Added https support for PVOutput uploads
    Fix #630 Battery related values all zero (Hybrid Inverter)
    Add -version command line argument

V3.9.6
    Fix #559 SBFspotUploadDaemon log to stdout?
    Fix #549 After upgrade to v395, the CSV file is no longer written

V3.9.5
    Improved event decoding
    Fix Events not written to database introduced in V3.9.0 (MySQL/MariaDB only)
    Add SBFspotVersion and Plantname to mqtt stream
    Fix #527 Data too long for column 'OldValue'
    Various refactorings
    Fix #539 Garbage in battery CSV file

V3.9.4
    Fail on incompatible firmware version (see https://forum.iobroker.net/topic/52761/sbfspot-funktioniert-nicht-communication-error)
    Improved SPW device discovery
    Handle returncode 21 (E_LRINOTAVAIL)
    Handle returncode -1 (Implement retry comm)

V3.9.3
    Return real error code when logon failed (deprecated E_LOGONFAILED)
    Fix #519 SMA2500TLST values wrong in CSV
    Fix #520 Output format modified - earlier format had standard format with 2 MPPT's
    Fix #522 Redundant loop

V3.9.2
    Fix #511 Segmentation fault on V3.9.1

V3.9.1
    Fix #508: V390: no contents written into the csv file: 'spot'
    Removed PAC_MAX
    C++11 range-based for-loops
    Removed -ble option
    Show MeteringGridInfo only when available
*/

/***
*
* Complete changelog can be found at:
* https://github.com/SBFspot/SBFspot/wiki/Modification-History
*
***/
