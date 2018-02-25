/************************************************************************************************
	SBFspot - Yet another tool to read power production of SMA® solar inverters
	(c)2012-2018, SBF

	Latest version found at https://github.com/SBFspot/SBFspot

	License: Attribution-NonCommercial-ShareAlike 3.0 Unported (CC BY-NC-SA 3.0)
	http://creativecommons.org/licenses/by-nc-sa/3.0/

	You are free:
		to Share — to copy, distribute and transmit the work
		to Remix — to adapt the work
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

class Rec40S32
{
private:
	int32_t m_LRI;
	int32_t m_DateTime;
	int32_t m_MinLL;	// Lower Level
	int32_t m_MaxLL;
	int32_t m_MinUL;	// Upper Level
	int32_t m_MaxUL;
	int32_t m_MinActual;// Current
	int32_t m_MaxActual;
	int32_t m_Res1;		// Reserved
	int32_t m_Res2;		// Reserved

public:
	Rec40S32() { memset(this, 0, sizeof(Rec40S32)); }

	void LRI(int32_t lri) { m_LRI = lri; }
	void DateTime(int32_t datetime) { m_DateTime = datetime; }
	void MinLL(int32_t minll) { m_MinLL = minll; }
	void MaxLL(int32_t maxll) { m_MaxLL = maxll; }
	void MinUL(int32_t minul) { m_MinUL = minul; }
	void MaxUL(int32_t maxul) { m_MaxUL = maxul; }
	void MinActual(int32_t minactual) { m_MinActual = minactual; }
	void MaxActual(int32_t maxactual) { m_MaxActual = maxactual; }
	void Res1(int32_t res1) { m_Res1 = res1; }
	void Res2(int32_t res2) { m_Res2 = res2; }
	
	int32_t LRI(void) const { return m_LRI & 0x00FFFF00; }
	int32_t MinPowerLimit(void) const { return m_MinLL; }
	int32_t MaxPowerLimit(void) const { return m_MinUL; }
	int32_t ActualPowerLimit(void) const { return m_MinActual; }

	int32_t MinLL(void) const { return m_MinLL; }
	int32_t MaxLL(void) const { return m_MaxLL; }
	int32_t MinUL(void) const { return m_MinUL; }
	int32_t MaxUL(void) const { return m_MaxUL; }
	int32_t MinActual(void) const { return m_MinActual; }
	int32_t MaxActual(void) const { return m_MaxActual; }
	int32_t Res1(void) const { return m_Res1; }
	int32_t Res2(void) const { return m_Res2; }

	float ActualPowerLimitPct(void) const { return (float)m_MinActual / (float)m_MinUL * 100.0f; }
};

