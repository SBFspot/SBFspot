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

#include "misc.h"
#include "bluetooth.h"

unsigned char CommBuf[COMMBUFSIZE];    //read buffer

struct sockaddr_in addr_in, addr_out;
int bytes_in_buffer = 0;
int bufptr = 0;
SOCKET sock = 0;

#ifdef WIN32
//http://www.winsocketdotnetworkprogramming.com/winsock2programming/winsock2advancedotherprotocol4p.html
//Windows Sockets Error Codes: http://msdn.microsoft.com/en-us/library/ms740668(v=vs.85).aspx

int bthConnect(char *btAddr)
{
	WSADATA wsd;
	SOCKADDR_BTH sab;
	int WSALastError = 0;

    if (WSAStartup(MAKEWORD(2, 2), &wsd) != 0)
		return WSAGetLastError();	//Unable to load Winsock

	BT_ADDR addr;
	if (str2ba(btAddr, &addr) != 0)
		return WSAEDESTADDRREQ;

    sock = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
    if (sock == INVALID_SOCKET)
    {
		//Unable to create socket
		WSALastError = WSAGetLastError();
        WSACleanup();
        return WSALastError;
	}

    memset (&sab, 0, sizeof(sab));
    sab.addressFamily  = AF_BTH;
    // Set the btAddr member to a BT_ADDR variable that
    // contains the address of the target device. App
    // can accept the device address as a string but must convert
    // the address and store it in a variable of type BT_ADDR.
    sab.btAddr = addr;

    // If the service identifier is available, then set the
    // serviceClassId member of SOCKADDR_BTH to the GUID of
    // the RFCOMM-based service. In this case, the client
    // performs an SDP query and then uses the resulting server channel.
    // sab.serviceClassId = nguiD;

    // Or If you want to use a hard-coded channel number, set the
    // port member of SOCKADDR_BTH to the server channel number (1-31)
    sab.port = 1;

    // Connect to the Bluetooth socket, created previously
    if (connect(sock, (SOCKADDR *)&sab, sizeof(sab)) == SOCKET_ERROR)
    {
		//Failed to connect
		WSALastError = WSAGetLastError();
        closesocket(sock);
        WSACleanup();
        return WSALastError;
    }

	return 0; //OK - Connected
}

int bthClose()
{
	int rc = 0;
	if (sock != 0)
	{
		closesocket(sock);
		sock = 0;
		rc = WSACleanup();
	}

	return rc;
}

int bthSend(unsigned char *btbuffer)
{
	if (DEBUG_NORMAL) HexDump(btbuffer, packetposition, 10);
    int bytes_sent = send(sock, (const char *)btbuffer, packetposition, 0);
	if (bytes_sent >= 0)
	{
		if (DEBUG_NORMAL) std::cout << bytes_sent << " Bytes sent" << std::endl;
	}
	else
	{
		std::cerr << "send() returned an error: " << errno << std::endl;
	}

    return bytes_sent;
}

int bthSearchDevices()
{
	WSADATA m_data;
	SOCKET s;
	WSAPROTOCOL_INFO protocolInfo;
	int protocolInfoSize;
	WSAQUERYSET querySet, *pResults;
	HANDLE hLookup;
	int result;
	BYTE buffer[1024];
	DWORD bufferLength, flags, addressSize;
	CSADDR_INFO *pCSAddr;
	WCHAR addressAsString[1024];

	puts("Scanning for SMA devices...\n");

	// Load the winsock2 library
	if (WSAStartup(MAKEWORD(2,2), &m_data) == 0)
	{
		// Create a bluetooth socket
		s = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
		if (s == INVALID_SOCKET)
		{
			printf("Failed to get bluetooth socket with error code %ld\n", WSAGetLastError());
			WSACleanup();
			return 1;
		}

		protocolInfoSize = sizeof(protocolInfo);

		// Get the bluetooth device info using getsockopt()
		if (getsockopt(s, SOL_SOCKET, SO_PROTOCOL_INFO, (char*)&protocolInfo, &protocolInfoSize) != 0)
		{
			printf("Failed to get protocol info, error %ld\n", WSAGetLastError());
			WSACleanup();
			return 1;
		}

		// Query set criteria
		memset(&querySet, 0, sizeof(querySet));
		querySet.dwSize = sizeof(querySet);
		querySet.dwNameSpace = NS_BTH;

		// Set the flags for query
		flags = LUP_RETURN_NAME | LUP_CONTAINERS | LUP_RETURN_ADDR | LUP_FLUSHCACHE | LUP_RETURN_TYPE;

		// Start a device in range query...
		result = WSALookupServiceBegin(&querySet, flags, &hLookup);

		if (result != 0)
		{
            printf("Unable to search bluetooth devices: error %ld\n", WSAGetLastError());
			WSACleanup();
			return 1;
		}

		while (result == 0)
		{
			bufferLength = sizeof(buffer);
			pResults = (WSAQUERYSET *)&buffer;

			result = WSALookupServiceNext(hLookup, flags, &bufferLength, pResults);
			if (result == 0)
            {
                // Get the device info, name, address etc
                pCSAddr = (CSADDR_INFO *)pResults->lpcsaBuffer;
                addressSize = sizeof(addressAsString);

                // Print the remote bluetooth device address...
                if (WSAAddressToString(pCSAddr->RemoteAddr.lpSockaddr, pCSAddr->RemoteAddr.iSockaddrLength, &protocolInfo, (LPWSTR)addressAsString, &addressSize) == 0)
				{
					//We're only interrested in SMA devices
					if (wcslen(pResults->lpszServiceInstanceName) > 0)
					{
						if ((_wcsnicmp(pResults->lpszServiceInstanceName, L"SMA", 3) == 0) || (_wcsnicmp(pResults->lpszServiceInstanceName, L"BlueCN", 6) == 0))
						{
							// print BTaddress without ()
							addressAsString[18] = 0;
							printf("%S '%S'\n", addressAsString+1, pResults->lpszServiceInstanceName);
						}
					}
				}
            }
        }

        // Close handle to the device query
        WSALookupServiceEnd(hLookup);
		// Cleanup the winsock library startup
		WSACleanup();
	}

	puts("Done.");
	return 0;
}

