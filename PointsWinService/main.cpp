#include <Windows.h>
#include <tchar.h>
#include <string>
#include <cstdio>
#include <curl\curl.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <cerrno>
#include <filesystem>

namespace fs = std::filesystem; 

SERVICE_STATUS        g_ServiceStatus = { 0 };
SERVICE_STATUS_HANDLE g_StatusHandle = NULL;
HANDLE                g_ServiceStopEvent = INVALID_HANDLE_VALUE;

VOID WINAPI ServiceMain(DWORD argc, LPTSTR* argv);
VOID WINAPI ServiceCtrlHandler(DWORD);
DWORD WINAPI ServiceWorkerThread(LPVOID lpParam);

#define SERVICE_NAME  _T("PointService")

std::string readFileContents(const std::string& filename)
{
    std::ifstream file(filename);
    std::string contents;
    std::string line;

    if (file.is_open())
    {
        std::string line;
        while (std::getline(file, line))
        {
            contents += line + "\n";
        }
        file.close();
    }
    else
    {
        std::ofstream logfile("C:\\PointService\\log.txt", std::ios_base::app);
        if (logfile.is_open())
        {
            logfile << "Cannot Open File: " << filename << std::endl;
            logfile << "Erorr: " << strerror(errno) << std::endl; 
            logfile.close();
        }
    }
    // Remove the trailing newline character, if present
    if (!contents.empty() && contents.back() == '\n') {
        contents.pop_back();
    }

    return contents;
}

int _tmain(int argc, TCHAR* argv[])
{
    OutputDebugString(_T("PointService: Main: Entry"));

    SERVICE_TABLE_ENTRY ServiceTable[] =
    {
        { const_cast<LPWSTR>(SERVICE_NAME), (LPSERVICE_MAIN_FUNCTION)ServiceMain },
        { NULL, NULL }
    };

    if (StartServiceCtrlDispatcher(ServiceTable) == FALSE)
    {
        OutputDebugString(_T("PointService: Main: StartServiceCtrlDispatcher returned error"));
        return GetLastError();
    }

    OutputDebugString(_T("PointService: Main: Exit"));
    return 0;
}

VOID WINAPI ServiceMain(DWORD argc, LPTSTR* argv)
{
    DWORD Status = E_FAIL;

    OutputDebugString(_T("PointService: ServiceMain: Entry"));

    g_StatusHandle = RegisterServiceCtrlHandler(SERVICE_NAME, ServiceCtrlHandler);

    // Initialize hThread before any jump statement
    HANDLE hThread = CreateThread(NULL, 0, ServiceWorkerThread, NULL, 0, NULL);

    if (g_StatusHandle == NULL)
    {
        OutputDebugString(_T("PointService: ServiceMain: RegisterServiceCtrlHandler returned error"));
        goto EXIT; // This is line 58 causing the error
    }

    // Check if hThread is created successfully
    if (hThread == NULL)
    {
        OutputDebugString(_T("PointService: ServiceMain: CreateThread failed"));
        goto EXIT;
    }

    // Tell the service controller we are starting
    ZeroMemory(&g_ServiceStatus, sizeof(g_ServiceStatus));
    g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_ServiceStatus.dwControlsAccepted = 0;
    g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    g_ServiceStatus.dwWin32ExitCode = 0;
    g_ServiceStatus.dwServiceSpecificExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 0;

    if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
    {
        OutputDebugString(_T("PointService: ServiceMain: SetServiceStatus returned error"));
    }

    /*
     * Perform tasks necessary to start the service here
     */
    OutputDebugString(_T("PointService: ServiceMain: Performing Service Start Operations"));

    // Create stop event to wait on later.
    g_ServiceStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (g_ServiceStopEvent == NULL)
    {
        OutputDebugString(_T("PointService: ServiceMain: CreateEvent(g_ServiceStopEvent) returned error"));

        g_ServiceStatus.dwControlsAccepted = 0;
        g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        g_ServiceStatus.dwWin32ExitCode = GetLastError();
        g_ServiceStatus.dwCheckPoint = 1;

        if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
        {
            OutputDebugString(_T("PointService: ServiceMain: SetServiceStatus returned error"));
        }
        goto EXIT;
    }

    // Tell the service controller we are started
    g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    g_ServiceStatus.dwWin32ExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 0;

    if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
    {
        OutputDebugString(_T("PointService: ServiceMain: SetServiceStatus returned error"));
    }

    OutputDebugString(_T("PointService: ServiceMain: Waiting for Worker Thread to complete"));

    // Wait until our worker thread exits effectively signaling that the service needs to stop
    WaitForSingleObject(hThread, INFINITE);

    OutputDebugString(_T("PointService: ServiceMain: Worker Thread Stop Event signaled"));

EXIT:
    /*
     * Perform any cleanup tasks
     */
    OutputDebugString(_T("PointService: ServiceMain: Performing Cleanup Operations"));

    if (hThread != NULL)
        CloseHandle(hThread);

    CloseHandle(g_ServiceStopEvent);

    g_ServiceStatus.dwControlsAccepted = 0;
    g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    g_ServiceStatus.dwWin32ExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 3;

    if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
    {
        OutputDebugString(_T("PointService: ServiceMain: SetServiceStatus returned error"));
    }

    OutputDebugString(_T("PointService: ServiceMain: Exit"));
}

