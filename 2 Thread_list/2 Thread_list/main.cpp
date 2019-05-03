#include <Windows.h>
#include <process.h>
#include <cstdio>
#include <ctime>
#include <list>
#include <conio.h>
#pragma comment(lib, "Winmm.lib")

#define df_UPDATE_THREAD_MAX 5
#define df_INTERVAL_PRINT	1000
#define df_INTERVAL_POP		30
#define df_INTERVAL_PUSH	1000

bool	g_bShutdown;
HANDLE	g_hSaveEvent;
std::list<int>		g_lstData;
CRITICAL_SECTION	g_csData;

// PrintThread
//	1. Print List's data every second
UINT __stdcall PrintThread(LPVOID lpParam);

// PopThread
//	1. pop back into list every 30 ms
UINT __stdcall PopThread(LPVOID lpParam);

// PushThread x 3
//	1. insert rand value into list every second
UINT __stdcall PushThread(LPVOID lpParam);

// SaveThread
//	1. Wake up by the main thread's event
//	2. When it wakes up, it saves the contents of List as file TXT.
//	(If you save more than 2 times, delete the existing file and save it)
UINT __stdcall SaveThread(LPVOID lpParam);

// MainThread
//	1. Press `S` key wakes SaveThread
//	2. Press `Q` key to quit all threads
void main()
{
	timeBeginPeriod(1);
	InitializeCriticalSection(&g_csData);

	g_hSaveEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	HANDLE hPopThread = (HANDLE)_beginthreadex(NULL, 0, PopThread, 0, 0, NULL);
	HANDLE hPushThread[df_UPDATE_THREAD_MAX];
	for (int iCnt = 0; iCnt < df_UPDATE_THREAD_MAX; ++iCnt)
		hPushThread[iCnt] = (HANDLE)_beginthreadex(NULL, 0, PushThread, 0, 0, NULL);
	HANDLE hSaveThread = (HANDLE)_beginthreadex(NULL, 0, SaveThread, 0, 0, NULL);
	HANDLE hPrintThread = (HANDLE)_beginthreadex(NULL, 0, PrintThread, 0, 0, NULL);

	while (!g_bShutdown)
	{
		if (_kbhit())
		{
			WCHAR wchKey = _getwch();
			if (wchKey == L's' || wchKey == L'S')
				SetEvent(g_hSaveEvent);

			if (wchKey == L'q' || wchKey == L'Q')
			{
				g_bShutdown = true;
				SetEvent(g_hSaveEvent);
			}
		}
		Sleep(1); // Yield Process
	}

	HANDLE hThread[3] = { hPrintThread, hPopThread, hSaveThread };
	WaitForMultipleObjects(3, hThread, true, INFINITE);
	for (int iCnt = 0; iCnt < 3; ++iCnt)
		CloseHandle(hThread[iCnt]);

	WaitForMultipleObjects(df_UPDATE_THREAD_MAX, hPushThread, true, INFINITE);
	for (int iCnt = 0; iCnt < df_UPDATE_THREAD_MAX; ++iCnt)
		CloseHandle(hPushThread[iCnt]);

	DeleteCriticalSection(&g_csData);
	timeEndPeriod(1);
}

UINT __stdcall PrintThread(LPVOID lpParam)
{
	wprintf(L"[%d]	PrintThread() Start\n", GetCurrentThreadId());

	while (!g_bShutdown)
	{

		wprintf(L"[%d]	PrintThread()	#", GetCurrentThreadId());
		// !! Alert !! List traversal must be synchronized 
		EnterCriticalSection(&g_csData);
		for (auto iter = g_lstData.begin(); iter != g_lstData.end(); ++iter)
			wprintf(L"%00d ", *iter);
		LeaveCriticalSection(&g_csData);
		wprintf(L"\n");

		Sleep(df_INTERVAL_PRINT);
	}

	wprintf(L"[%d]	PrintThread() Exit\n", GetCurrentThreadId());
	return 0;
}

UINT __stdcall PopThread(LPVOID lpParam)
{
	wprintf(L"[%d]	PopThread() Start\n", GetCurrentThreadId());

	while (!g_bShutdown)
	{
		if (!g_lstData.empty())
		{
			EnterCriticalSection(&g_csData);
			g_lstData.pop_front();
			LeaveCriticalSection(&g_csData);
		}
		Sleep(df_INTERVAL_POP);
	}

	wprintf(L"[%d]	PopThread() Exit\n", GetCurrentThreadId());
	return 0;
}

UINT __stdcall PushThread(LPVOID lpParam)
{
	wprintf(L"[%d]	PushThread() Start\n", GetCurrentThreadId());

	int iData;
	srand(time(NULL) + GetCurrentThreadId() + GetCurrentProcessId());
	while (!g_bShutdown)
	{
		iData = rand();

		EnterCriticalSection(&g_csData);
		g_lstData.push_front(iData);
		LeaveCriticalSection(&g_csData);

		Sleep(df_INTERVAL_PUSH);
	}

	wprintf(L"[%d]	PushThread() Exit\n", GetCurrentThreadId());
	return 0;
}

UINT __stdcall SaveThread(LPVOID lpParam)
{
	wprintf(L"[%d]	SaveThread() Start\n", GetCurrentThreadId());

	FILE * pFile;
	while (!g_bShutdown)
	{
		WaitForSingleObject(g_hSaveEvent, INFINITE);

		wprintf(L"[%d]	SaveThread() \n", GetCurrentThreadId());
		_wfopen_s(&pFile, L"DataList.txt", L"w");

		EnterCriticalSection(&g_csData);
		for (auto iter = g_lstData.begin(); iter != g_lstData.end(); ++iter)
			fprintf_s(pFile, "%d	", (*iter));
		LeaveCriticalSection(&g_csData);
		fclose(pFile);
	}

	wprintf(L"[%d]	SaveThread() Exit\n", GetCurrentThreadId());
	return 0;
}