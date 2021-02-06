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

#include "TagDefs.h"

#include "Defines.h"
#include "osselect.h"
#include <fstream>
#include <iostream>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

using namespace boost::algorithm;

bool TagDefs::isverbose(int level)
{
    return !quiet && (verbose >= level);
}
void TagDefs::print_error(std::string msg)
{
    std::cerr << "Error: " << msg << std::endl;
}
void TagDefs::print_error(std::string msg, unsigned int line, std::string fpath)
{
    std::cerr << "Error: " << msg << " on line " << line << " [" << fpath << "]\n";
}
void TagDefs::addTag(unsigned int tagID, std::string tag, unsigned int lri, std::string desc)
{
    m_tagdefmap.insert(std::make_pair(tagID, TD(tag, lri, desc)));
}

int TagDefs::readall(std::string path, std::string locale)
{
    boost::algorithm::to_upper(locale); //fix case sensitivity issues on linux systems

	//Build fullpath to taglist<locale>.txt
	//Default to EN-US if localized file not found
	std::string fn_taglist = path + "TagList" + locale + ".txt";

	std::ifstream fs;
	fs.open(fn_taglist.c_str(), std::ifstream::in);
	if (!fs.is_open())
    {
		print_error("Could not open file " + fn_taglist);
        if (stricmp(locale.c_str(), "EN-US") != 0)
		{
			if (isverbose(0)) std::cout << "Using default locale en-US\n";
			fn_taglist = path + "TagListEN-US.txt";

			fs.open(fn_taglist.c_str(), std::ifstream::in);
			if (!fs.is_open())
			{
				print_error("Could not open file " + fn_taglist);
				return READ_ERROR;
			}
		}
		else return READ_ERROR;
    }

	std::string line;
	unsigned int lineCnt = 0;

	while(getline(fs, line))
	{
		lineCnt++;

		//Get rid of comments and empty lines
		size_t hashpos = line.find_first_of("#\r");
		if (hashpos != std::string::npos) line.erase(hashpos);

		if (line.size() > 0)
		{
			std::string tag_id;
			std::vector<std::string> tag_info;
			
			// Split line at first '='
			std::string::size_type pos = line.find('=');
			if (pos != std::string::npos)
			{
				tag_id = line.substr(0, pos);
				std::string tag = line.substr(pos + 1);
				boost::split(tag_info, tag, [](char c) { return c == '\\'; }); // boost::is_any_of("=") ==> [](char c) { return c == '='; }
			}

			// Split line Tag\Lri\Descr
			if (tag_info.size() != 3)
				print_error("Wrong number of items", lineCnt, fn_taglist);
			else
			{
				bool entryOK = true;
				unsigned int tagID = 0;
				try
				{
					tagID = boost::lexical_cast<unsigned int>(tag_id);
				}
				catch(...)
				{
					print_error("Invalid tagID", lineCnt, fn_taglist);
					entryOK = false;
				}

				unsigned int lri = 0;
				try
				{
					lri = boost::lexical_cast<unsigned int>(tag_info[1]);
				}
				catch(...)
				{
					print_error("Invalid LRI", lineCnt, fn_taglist);
					entryOK = false;
				}

				if (entryOK)
				{
					std::string tag = tag_info[0];
					trim(tag);

					std::string descr = tag_info[2];
					trim(descr);

					addTag(tagID, tag, lri, descr);
				}
			}
		}
	}

	fs.close();

	return READ_OK;
}

unsigned int TagDefs::getTagIDForLRI(unsigned int LRI)
{
	LRI &= 0x00FFFF00;
	for (std::map<unsigned long, TD>::iterator it=m_tagdefmap.begin(); it!=m_tagdefmap.end(); ++it)
	{
		if (LRI == it->second.getLRI())
			return it->first;
	}

	return 0;
}

std::string TagDefs::getTagForLRI(unsigned int LRI)
{
	LRI &= 0x00FFFF00;
	for (std::map<unsigned long, TD>::iterator it=m_tagdefmap.begin(); it!=m_tagdefmap.end(); ++it)
	{
		if (LRI == it->second.getLRI())
			return it->second.getTag();
	}

	return "";
}

std::string TagDefs::getDescForLRI(unsigned int LRI)
{
	LRI &= 0x00FFFF00;
	for (std::map<unsigned long, TD>::iterator it=m_tagdefmap.begin(); it!=m_tagdefmap.end(); ++it)
	{
		if (LRI == it->second.getLRI())
			return it->second.getDesc();
	}

	return "";
}
