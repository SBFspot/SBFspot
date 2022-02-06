/************************************************************************************************
    SBFspot - Yet another tool to read power production of SMA® solar inverters
    (c)2012-2022, SBF

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

#include "osselect.h"
#include "nan.h"

class mppt
{
private:
    int32_t m_Pdc = NaN_S32;
    int32_t m_Udc = NaN_S32;
    int32_t m_Idc = NaN_S32;

public:
    mppt() { }
    mppt(const int32_t Pdc, const int32_t Udc, const int32_t Idc)
    {
        m_Pdc = Pdc;
        m_Udc = Udc;
        m_Idc = Idc;
    }
    ~mppt() { }

    int32_t Pdc() const { return m_Pdc; }
    int32_t Udc() const { return m_Udc; }
    int32_t Idc() const { return m_Idc; }
    float kW() const { return (float)(m_Pdc) / 1000; }
    float Watt() const { return (float)(m_Pdc); }
    float Volt() const { return (float)(m_Udc) / 100; }
    float Amp() const { return (float)(m_Idc) / 1000; }

    void Pdc(const int32_t Pdc) { m_Pdc = Pdc; }
    void Udc(const int32_t Udc) { m_Udc = Udc; }
    void Idc(const int32_t Idc) { m_Idc = Idc; }

};

