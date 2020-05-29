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

#include "misc.h"
#include "Ethernet.h"

const char *IP_Broadcast = "239.12.255.254";

int ethConnect(short port)
{
    int ret = 0;

#ifdef WIN32
	WSADATA wsa;
     
    if (DEBUG_NORMAL) printf("Initialising Winsock...\n");
    if (WSAStartup(MAKEWORD(2,2),&wsa) != 0)
    {
        printf("Failed while WSAStartup. Error Code : %d\n",WSAGetLastError());
        return -1;
    }
#endif
    // create socket for UDP
    if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1)
    {
        printf ("Socket error : %i\n", sock);
        return -1;
    }

    // set up parameters for UDP
    memset((char *)&addr_out, 0, sizeof(addr_out));
    addr_out.sin_family = AF_INET;
    addr_out.sin_port = htons(port);
    addr_out.sin_addr.s_addr = htonl(INADDR_ANY);
    ret = bind(sock, (struct sockaddr*) &addr_out, sizeof(addr_out));
    // here is the destination IP
	addr_out.sin_addr.s_addr = inet_addr(IP_Broadcast);

    // set options to receive broadcasted packets
    // leave this block and you have normal UDP communication (after the inverter scan)
    struct ip_mreq mreq;

    mreq.imr_multiaddr.s_addr = inet_addr(IP_Broadcast);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    ret = setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char*)&mreq, sizeof(mreq));
	unsigned char loop = 0;
    ret = setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP, (const char *)&loop, sizeof(loop));

    if (ret < 0)
    {
        printf ("setsockopt IP_ADD_MEMBERSHIP failed\n");
        return -1;
    }
    // end of setting broadcast options

    return 0; //OK
}

int ethRead(unsigned char *buf, unsigned int bufsize)
{
    int bytes_read;
    short timeout = 5;
    socklen_t addr_in_len = sizeof(addr_in);

    fd_set readfds;

	do
	{
		struct timeval tv;
		tv.tv_sec = timeout;     //set timeout of reading
		tv.tv_usec = 0;

		FD_ZERO(&readfds);
		FD_SET(sock, &readfds);

		int rc = select(sock+1, &readfds, NULL, NULL, &tv);
		if (DEBUG_HIGHEST) printf("select() returned %d\n", rc);
		if (rc == -1)
		{
			if (DEBUG_HIGHEST) printf("errno = %d\n", errno);
		}

		if (FD_ISSET(sock, &readfds))
			bytes_read = recvfrom(sock, (char*)buf, bufsize, 0, (struct sockaddr *)&addr_in, &addr_in_len);
		else
		{
			if (DEBUG_HIGHEST) puts("Timeout reading socket");
			return -1;
		}

		if ( bytes_read > 0)
		{
			if (bytes_read > MAX_CommBuf)
			{
				MAX_CommBuf = bytes_read;
				if (DEBUG_NORMAL)
					printf("MAX_CommBuf is now %d bytes\n", MAX_CommBuf);
			}
		   	if (DEBUG_NORMAL)
		   	{
				printf("Received %d bytes from IP [%s]\n", bytes_read, inet_ntoa(addr_in.sin_addr));
		   		if (bytes_read == 600 || bytes_read == 608 || bytes_read == 0)
		   			printf(" ==> packet ignored\n");
			}
		}
		else
			printf("recvfrom() returned an error: %d\n", bytes_read);

	} while (bytes_read == 600 || bytes_read == 608); // keep on reading if data received from Energy Meter (600 bytes) or Sunny Home Manager (608 bytes)

    return bytes_read;
}

int ethSend(unsigned char *buffer, const char *toIP)
{
	if (DEBUG_NORMAL) HexDump(buffer, packetposition, 10);

	addr_out.sin_addr.s_addr = inet_addr(toIP);
    size_t bytes_sent = sendto(sock, (const char*)buffer, packetposition, 0, (struct sockaddr *)&addr_out, sizeof(addr_out));

	if (DEBUG_NORMAL) std::cout << bytes_sent << " Bytes sent to IP [" << inet_ntoa(addr_out.sin_addr) << "]" << std::endl;

    return bytes_sent;
}

#ifdef WIN32
int ethClose()
{
	if (sock != 0)
	{
		closesocket(sock);
		sock = 0;
	}

	return 0;
}

#endif

#ifdef linux
int ethClose()
{
	if (sock != 0)
	{
		close(sock);
		sock = 0;
	}
    return 0;
}

int getLocalIP(unsigned char IPaddress[4])
{
    int rc = 0;
    struct ifaddrs *myaddrs;
    struct in_addr *inaddr;

    if(getifaddrs(&myaddrs) == 0)
    {
        for (struct ifaddrs *ifa = myaddrs; ifa != NULL; ifa = ifa->ifa_next)
        {
            if (ifa->ifa_addr != NULL)
            {
                // Find the active network adapter
                if ((ifa->ifa_addr->sa_family == AF_INET) && (ifa->ifa_flags & IFF_UP) && (stricmp(ifa->ifa_name, "lo") != 0))
                {
                    struct sockaddr_in *s4 = (struct sockaddr_in *)ifa->ifa_addr;
                    inaddr = &s4->sin_addr;

                    unsigned long ipaddr = inaddr->s_addr;
                    IPaddress[3] = ipaddr & 0xFF;
                    IPaddress[2] = (ipaddr >> 8) & 0xFF;
                    IPaddress[1] = (ipaddr >> 16) & 0xFF;
                    IPaddress[0] = (ipaddr >> 24) & 0xFF;

                    break;
                }
            }
        }

        freeifaddrs(myaddrs);
    }
    else
        rc = -1;

    return rc;
}
#endif
