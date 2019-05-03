#include <Windows.h>
#include <process.h>
#include <cstdio>
#include <ctime>
#pragma comment(lib, "Winmm.lib")

#define df_UPDATE_THREAD_MAX 3

CRITICAL_SECTION g_cs;
LONGLONG g_Data = 0;			// Virtual Data
LONGLONG g_Connect = 0;			// Virtual Client
bool	 g_bShutdown;

UINT __stdcall AcceptThread(LPVOID lpParam);
UINT __stdcall DisconnectThread(LPVOID lpParam);
UINT __stdcall UpdateThread(LPVOID lpParam);

void main()
{
	DWORD	dwStartTick = timeGetTime();
	DWORD	dwPrintTick = dwStartTick;
	DWORD	dwCurrentTick;

	timeBeginPeriod(1);
	InitializeCriticalSection(&g_cs);

	HANDLE hAcceptThread = (HANDLE)_beginthreadex(NULL, 0, AcceptThread, 0, 0, NULL);
	HANDLE hDisconnectThread = (HANDLE)_beginthreadex(NULL, 0, DisconnectThread, (void*)1, 0, NULL);
	HANDLE	hUpdateThread[df_UPDATE_THREAD_MAX];
	for (int iCnt = 0; iCnt < df_UPDATE_THREAD_MAX; ++iCnt)
		hUpdateThread[iCnt] = (HANDLE)_beginthreadex(NULL, 0, UpdateThread, 0, 0,NULL);

	while (!g_bShutdown)
	{
		dwCurrentTick = timeGetTime();

		if (dwCurrentTick - dwStartTick >= 20000)		// Exit 20 seconds later
			g_bShutdown = true;

		wprintf(L"[%d]	main() #		g_Connect : %lld \n", GetCurrentThreadId(), g_Connect);
		dwPrintTick = dwCurrentTick;

		Sleep(1000);	// Yield Process
	}


	//------------------------------------------------
	// Release Thread
	//------------------------------------------------
	// * Wait for thread exit

	// WaitForMultipleObjects()
	// - Success Return Value:	WAIT_OBJECT_0 ~ WAIT_OBJECT_0 + nCount-1 or WAIT_TIMEOUT
	// - Failed Return Value:	WAIT_FAILED
	HANDLE hThread[5] = { hAcceptThread, hDisconnectThread, hUpdateThread[0], hUpdateThread[1], hUpdateThread[2] };
	WaitForMultipleObjects(5, hThread, TRUE, INFINITE);

	DWORD ExitCode;
	for (int iCnt = 0; iCnt < 5; ++iCnt)
	{
		GetExitCodeThread(hThread[iCnt], &ExitCode);
		if (ExitCode != 0)
		{
			wprintf(L"[%d]	# Exit Error\n", GetThreadId(hThread[iCnt]));
			continue;
		}
		CloseHandle(hThread[iCnt]);
	}

	DeleteCriticalSection(&g_cs);
	timeEndPeriod(1);
}

UINT __stdcall AcceptThread(LPVOID lpParam)
{
	// The seed value of rand() is applied separately for each thread
	srand((unsigned int)(time(NULL) + GetCurrentThreadId() + (UINT)lpParam));

	while (!g_bShutdown)
	{
		// TODO: AcceptThread -	Interlocked Operation
		InterlockedIncrement64(&g_Connect);

		// Increases g_Connect randomly every 100 to 1000ms
		DWORD dwUpdateInterval = (unsigned int)rand() % 900 + 100;
		Sleep(dwUpdateInterval);	// Yield Process
	}
	return 0;
}

UINT __stdcall DisconnectThread(LPVOID lpParam)
{
	// The seed value of `rand()` is applied separately for each thread
	srand((unsigned int)(time(NULL) + GetCurrentThreadId()));

	while (!g_bShutdown)
	{
		if (g_Connect > 0)
			// TODO: DisconnectThread -	Interlocked Operation
			InterlockedDecrement64(&g_Connect);

		// Reduces g_Connect randomly every 100 to 1000ms
		DWORD dwUpdateInterval = (unsigned int)rand() % 900 + 100;
		Sleep(dwUpdateInterval);	// Yield Process
	}
	return 0;
}

UINT __stdcall UpdateThread(LPVOID lpParam)
{

	while (!g_bShutdown)
	{
		// TODO: UpdateThread - CRITICAL_SECTION
		///EnterCriticalSection(&cs);	// *Wrong Use :: Not multi-threaded
		EnterCriticalSection(&g_cs);	// *Correct Use :: Lock only when necessary
		++g_Data;
		if (g_Data % 1000 == 0)	
			wprintf(L"[%d]	UpdateThread() #	g_Data : %lld \n", GetCurrentThreadId(), g_Data);
		LeaveCriticalSection(&g_cs);
		///LeaveCriticalSection(&cs);

		Sleep(1);	// Yield Process
	}
	return 0;
}