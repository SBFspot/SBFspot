/************************************************************************************************
	SBFspot - Yet another tool to read power production of SMA® solar inverters
	(c)2012-2015, SBF

	Latest version found at https://sbfspot.codeplex.com

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

class Rec40Att
{
private:
	int32_t m_LRI;
	int32_t m_DateTime;
	int32_t m_Attrib;

public:
	Rec40Att() { memset(this, 0, sizeof(Rec40Att)); }

	void LRI(int32_t lri) { m_LRI = lri; }
	void DateTime(int32_t datetime) { m_DateTime = datetime; }
	void Attrib(int32_t attrib) { m_Attrib = attrib; }

	int32_t LRI(void) const { return m_LRI & 0x00FFFF00; }
	time_t DateTime(void) const { return (time_t)m_DateTime; }
	int32_t Attrib(void) const { return m_Attrib; }
};

