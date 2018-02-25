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

#include "TagDefs.h"
#include "misc.h"
#include "SBFspot.h"
#include <map>
#include <string>
#include <fstream>
#include <vector>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

using namespace std;
using namespace boost;
using namespace boost::algorithm;

int TagDefs::readall(string path, string locale)
{
	to_upper(locale); //fix case sensitivity issues on linux systems

	//Build fullpath to taglist<locale>.txt
	//Default to EN-US if localized file not found
	string fn_taglist = path + "TagList" + locale + ".txt";

	ifstream fs;
	fs.open(fn_taglist.c_str(), std::ifstream::in);
	if (!fs.is_open())
    {
		print_error("Could not open file " + fn_taglist);
		if (stricmp(locale.c_str(), "EN-US") != 0)
		{
			if (isverbose(0)) cout << "Using default locale en-US\n";
			fn_taglist = path + "TagListEN-US.txt";

			fs.open(fn_taglist.c_str(), ifstream::in);
			if (!fs.is_open())
			{
				print_error("Could not open file " + fn_taglist);
				return READ_ERROR;
			}
		}
		else return READ_ERROR;
    }

	string line;
	unsigned int lineCnt = 0;

	while(getline(fs, line))
	{
		lineCnt++;

		//Get rid of comments and empty lines
		size_t hashpos = line.find_first_of("#\r");
		if (hashpos != string::npos) line.erase(hashpos);

		if (line.size() > 0)
		{
			//Split line TagID=Tag\Lri\Descr
			vector<string> lineparts;
			boost::split(lineparts, line, boost::is_any_of("=\\"));
			if (lineparts.size() != 4)
				print_error("Wrong number of items", lineCnt, fn_taglist);
			else
			{
				int entryOK = 1;
				unsigned int tagID = 0;
				try
				{
					tagID = boost::lexical_cast<unsigned int>(lineparts[0]);
				}
				catch(...)
				{
					print_error("Invalid tagID", lineCnt, fn_taglist);
					entryOK = 0;
				}

				unsigned int lri = 0;
				try
				{
					lri = boost::lexical_cast<unsigned int>(lineparts[2]);
				}
				catch(...)
				{
					print_error("Invalid LRI", lineCnt, fn_taglist);
					entryOK = 0;
				}

				if (entryOK == 1)
				{
					string tag = lineparts[1];
					trim(tag);

					string descr = lineparts[3];
					trim(descr);

					add(tagID, tag, lri, descr);
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

string TagDefs::getTagForLRI(unsigned int LRI)
{
	LRI &= 0x00FFFF00;
	for (std::map<unsigned long, TD>::iterator it=m_tagdefmap.begin(); it!=m_tagdefmap.end(); ++it)
	{
		if (LRI == it->second.getLRI())
			return it->second.getTag();
	}

	return "";
}

string TagDefs::getDescForLRI(unsigned int LRI)
{
	LRI &= 0x00FFFF00;
	for (std::map<unsigned long, TD>::iterator it=m_tagdefmap.begin(); it!=m_tagdefmap.end(); ++it)
	{
		if (LRI == it->second.getLRI())
			return it->second.getDesc();
	}

	return "";
}
