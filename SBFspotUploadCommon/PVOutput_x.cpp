/************************************************************************************************
    SBFspot - Yet another tool to read power production of SMA solar inverters
    (c)2012-2018, SBF

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


#include "PVOutput.h"

// Static Callback member function
// Disable Compiler Optimization for this function (Issue 97)

// This function has been moved to its own cpp file to disable optimization, leaving the rest
// of the code optimized. Previous attempts to disable optimization with __attribute__((optimize("Os")))
// failed (Linux)
// Compile without -O switch

#if defined(_WIN32)
#pragma optimize("", off)
#endif

void PVOutput::writeCallback(char *ptr, size_t size, size_t nmemb, void *f)
{
    // Call non-static member function
    static_cast<PVOutput *>(f)->writeCallback_impl(ptr, size, nmemb);
}

#if defined(_WIN32)
#pragma optimize("", on)
#endif

