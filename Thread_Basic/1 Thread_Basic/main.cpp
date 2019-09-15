#include <Windows.h>
#include <process.h>
#include <cstdio>
#include <ctime>
#include <random>
#pragma comment(lib, "Winmm.lib")

#define df_UPDATE_THREAD_MAX 3

CRITICAL_SECTION g_cs;
LONGLONG g_Data = 0;			// ������ ������
LONGLONG g_Connect = 0;			// ������ Ŭ���̾�Ʈ
bool	 g_bShutdown;

int64_t rand(const int64_t min, const int64_t max);

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
		hUpdateThread[iCnt] = (HANDLE)_beginthreadex(NULL, 0, UpdateThread, 0, 0, NULL);

	while (!g_bShutdown)
	{
		dwCurrentTick = timeGetTime();

		if (dwCurrentTick - dwStartTick >= 20000)		// 20�� �� ����
			g_bShutdown = true;

		wprintf(L"[%d]	main() #		g_Connect : %lld \n", GetCurrentThreadId(), g_Connect);
		dwPrintTick = dwCurrentTick;

		Sleep(1000);	// Yield Process
	}


	//------------------------------------------------
	// Release Thread
	//------------------------------------------------
	// * Wait for thread exit

	// WaitForMultipleObjects() ���ϰ�
	// - ������ : WAIT_OBJECT_0 ~ WAIT_OBJECT_0 + nCount-1 or WAIT_TIMEOUT
	// - ���н� : WAIT_FAILED
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

int64_t rand(const int64_t min, const int64_t max)
{
	static std::random_device	g_rn;	// �ܺ� ��ġ���� ���� ������ ����
	static std::mt19937_64		g_rnd;	// 64-bit ���� ������ (32-bit ������ mt19937)
	std::uniform_int_distribution<int64_t> range(min, max); // �Է��� ���� �� �յ��� ���� ���� ����
	return range(g_rnd);
}

UINT __stdcall AcceptThread(LPVOID lpParam)
{
	// rand() �Լ��� �õ尪�� ������ ���� �������־�� ��
	///srand((UINT)time(NULL) + GetCurrentThreadId() + (UINT)lpParam);

	while (!g_bShutdown)
	{
		// TODO: AcceptThread -	Interlocked Operation
		InterlockedIncrement64(&g_Connect);

		// 100~1000ms ������ �ð����� g_Connect ����
		DWORD dwUpdateInterval = (DWORD)rand(100, 1000);/*(unsigned int)rand() % 900 + 100;*/
		Sleep(dwUpdateInterval);	// Yield Process
	}
	return 0;
}

UINT __stdcall DisconnectThread(LPVOID lpParam)
{
	// rand() �Լ��� �õ尪�� ������ ���� �������־�� ��
	///srand((UINT)time(NULL) + GetCurrentThreadId());

	while (!g_bShutdown)
	{
		if (g_Connect > 0)
			// TODO: DisconnectThread -	Interlocked Operation
			InterlockedDecrement64(&g_Connect);

		// 100~1000ms ������ �ð����� g_Connect ����
		DWORD dwUpdateInterval = (DWORD)rand(100, 1000);/*(unsigned int)rand() % 900 + 100;*/
		Sleep(dwUpdateInterval);	// Yield Process
	}
	return 0;
}

UINT __stdcall UpdateThread(LPVOID lpParam)
{

	while (!g_bShutdown)
	{
		// TODO: UpdateThread - CRITICAL_SECTION
		EnterCriticalSection(&g_cs);	// Lock�� �ʿ��� ������
		++g_Data;
		if (g_Data % 1000 == 0)
			wprintf(L"[%d]	UpdateThread() #	g_Data : %lld \n", GetCurrentThreadId(), g_Data);
		LeaveCriticalSection(&g_cs);

		Sleep(1);	// Yield Process
	}
	return 0;
}