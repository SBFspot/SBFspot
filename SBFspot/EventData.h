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
#include "endianness.h"

#include <string>

extern int quiet;
extern int verbose;

//SMA Structs must be aligned on byte boundaries
#pragma pack(push, 1)
typedef struct
{
    int32_t DateTime;   // fix issue CP30
	uint16_t EntryID;
	uint16_t SUSyID;
	uint32_t SerNo;
	uint16_t EventCode;
	uint16_t EventFlags;
	uint32_t Group;
	uint32_t ulong1;
	uint32_t Tag;
	uint32_t Counter;
	uint32_t DT_Change;
	uint32_t Parameter;
	uint32_t NewVal;
	uint32_t OldVal;
} SMA_EVENTDATA;
#pragma pack(pop)

class EventData
{
private:
	time_t   m_DateTime;
	uint16_t m_EntryID;
	uint16_t m_SUSyID;
	uint32_t m_SerNo;
	uint16_t m_EventCode;
	uint16_t m_EventFlags;
	uint32_t m_Group;
	uint32_t m_Tag;
	uint32_t m_Counter;
	uint32_t m_DT_Change;
	uint32_t m_Parameter;
	uint32_t m_NewVal;
	uint32_t m_OldVal;
	uint32_t m_UserGroup;

public:
	EventData(const uint32_t UserGroup, const SMA_EVENTDATA *ev):
		// Use btohs and btohl byte swapping macros for big endian systems (MIPS,...)
		// Fix Issue 98
        m_DateTime(btohl(ev->DateTime)),
        m_EntryID(btohs(ev->EntryID)),
        m_SUSyID(btohs(ev->SUSyID)),
        m_SerNo(btohl(ev->SerNo)),
        m_EventCode(btohs(ev->EventCode)),
        m_EventFlags(btohs(ev->EventFlags)),
        m_Group(btohl(ev->Group)),
        m_Tag(btohl(ev->Tag)),
        m_Counter(btohl(ev->Counter)),
        m_DT_Change(btohl(ev->DT_Change)),
        m_Parameter(btohl(ev->Parameter)),
        m_NewVal(btohl(ev->NewVal)),
        m_OldVal(btohl(ev->OldVal)),
        m_UserGroup(UserGroup) {}
	time_t   DateTime() const { return m_DateTime; }
	uint16_t EntryID() const { return m_EntryID; }
	uint16_t SUSyID() const { return m_SUSyID; }
	uint32_t SerNo() const { return m_SerNo; }
	uint16_t EventCode() const { return m_EventCode; }
	uint16_t EventFlags() const { return m_EventFlags; }
	uint32_t Group() const { return GroupTagID(); }
	uint32_t Tag() const { return m_Tag; }
	uint32_t Counter() const { return m_Counter; }
	uint32_t DT_Change() const { return m_DT_Change; }
	uint32_t Parameter() const { return m_Parameter; }
	uint32_t NewVal() const { return m_NewVal; }
	uint32_t OldVal() const { return m_OldVal; }
	uint32_t UserGroup() const { return m_UserGroup; }
	unsigned int UserGroupTagID() const;
	std::string EventType() const;
	std::string EventCategory() const;
	unsigned int DataType() const { return m_Parameter >> 24; }
	friend bool SortEntryID_Asc(const EventData& ed1, const EventData& ed2) { return ed1.m_EntryID < ed2.m_EntryID; }
	friend bool SortEntryID_Desc(const EventData& ed1, const EventData& ed2) { return ed1.m_EntryID > ed2.m_EntryID; }

private:
	unsigned int GroupTagID() const;
	bool isverbose(int level)
	{
		return !quiet && (verbose >= level);
	}

};

bool SortEntryID_Asc(const EventData& ed1, const EventData& ed2);
bool SortEntryID_Desc(const EventData& ed1, const EventData& ed2);
