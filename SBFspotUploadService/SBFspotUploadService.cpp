/****************************** Module Header ******************************\
* Module Name:  SBFspotUploadService.cpp
* Project:      SBFspotUploadService
* Copyright (c) Microsoft Corporation.
* 
* The file defines the entry point of the application. According to the 
* arguments in the command line, the function installs or uninstalls or 
* starts the service by calling into different routines.
* 
* This source is subject to the Microsoft Public License.
* See http://www.microsoft.com/en-us/openness/resources/licenses.aspx#MPL.
* All other rights reserved.
* 
* THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND, 
* EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED 
* WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
\***************************************************************************/

#pragma region Includes
#include <stdio.h>
#include <windows.h>
#include "ServiceInstaller.h"
#include "ServiceBase.h"
#include "UploadService.h"
#pragma endregion

void WriteEventLogEntry(PWSTR pszMessage, WORD wType);

// 
// Settings of the service
// 

// Internal name of the service
#define SERVICE_NAME            L"SBFspotUpload"

// Displayed name of the service
#define SERVICE_DISPLAY_NAME    L"SBFspot Upload Service"

// Description of the service
#define SERVICE_DESCRIPTION     L"Uploads SBFspot data to PVOutput.org"

// Service start options.
#define SERVICE_START_TYPE      SERVICE_AUTO_START

// List of service dependencies - "dep1\0dep2\0\0"
#define SERVICE_DEPENDENCIES    L""

// The name of the account under which the service should run
#define SERVICE_ACCOUNT         L"NT AUTHORITY\\LocalService"

// The password to the service account name
#define SERVICE_PASSWORD        NULL

//
//  FUNCTION: wmain(int, wchar_t *[])
//
//  PURPOSE: entrypoint for the application.
// 
//  PARAMETERS:
//    argc - number of command line arguments
//    argv - array of command line arguments
//
//  RETURN VALUE:
//    none
//
//  COMMENTS:
//    wmain() either performs the command line task, or run the service.
//
int wmain(int argc, wchar_t *argv[])
{
	Configuration config;

    if ((argc > 1) && ((*argv[1] == L'-' || (*argv[1] == L'/'))))
    {
        if (_wcsicmp(L"install", argv[1] + 1) == 0)
        {
            if (config.readSettings(argv[0], L"") == Configuration::CFG_OK)
            {
                // Install the service when the command is 
                // "-install" or "/install".
                InstallService(
                    SERVICE_NAME,               // Name of service
                    SERVICE_DISPLAY_NAME,       // Name to display
                    SERVICE_DESCRIPTION,        // Description
                    SERVICE_START_TYPE,         // Service start type
                    SERVICE_DEPENDENCIES,       // Dependencies
                    SERVICE_ACCOUNT,            // Service running account
                    SERVICE_PASSWORD            // Password of the account
                    );
            }
        }
        else if (_wcsicmp(L"remove", argv[1] + 1) == 0)
        {
            // Uninstall the service when the command is 
            // "-remove" or "/remove".
            UninstallService(SERVICE_NAME);
        }
        else
        {
            wprintf(L"Invalid parameter: ");
            wprintf(argv[1] + 1);
        }
    }
    else
    {
        // We get here when:
        //   - executing command without parameters
        //   - starting the service (sc start SBFspotUpload)
        wprintf(L"Parameters:\n");
        wprintf(L" -install  to install the service.\n");
        wprintf(L" -remove   to remove the service.\n");
        wprintf(L"\n\nTo start the service, reboot or type\n\tsc start %s\n", SERVICE_NAME);
        
        CSBFspotUploadService service(SERVICE_NAME);
        CServiceBase::Run(service);
    }

    return 0;
}

void WriteEventLogEntry(PWSTR pszMessage, WORD wType)
{
    HANDLE hEventSource = NULL;
    LPCWSTR lpszStrings[2] = { NULL, NULL };

    hEventSource = RegisterEventSource(NULL, SERVICE_NAME);
    if (hEventSource)
    {
        lpszStrings[0] = SERVICE_NAME;
        lpszStrings[1] = pszMessage;

        ReportEvent(hEventSource,  // Event log handle
            wType,                 // Event type
            0,                     // Event category
            0,                     // Event identifier
            NULL,                  // No security identifier
            2,                     // Size of lpszStrings array
            0,                     // No binary data
            lpszStrings,           // Array of strings
            NULL                   // No binary data
            );

        DeregisterEventSource(hEventSource);
    }
}
