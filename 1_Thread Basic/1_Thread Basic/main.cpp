#include <Windows.h>
#include <process.h>
#include <cstdio>
#include <ctime>
#pragma comment(lib, "Winmm.lib")

#define MAX_THREADS 3

CRITICAL_SECTION g_cs;
LONGLONG g_Data = 0;			// 가상의 데이터 처리
LONGLONG g_Connect = 0;			// 가상의 접속자 수
bool				g_bShutdown = false;


unsigned int __stdcall AcceptThread(LPVOID lpParam);
unsigned int __stdcall DisconnectThread(LPVOID lpParam);
unsigned int __stdcall UpdateThread(LPVOID lpParam);

int main()
{
	DWORD	dwTick = timeGetTime();
	DWORD	dwPrintTick = timeGetTime();
	DWORD	dwCurrentTick;
	int		err;

	HANDLE	hUpdateThread[MAX_THREADS];

	timeBeginPeriod(1);
	InitializeCriticalSection(&g_cs);

	HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (hEvent == NULL)
	{
		err = GetLastError();
		wprintf(L"CreateEvent Error : %d\n", err);
	}

	// BeginThread
	HANDLE hAcceptThread = (HANDLE)_beginthreadex(NULL, 0, AcceptThread, 0, 0, NULL);
	if (hAcceptThread == 0)
	{
		err = GetLastError();
		wprintf(L"_beginthreadex Error : hAcceptThread %d\n", err);
	}

	HANDLE hDisconnectThread = (HANDLE)_beginthreadex(NULL, 0, DisconnectThread, 0, 0, NULL);
	if (hDisconnectThread == 0)
	{
		err = GetLastError();
		wprintf(L"_beginthreadex Error : hDisconnectThread %d\n", err);
	}
	for (int iCnt = 0; iCnt < MAX_THREADS; ++iCnt)
	{
		hUpdateThread[iCnt] = (HANDLE)_beginthreadex(NULL, 0, UpdateThread, 0, 0,NULL);
		if (hUpdateThread[iCnt] == 0)
		{
			err = GetLastError();
			wprintf(L"_beginthreadex Error : hUpdateThread[%d] %d\n", iCnt, err);
		}
	}

	while (1)
	{
		if (g_bShutdown)
		{
			SetEvent(hEvent); // Function to convert from `non-signaled` to `signaled`
			break;
		}

		dwCurrentTick = timeGetTime();

		if (dwCurrentTick - dwTick >= 20000)		// 20초 후 g_Shutdown = true 로 다른 스레드들 종료
			g_bShutdown = true;

		if (dwCurrentTick - dwPrintTick >= 1000)	// 1초마다 g_Connect 를 출력
		{
			wprintf(L"[%d]	main() #		g_Connect : %lld \n", GetCurrentThreadId(), g_Connect);
			dwPrintTick = dwCurrentTick;
		}

		Sleep(1);	// Yield Process
	}


	//------------------------------------------------
	// Release Thread
	//------------------------------------------------
	// * Wait for thread exit

	// WaitForMultipleObjects()
	// - Success Return Value:	WAIT_OBJECT_0 ~ WAIT_OBJECT_0 + nCount-1 or WAIT_TIMEOUT
	// - Failed Return Value:	WAIT_FAILED
	HANDLE hThread[5] = { hAcceptThread, hDisconnectThread, hUpdateThread[0], hUpdateThread[1], hUpdateThread[2] };
	int retval = WaitForMultipleObjects(5, hThread, TRUE, INFINITE);
	if (retval == WAIT_FAILED)
	{
		err = GetLastError();
		wprintf(L"WaitForMultipleObjects Error : %d \n", err);
	}

	DeleteCriticalSection(&g_cs);

	// Thread Exit Error Confirm & CloseHandle
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

	timeEndPeriod(1);
	return 0;
}

unsigned int __stdcall AcceptThread(LPVOID lpParam)
{
	DWORD dwTick = timeGetTime();
	DWORD dwUpdateTiming;
	DWORD dwCurrentTick;

	// The seed value of rand() is applied separately for each thread
	srand((unsigned int)(time(NULL) + GetCurrentThreadId()));
	dwUpdateTiming = (unsigned int)rand() % 900 + 100;

	while (1)
	{
		if (g_bShutdown)
			break;

		dwCurrentTick = timeGetTime();

		// Increases g_Connect randomly every 100 to 1000ms
		if (dwCurrentTick - dwTick == dwUpdateTiming)
		{
			///////////////////////////////////////////////////////////////////////
			// TODO: AcceptThread -	Interlocked Operation
			InterlockedIncrement64(&g_Connect);
			///////////////////////////////////////////////////////////////////////

			dwTick = dwCurrentTick;
			dwUpdateTiming = (unsigned int)rand() % 900 + 100;
		}

		Sleep(1);	// Yield Process
	}
	return 0;
}

unsigned int __stdcall DisconnectThread(LPVOID lpParam)
{
	DWORD dwTick = timeGetTime();
	DWORD dwUpdateTiming;
	DWORD dwCurrentTick;

	// The seed value of `rand()` is applied separately for each thread
	srand((unsigned int)(time(NULL) + GetCurrentThreadId()));
	dwUpdateTiming = (unsigned int)rand() % 900 + 100;

	while (1)
	{
		if (g_bShutdown)
			break;

		dwCurrentTick = timeGetTime();

		// Reduces g_Connect randomly every 100 to 1000ms
		if (dwCurrentTick - dwTick == dwUpdateTiming) 
		{
			if (g_Connect > 0)
			{
				///////////////////////////////////////////////////////////////////////
				// TODO: DisconnectThread -	Interlocked Operation
				InterlockedDecrement64(&g_Connect);
				///////////////////////////////////////////////////////////////////////
			}
			dwTick = dwCurrentTick;
			dwUpdateTiming = (unsigned int)rand() % 900 + 100;
		}

		Sleep(1);	// Yield Process
	}
	return 0;
}

unsigned int __stdcall UpdateThread(LPVOID lpParam)
{
	DWORD dwTick = timeGetTime();
	DWORD dwCurrentTick;

	while (1)
	{
		if (g_bShutdown)
			break;

		dwCurrentTick = timeGetTime();

		///////////////////////////////////////////////////////////////////////
		// TODO: UpdateThread - CRITICAL_SECTION
		///EnterCriticalSection(&cs);		// *Wrong Use :: Not multi-threaded
		if (dwCurrentTick - dwTick >= 10)
		{
			EnterCriticalSection(&g_cs);	// *Correct Use :: Lock only when necessary
			++g_Data;
			if (g_Data % 1000 == 0)			//1000 단위마다 출력
				wprintf(L"[%d]	UpdateThread() #	g_Data : %lld \n", GetCurrentThreadId(), g_Data);
			dwTick = dwCurrentTick;
			LeaveCriticalSection(&g_cs);
		}
		///LeaveCriticalSection(&cs);
		///////////////////////////////////////////////////////////////////////

		Sleep(1);	// Yield Process
	}
	return 0;
}