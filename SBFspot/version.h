#define VERSION "3.9.4"

/*
V3.9.1
    Fix #508: V390: no contents written into the csv file: 'spot'
    Removed PAC_MAX
    C++11 range-based for-loops
    Removed -ble option
    Show MeteringGridInfo only when available

V3.9.2
    Fix #511 Segmentation fault on V3.9.1

V3.9.3
    Return real error code when logon failed (deprecated E_LOGONFAILED)
    Fix #519 SMA2500TLST values wrong in CSV
    Fix #520 Output format modified - earlier format had standard format with 2 MPPT's
    Fix #522 Redundant loop

V3.9.4
    Fail on incompatible firmware version (see https://forum.iobroker.net/topic/52761/sbfspot-funktioniert-nicht-communication-error)

*/

/***
*
* Complete changelog can be found at:
* https://github.com/SBFspot/SBFspot/wiki/Modification-History
*
***/