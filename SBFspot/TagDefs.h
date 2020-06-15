/************************************************************************************************
	SBFspot - Yet another tool to read power production of SMA® solar inverters
	(c)2012-2020, SBF

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

#include <map>
#include <string>
#include <iostream>

extern int quiet;
extern int verbose;

class TagDefs
{
public:
	enum {READ_OK = 0, READ_ERROR = -1};

private:
	class TD
	{
	private:
		std::string m_tag;		// Label
		unsigned int m_lri;	// Logical Record Index
		std::string m_desc;		// Description

	public:
		TD() { }
		TD(std::string tag, unsigned int lri, std::string desc) : m_tag(tag), m_lri(lri), m_desc(desc) {}
		std::string getTag() const { return m_tag; }
		unsigned int getLRI() const { return m_lri; }
		std::string getDesc() const { return m_desc; }
	};

private:
	std::map<unsigned long, TD> m_tagdefmap;

private:
	bool isverbose(int level)
	{
		return !quiet && (verbose >= level);
	}
	void print_error(std::string msg)
	{
		std::cerr << "Error: " << msg << std::endl;
	}
	void print_error(std::string msg, unsigned int line, std::string fpath)
	{
		std::cerr << "Error: " << msg << " on line " << line << " [" << fpath << "]\n";
	}
	void addTag(unsigned int tagID, std::string tag, unsigned int lri, std::string desc)
	{
		m_tagdefmap.insert(std::make_pair(tagID, TD(tag, lri, desc)));
	}

public:
	int readall(std::string path, std::string locale);
	std::string getTag(unsigned int tagID) { return m_tagdefmap[tagID].getTag(); }
	unsigned int getTagIDForLRI(unsigned int LRI);
	std::string getTagForLRI(unsigned int LRI);
	std::string getDescForLRI(unsigned int LRI);
	unsigned int getLRI(unsigned int tagID) { return m_tagdefmap[tagID].getLRI(); }
	std::string getDesc(unsigned int tagID) { return m_tagdefmap[tagID].getDesc(); }
	std::string getDesc(unsigned int tagID, std::string _default) { return m_tagdefmap[tagID].getDesc().empty() ? _default : m_tagdefmap[tagID].getDesc(); }
	std::map<unsigned int, TD>::size_type size(void) const { return m_tagdefmap.size(); }
};
