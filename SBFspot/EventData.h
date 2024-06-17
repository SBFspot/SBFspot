/************************************************************************************************
    SBFspot - Yet another tool to read power production of SMA solar inverters
    (c)2012-2024, SBF

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
#include "endianness.h"
#include "TagDefs.h"
#include <string>
#include <memory>

extern bool quiet;
extern int verbose;

// EventCodes
enum SMA_EVENTNUMBER : uint16_t
{
    EvtSetSclParaOk     = 10100,
    EvtSetSclParaNok    = 10101,
    EvtSetSttParaOk     = 10102,
    EvtSetSttParaNok    = 10103,
    EvtSetStrParaOk     = 10104,
    EvtSetStrParaNok    = 10105,
    EvtUpdOk            = 10106,
    EvtUpdNok           = 10107,
    EvtOldTm            = 10108,
    EvtNewTm            = 10109,
    EvtComSttChg        = 10251,
    EvtConnFail         = 10252,
    EvtConnSpdChg       = 10253,
    EvtDpxModChg        = 10254,
    EvtNetwLodOk        = 10255,
    EvtNetwId1          = 10256,
};

// Align structs on byte boundaries
#pragma pack(push, 1)
union SMA_EVENTARGS
{
    char str[16];
    struct U32
    {
        uint32_t Para1;
        uint32_t Para2;
        uint32_t Para3;
        uint32_t Para4;
    } U32;
};

struct SMA_EVENTDATA
{
    int32_t  DateTime;
    uint16_t EntryID;
    uint16_t SUSyID;
    uint32_t SerNo;
    uint16_t EventCode;
    uint16_t EventFlags;
    uint32_t Group;
    uint32_t ulong1;
    uint32_t Tag;
    uint32_t Counter;
    SMA_EVENTARGS Args;
};
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
    uint32_t m_UserGroup;
    SMA_EVENTARGS m_EventArgs;

public:
    EventData(const uint32_t UserGroup, const SMA_EVENTDATA *ev) :
        // Use btohs / btohl byte swapping macros for big endian systems (MIPS,...)
        m_DateTime(btohl(ev->DateTime)),
        m_EntryID(btohs(ev->EntryID)),
        m_SUSyID(btohs(ev->SUSyID)),
        m_SerNo(btohl(ev->SerNo)),
        m_EventCode(btohs(ev->EventCode)),
        m_EventFlags(btohs(ev->EventFlags)),
        m_Group(btohl(ev->Group)),
        m_Tag(btohl(ev->Tag)),
        m_Counter(btohl(ev->Counter)),
        m_UserGroup(UserGroup)
    {
        memcpy(&m_EventArgs, &ev->Args, sizeof(SMA_EVENTARGS));
    }
    time_t   DateTime() const { return m_DateTime; }
    uint16_t EntryID() const { return m_EntryID; }
    uint16_t SUSyID() const { return m_SUSyID; }
    uint32_t SerNo() const { return m_SerNo; }
    uint16_t EventCode() const { return m_EventCode; }
    uint16_t EventFlags() const { return m_EventFlags; }
    uint32_t Group() const { return GroupTagID(); }
    uint32_t Tag() const { return m_Tag; }
    uint32_t Counter() const { return m_Counter; }
    uint32_t DT_Change() const { return btohl(m_EventArgs.U32.Para1); }
    uint32_t Parameter() const { return btohl(m_EventArgs.U32.Para2); }
    uint32_t NewVal() const { return btohl(m_EventArgs.U32.Para3); }
    uint32_t OldVal() const { return btohl(m_EventArgs.U32.Para4); }
    uint32_t UserGroup() const { return m_UserGroup; }
    unsigned int UserGroupTagID() const;
    std::string EventType() const;
    std::string EventCategory() const;
    unsigned int DataType() const { return Parameter() >> 24; }
    std::string EventDescription() const;
    std::string quote(const std::string& str) const { return '"' + str + '"'; }
    std::string ToString() const;
    std::string ToString(const char *datetimeformat) const;
    std::string S0() const;
    std::string X(size_t idx) const;
    std::string EventStrPara() const;

private:
    unsigned int GroupTagID() const;
    bool isverbose(int level) { return !quiet && (verbose >= level); }
    std::string ToLocalTime(const time_t rawtime, const char *format) const;
};