VOID WINAPI ServiceCtrlHandler(DWORD CtrlCode)
{
    OutputDebugString(_T("PointService: ServiceCtrlHandler: Entry"));

    switch (CtrlCode)
    {
    case SERVICE_CONTROL_STOP:

        OutputDebugString(_T("PointService: ServiceCtrlHandler: SERVICE_CONTROL_STOP Request"));

        if (g_ServiceStatus.dwCurrentState != SERVICE_RUNNING)
            break;

        /*
         * Perform tasks neccesary to stop the service here
         */

        g_ServiceStatus.dwControlsAccepted = 0;
        g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
        g_ServiceStatus.dwWin32ExitCode = 0;
        g_ServiceStatus.dwCheckPoint = 4;

        if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
        {
            OutputDebugString(_T("PointService: ServiceCtrlHandler: SetServiceStatus returned error"));
        }

        // This will signal the worker thread to start shutting down
        SetEvent(g_ServiceStopEvent);

        break;

    default:
        break;
    }

    OutputDebugString(_T("PointService: ServiceCtrlHandler: Exit"));
}


DWORD WINAPI ServiceWorkerThread(LPVOID lpParam)
{
    OutputDebugString(_T("PointService: ServiceWorkerThread: Entry"));
    //  Periodically check if the service has been requested to stop
    while (WaitForSingleObject(g_ServiceStopEvent, 0) != WAIT_OBJECT_0)
    {
        std::string Team1Client1API = "0de7dc34d37098e7a3eca9c7f087e56b";
        std::string MachineID = "Team1Client1";
        CURL* curl;
        CURLcode res;
        std::ofstream logfile("C:\\PointService\\log.txt", std::ios_base::app);


        // Initial winsock
        curl_global_init(CURL_GLOBAL_ALL);

        // get curl handle
        curl = curl_easy_init();

        if (curl) {

            // Filename Declaration
            std::string filename = "C:\\King.txt";

            if (!fs::exists(filename))
            {
                std::ofstream outfile(filename);

                if (!outfile.is_open())
                {
                    if (logfile.is_open())
                    {
                        logfile << "Error Creating File " << filename << std::endl;
                        logfile.close();
                    }
                }

                outfile.close();
            }
            // Retrieve Contents of file
            std::string fileContents = readFileContents(filename);

            if (logfile.is_open())
            {
                logfile << "Filename: " << filename << std::endl;
                logfile << "File Contents: " << fileContents << std::endl;
                logfile.close();
            }

            // set target URL
            curl_easy_setopt(curl, CURLOPT_URL, "https://pointserv.local/api/post_data");

            /* Set the value of verification to null */
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);

            std::string jsonData = "{\"MachineID\": \"" + MachineID + "\", \"TeamID\": \"" + fileContents + "\"}";

            // Specify the POST data
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonData.c_str());

            // Set Custom Header for API 
            struct curl_slist* headers = nullptr;
            headers = curl_slist_append(headers, ("X-API-Key: " + Team1Client1API).c_str());
            headers = curl_slist_append(headers, "Content-Type: application/json");
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

            // Perform the request
            res = curl_easy_perform(curl);

            if (res != CURLE_OK) {
                fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            }
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
        }
        curl_global_cleanup();
        Sleep(5000);
    }

    OutputDebugString(_T("PointService: ServiceWorkerThread: Exit"));

    return ERROR_SUCCESS;
}