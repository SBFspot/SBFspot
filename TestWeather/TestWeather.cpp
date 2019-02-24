// TestWeather.cpp : Defines the entry point for the console application.
//

#include "../SBFspotWeatherCommon/CommonServiceCode.h"

int verbose = 0;
int quiet = 0;
bool bStopping = false;
Configuration cfg;

int main(int argc, char* argv[])
{

	if (cfg.readSettings(argv[0], "") == Configuration::CFG_OK)
	{
		CommonServiceCode();
	}

	return 0;
}