/**
      Implementation of str2ba for winsock2.
 */
int str2ba(const char *straddr, BTH_ADDR *btaddr)
{
      int i;
      unsigned int aaddr[6];
      BTH_ADDR tmpaddr = 0;

      if (sscanf(straddr, "%02x:%02x:%02x:%02x:%02x:%02x",
                  &aaddr[0], &aaddr[1], &aaddr[2],
                  &aaddr[3], &aaddr[4], &aaddr[5]) != 6)
            return 1;
      *btaddr = 0;
      for (i = 0; i < 6; i++) {
            tmpaddr = (BTH_ADDR) (aaddr[i] & 0xff);
            *btaddr = ((*btaddr) << 8) + tmpaddr;
      }
      return 0;
}

int setBlockingMode()
{
	unsigned long Mode = 0;
	return ioctlsocket(sock, FIONBIO, &Mode);
}

int setNonBlockingMode()
{
	unsigned long Mode = 1;
	return ioctlsocket(sock, FIONBIO, &Mode);
}

void bthClear()
{
	unsigned char buf[COMMBUFSIZE];

	setNonBlockingMode();

	int numbytes = 0;
	do
	{
		numbytes = recv(sock, (char *)&buf, sizeof(buf), 0);
		if (numbytes == -1)
		{
			numbytes = 0;
			if (DEBUG_HIGHEST)
			{
				if (WSAGetLastError() == WSAEWOULDBLOCK)
					puts("BT buffer is empty.");
				else
					printf("recv() returned an error: %d", WSAGetLastError());
			}
		}
		else
		{
			if (DEBUG_HIGHEST)
			{
				puts("Still some data available in BT buffer...");
				HexDump(buf, numbytes, 10);
			}
		}
	} while (numbytes != 0);

	setBlockingMode();
}

#endif /* WIN32 */

#ifdef linux

struct sockaddr_rc addr = {0};

int bthConnect(char *btAddr)
{
    int status = 0;
    sock = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);

    addr.rc_family = AF_BLUETOOTH;
    addr.rc_channel = 1;
    str2ba(btAddr, &addr.rc_bdaddr);

    // Connect to Inverter
    status = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
    return(status);
}

int bthClose()
{
	if (sock != 0)
	{
		close(sock);
		sock = 0;
	}
    return 0;
}

int bthSend(unsigned char *btbuffer)
{
	if (DEBUG_NORMAL) HexDump(btbuffer, packetposition, 10);

    int bytes_sent = send(sock, btbuffer, packetposition, 0);

	if (bytes_sent >= 0)
	{
		if (DEBUG_NORMAL) std::cout << bytes_sent << " Bytes sent" << std::endl;
	}
	else
	{
		std::cerr << "send() returned an error: " << errno << std::endl;
	}

    return bytes_sent;
}

int setNonBlockingMode()
{
	int flags = fcntl(sock, F_GETFL, 0);
	return fcntl(sock, F_SETFL, flags | O_NONBLOCK);
}

int setBlockingMode()
{
	int flags = fcntl(sock, F_GETFL, 0);
	return fcntl(sock, F_SETFL, flags & (!O_NONBLOCK));
}

void bthClear()
{
	unsigned char buf[COMMBUFSIZE];

	setNonBlockingMode();

	int numbytes = 0;
	do
	{
		numbytes = recv(sock, (char *)&buf, sizeof(buf), 0);

        if (numbytes == -1)
        {
            numbytes = 0;
            if (errno == EWOULDBLOCK)
            {
                if (DEBUG_HIGHEST)
                    puts("BT buffer is empty.");
            }
            else
                if (DEBUG_HIGHEST)
                    printf("recv() returned an error: %d", errno);
        }
        else if (DEBUG_HIGHEST)
        {
            puts("Still some data available in BT buffer...");
            HexDump(buf, numbytes, 10);
        }
	} while (numbytes != 0);

	setBlockingMode();
}

#endif /* linux */

int bthRead(unsigned char *buf, unsigned int bufsize)
{
    int bytes_read;

    fd_set readfds;

    struct timeval tv;
    tv.tv_sec = BT_TIMEOUT;     //set timeout of reading
    tv.tv_usec = 0;

    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);

    select(sock+1, &readfds, NULL, NULL, &tv);

    if (FD_ISSET(sock, &readfds))       // did we receive anything within BT_TIMEOUT seconds
        bytes_read = recv(sock, (char *)buf, bufsize, 0);
    else
    {
		if (DEBUG_HIGHEST) puts("Timeout reading socket");
        return -1; // E_NODATA
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
			printf("Received %d bytes\n", bytes_read);
    }
    else
	{
		std::cerr << "recv() returned an error: " << errno << std::endl;
        return -1; // E_NODATA
	}

    return bytes_read;
}

