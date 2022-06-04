/****************************** Module Header ******************************\
* Module Name:  UploadService.cpp
* Project:      SBFspotUploadService
* Copyright (c) Microsoft Corporation.
* 
* Provides a sample service class that derives from the service base class - 
* CServiceBase. The sample service logs the service start and stop 
* information to the Application event log, and shows how to run the main 
* function of the service in a thread pool worker thread.
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
#include "../SBFspotUploadCommon/CommonServiceCode.h"
#include <boost/lexical_cast.hpp>
#include <boost/format.hpp>
#include "UploadService.h"
#include "ThreadPool.h"
#pragma endregion

using namespace std;
using namespace boost;

int verbose = 0;
bool quiet = false;
Configuration cfg;
bool bStopping;

CSBFspotUploadService::CSBFspotUploadService(PWSTR pszServiceName, 
                               BOOL fCanStop, 
                               BOOL fCanShutdown, 
                               BOOL fCanPauseContinue)
: CServiceBase(pszServiceName, fCanStop, fCanShutdown, fCanPauseContinue)
{
    bStopping = FALSE;

    // Create a manual-reset event that is not signaled at first to indicate 
    // the stopped signal of the service.
    m_hStoppedEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (m_hStoppedEvent == NULL)
    {
        throw GetLastError();
    }
}


CSBFspotUploadService::~CSBFspotUploadService(void)
{
    if (m_hStoppedEvent)
    {
        CloseHandle(m_hStoppedEvent);
        m_hStoppedEvent = NULL;
    }
}


//
//   FUNCTION: CSBFspotUploadService::OnStart(DWORD, LPWSTR *)
//
//   PURPOSE: The function is executed when a Start command is sent to the 
//   service by the SCM or when the operating system starts (for a service 
//   that starts automatically). It specifies actions to take when the 
//   service starts. In this code sample, OnStart logs a service-start 
//   message to the Application log, and queues the main service function for 
//   execution in a thread pool worker thread.
//
//   PARAMETERS:
//   * dwArgc   - number of command line arguments
//   * lpszArgv - array of command line arguments
//
//   NOTE: A service application is designed to be long running. Therefore, 
//   it usually polls or monitors something in the system. The monitoring is 
//   set up in the OnStart method. However, OnStart does not actually do the 
//   monitoring. The OnStart method must return to the operating system after 
//   the service's operation has begun. It must not loop forever or block. To 
//   set up a simple monitoring mechanism, one general solution is to create 
//   a timer in OnStart. The timer would then raise events in your code 
//   periodically, at which time your service could do its monitoring. The 
//   other solution is to spawn a new thread to perform the main service 
//   functions, which is demonstrated in this code sample.
//
void CSBFspotUploadService::OnStart(DWORD dwArgc, LPWSTR *lpszArgv)
{
	wchar_t msg[512];

    // Log a service start message to the Application log.
    WriteEventLogEntry(L"SBFspotUpload in OnStart", EVENTLOG_INFORMATION_TYPE);

    SC_HANDLE schSCManager = NULL;
    SC_HANDLE schService = NULL;
    LPQUERY_SERVICE_CONFIG lpsc = NULL; 
    DWORD dwBytesNeeded, cbBufSize, dwError; 

	// Open Service Config Mgr
	schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ENUMERATE_SERVICE);
	if (NULL == schSCManager) 
    {
		dwError = GetLastError();
		wsprintf(msg, L"OpenSCManager failed (0x%08X)", dwError);
		WriteEventLogEntry(msg, EVENTLOG_ERROR_TYPE);
		throw dwError;
    }

    // Get a handle to the service.
	schService = OpenService(schSCManager, m_name, SERVICE_QUERY_CONFIG);
    if (schService == NULL)
    { 
		dwError = GetLastError();
        wsprintf(msg, L"OpenService failed (0x%08X)", dwError); 
		WriteEventLogEntry(msg, EVENTLOG_ERROR_TYPE);
        CloseServiceHandle(schSCManager);
		throw dwError;
    }

    // Get the configuration information.
    if( !QueryServiceConfig(schService, NULL, 0, &dwBytesNeeded))
    {
        dwError = GetLastError();
        if (ERROR_INSUFFICIENT_BUFFER == dwError)
        {
            cbBufSize = dwBytesNeeded;
            lpsc = (LPQUERY_SERVICE_CONFIG) LocalAlloc(LMEM_FIXED, cbBufSize);
        }
        else
        {
            wsprintf(msg, L"QueryServiceConfig failed (0x%08X)", dwError);
		    CloseServiceHandle(schService); 
			CloseServiceHandle(schSCManager);
			throw dwError;
        }
    }

	if (!QueryServiceConfig(schService, lpsc, cbBufSize, &dwBytesNeeded)) 
    {
        dwError = GetLastError();
        wprintf(msg, L"QueryServiceConfig failed (0x%08X)", dwError);
		WriteEventLogEntry(msg, EVENTLOG_ERROR_TYPE);
	    CloseServiceHandle(schService); 
		CloseServiceHandle(schSCManager);
		throw dwError;
    }

	// Get fullpath to service
	std::wstring svcpath(lpsc->lpBinaryPathName);

	// Do Cleanup
	if (lpsc)
	{
		LocalFree(lpsc);
		lpsc = NULL;
	}
	if (schService)
	{
		CloseServiceHandle(schService);
		schService = NULL;
	}
	if (schSCManager)
	{
		CloseServiceHandle(schSCManager);
		schSCManager = NULL;
	}

	if (cfg.readSettings(svcpath, L"") == Configuration::CFG_OK)
	{
		// Check if DB is accessible
		db_SQL_Base db;
#if defined(USE_MYSQL)
		db.open(cfg.getSqlHostname(), cfg.getSqlUsername(), cfg.getSqlPassword(), cfg.getSqlDatabase(), cfg.getSqlPort());
#elif defined(USE_SQLITE)
		db.open(cfg.getSqlDatabase());
#endif
		if (!db.isopen())
		{
			wprintf(msg, L"Unable to open database [%s] -- Check configuration", cfg.getSqlDatabase());
			WriteEventLogEntry(msg, EVENTLOG_ERROR_TYPE);
			throw Configuration::CFG_ERROR;
		}

		// DB is open - Check DB Version
		int schema_version = 0;
		db.get_config(SQL_SCHEMAVERSION, schema_version);
		db.close();

		if (schema_version < SQL_MINIMUM_SCHEMA_VERSION)
		{
			wprintf(msg, L"Upgrade your database to version %d", SQL_MINIMUM_SCHEMA_VERSION);
			WriteEventLogEntry(msg, EVENTLOG_ERROR_TYPE);
			throw Configuration::CFG_ERROR;
		}

		// Queue the main service function for execution in a worker thread.
		CThreadPool::QueueUserWorkItem(&CSBFspotUploadService::ServiceWorkerThread, this);
	}
	else
	{
		WriteEventLogEntry(L"Error reading config file" , EVENTLOG_ERROR_TYPE);
		throw Configuration::CFG_ERROR;
	}

	return;
}


//
//   FUNCTION: CSBFspotUploadService::ServiceWorkerThread(void)
//
//   PURPOSE: The method performs the main function of the service. It runs 
//   on a thread pool worker thread.
//
void CSBFspotUploadService::ServiceWorkerThread(void)
{
	Log("Starting ServiceWorkerThread()", ERRLEVEL::LOG_INFO_);

	CommonServiceCode();

	Log("Stopping Service...", ERRLEVEL::LOG_INFO_);

    // Signal the stopped event.
    SetEvent(m_hStoppedEvent);
}

//
//   FUNCTION: CSBFspotUploadService::OnStop(void)
//
//   PURPOSE: The function is executed when a Stop command is sent to the 
//   service by SCM. It specifies actions to take when a service stops 
//   running. In this code sample, OnStop logs a service-stop message to the 
//   Application log, and waits for the finish of the main service function.
//
//   COMMENTS:
//   Be sure to periodically call ReportServiceStatus() with 
//   SERVICE_STOP_PENDING if the procedure is going to take long time. 
//
void CSBFspotUploadService::OnStop()
{
    // Log a service stop message to the Application log.
    WriteEventLogEntry(L"SBFspotUpload in OnStop", 
        EVENTLOG_INFORMATION_TYPE);

    // Indicate that the service is stopping and wait for the finish of the 
    // main service function (ServiceWorkerThread).
    bStopping = TRUE;
    if (WaitForSingleObject(m_hStoppedEvent, INFINITE) != WAIT_OBJECT_0)
    {
        throw GetLastError();
    }
}