/************************************************************************************************
    SBFspot - Yet another tool to read power production of SMA? solar inverters
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

#pragma once

#define NaN_S16 0x8000                          // "Not a Number" representation for int16_t
#define NaN_U16 0xFFFF                          // "Not a Number" representation for uint16_t
#define NaN_S32 (int32_t) 0x80000000            // "Not a Number" representation for int32_t
#define NaN_U32 (uint32_t)0xFFFFFFFF            // "Not a Number" representation for uint32_t
#define NaN_S64 (int64_t) 0x8000000000000000    // "Not a Number" representation for int64_t
#define NaN_U64 (uint64_t)0xFFFFFFFFFFFFFFFF    // "Not a Number" representation for uint64_t

inline const bool is_NaN(const int16_t S16)
{
    return S16 == NaN_S16;
}

inline const bool is_NaN(const uint16_t U16)
{
    return U16 == NaN_U16;
}

inline const bool is_NaN(const int32_t S32)
{
    return S32 == NaN_S32;
}

inline const bool is_NaN(const uint32_t U32)
{
    return U32 == NaN_U32;
}

inline const bool is_NaN(const int64_t S64)
{
    return S64 == NaN_S64;
}

inline const bool is_NaN(const uint64_t U64)
{
    return U64 == NaN_U64;
}
