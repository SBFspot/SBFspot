/************************************************************************************************
    SBFspot - Yet another tool to read power production of SMA solar inverters
    (c)2012-2021, SBF

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

#include "Timer.h"

#include "Config.h"
#include "misc.h"
#include "sunrise_sunset.h"

extern int verbose;

Timer::Timer(Config& config)
    : m_config(config)
{
}

bool Timer::isBright() const
{
    // If co-ordinates provided, calculate sunrise & sunset times for this location.
    if ((m_config.latitude != 0) || (m_config.longitude != 0))
    {
        bool isLight = sunrise_sunset(m_config.latitude, m_config.longitude, &m_config.sunrise, &m_config.sunset, (float)m_config.SunRSOffset / 3600);

        if (VERBOSE_NORMAL)
        {
            printf("sunrise: %02d:%02d\n", (int)m_config.sunrise, (int)((m_config.sunrise - (int)m_config.sunrise) * 60));
            printf("sunset : %02d:%02d\n", (int)m_config.sunset, (int)((m_config.sunset - (int)m_config.sunset) * 60));
        }

        return isLight;
    }

    return true;
}

std::chrono::system_clock::time_point Timer::nextTimePoint() const
{
    std::chrono::system_clock::time_point timePoint;
    // If we do not loop, we check immediately
    if (!m_config.loop)
        timePoint = std::chrono::system_clock::now();

    // If dark, compute next time point after sunrise
    else if (!isBright())
    {
        auto now = boost::posix_time::second_clock::local_time();
        int sunriseSeconds = (int)(m_config.sunrise * 60 * 60) - m_config.SunRSOffset;
        sunriseSeconds /= m_config.liveInterval;
        sunriseSeconds *= m_config.liveInterval;
        auto date = boost::posix_time::ptime(now.date(), boost::posix_time::seconds(sunriseSeconds));
        date -= boost::posix_time::seconds(get_tzOffset(NULL));

        // Check if after sunset (after noon).
        if (now.time_of_day().total_seconds() > (12 * 60 * 60))
        {
            date += boost::gregorian::days(1);
        }

        timePoint = std::chrono::system_clock::from_time_t(boost::posix_time::to_time_t(date));
    }
    else
    {
        auto seconds = time(nullptr);
        seconds /= m_config.liveInterval;
        ++seconds;
        seconds *= m_config.liveInterval;
        timePoint = std::chrono::system_clock::from_time_t(seconds);
    }

    if (VERBOSE_HIGH)
    {
        std::time_t t = std::chrono::system_clock::to_time_t(timePoint);
        std::cout << "Next poll: " << std::ctime(&t) << std::endl;
    }

    return timePoint;
}
