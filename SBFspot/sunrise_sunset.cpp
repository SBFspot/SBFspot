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

#include "sunrise_sunset.h"
#include "misc.h"	//get_tzOffset()

/*
 * This new algorithm should fix a few issues with sunrise/sunset in different timezones - Thanks to Ron Patton
 * Issue 20: Version 2.0.1 has lost a grip of time
 * Issue 50: SBFspot stopped functioning completely. Must use -finq option to produce output.
 * Issue 51: Timezone awareness broken again
 */

// C program calculating the sunrise and sunset for the current date and a fixed location(latitude,longitude)
// Note, twilight calculation gives insufficient accuracy of results
// Jarmo Lammi 1999 - 2001
// Last update July 21st, 2001

const double degs = 180.0/pi; //rtd()
const double rads = pi/180.0; //dtr()

double L; // mean longitude of the Sun
double g; // mean anomaly of the Sun

// Get the days to J2000
// h is UT in decimal hours
// FNday only works between 1901 to 2099 - see Meeus chapter 7
double FNday (int y, int m, int d, double h)
{
	long int luku = - 7 * (y + (m + 9)/12)/4 + 275*m/9 + d;
	// type casting necessary on PC DOS and TClite to avoid overflow
	luku += (long int)y * 367;
	return (double)luku - 730531.5 + h/24.0;
};

// the function below returns an angle in the range 0 to 2*pi
double FNrange (double x)
{
    double b = 0.5 * x / pi;
    double a = 2.0 * pi * (b - (long)(b));
    if (a < 0) a = 2.0 * pi + a;
    return a;
};

// Calculating the hourangle
double f0(double lat, double declin)
{
	const double SunDia = 0.53;			// Sunradius degrees
	const double AirRefr = 34.0/60.0;	// athmospheric refraction degrees
	// Correction: different sign at S HS
	double dfo = rads*(0.5*SunDia + AirRefr);
	if(lat < 0.0) dfo = -dfo;
	double fo = tan(declin + dfo) * tan(lat*rads);
	if (fo>0.99999) fo=1.0; // to avoid overflow
	fo = asin(fo) + pi/2.0;
	return fo;
};

// Calculating the hourangle for twilight times
double f1(double lat, double declin)
{
	// Correction: different sign at S HS
	double df1 = rads * 6.0;
	if (lat < 0.0) df1 = -df1;
	double fi = tan(declin + df1) * tan(lat*rads);
	if (fi>0.99999) fi=1.0; // to avoid overflow
	fi = asin(fi) + pi/2.0;
	return fi;
};

// Find the ecliptic longitude of the Sun
double FNsun (double d)
{
	// mean longitude of the Sun
	L = FNrange(280.461 * rads + .9856474 * rads * d);
	// mean anomaly of the Sun
	g = FNrange(357.528 * rads + .9856003 * rads * d);
	// Ecliptic longitude of the Sun
	return FNrange(L + 1.915 * rads * sin(g) + .02 * rads * sin(2 * g));
};

int sunrise_sunset(const float latit, const float longit, float *sunrise, float *sunset, const float offset)
{
	// get the date and time from the user
	// read system date and extract the year

	/** First get time **/
	time_t sekunnit;
	time(&sekunnit);

	/** Next get localtime **/
	struct tm p;
	memcpy(&p, localtime(&sekunnit), sizeof(p));

	int y = p.tm_year + 1900;
	int m = p.tm_mon + 1;
	int day = p.tm_mday;

	const double h = 12;

	// Get TZ in hours
	double tzone = (double)get_tzOffset(NULL) / 3600; //Fix for half-hour timezones

	double d = FNday(y, m, day, h);

	// Use FNsun to find the ecliptic longitude of the Sun
	double lambda = FNsun(d);

	// Obliquity of the ecliptic
	double obliq = 23.439 * rads - .0000004 * rads * d;

	//   Find the RA and DEC of the Sun
	double alpha = atan2(cos(obliq) * sin(lambda), cos(lambda));
	double delta = asin(sin(obliq) * sin(lambda));

	// Find the Equation of Time in minutes
	// Correction suggested by David Smith
	double LL = L - alpha;
	if (L < pi) LL += 2.0*pi;
	double equation = 1440.0 * (1.0 - LL / pi / 2.0);
	double ha = f0(latit, delta);
	//double hb = f1(latit, delta);
	//double twx = 12.0 * (hb - ha) / pi;	// length of twilight in hours
	//double twam = riset - twx;			// morning twilight begin
	//double twpm = settm + twx;			// evening twilight end


	// Conversion of angle to hours and minutes
	double daylen = degs * ha / 7.5;
	if (daylen < 0.0001) daylen = 0.0;
	// arctic winter
	double riset = 12.0 - 12.0 * ha/pi + tzone - longit/15.0 + equation/60.0;
	double settm = 12.0 + 12.0 * ha/pi + tzone - longit/15.0 + equation/60.0;
	//double noontime = riset + 12.0 * ha/pi;
	double altmax = 90.0 + delta * degs - latit;
	// Correction for S HS suggested by David Smith
	// to express altitude as degrees from the N horizon
	if (latit < delta * degs) altmax = 180.0 - altmax;

	if (riset > 24.0) riset -= 24.0;
	if (settm > 24.0) settm -= 24.0;

	*sunrise = (float)riset;
	*sunset = (float)settm;

	// Convert HH:MM to float
	float now = p.tm_hour + (float)p.tm_min / 60;
	if ((now >= (*sunrise - offset)) && (now <= (*sunset + offset)))
		return 1;	// Sun's up
	else
		return 0;	// Sun's down
}

