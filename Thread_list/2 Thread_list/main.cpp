#include <Windows.h>
#include <process.h>
#include <cstdio>
#include <ctime>
#include <list>
#include <conio.h>
#pragma comment(lib, "Winmm.lib")
#include <random>

#define df_UPDATE_THREAD_MAX 4
#define df_INTERVAL_PRINT	500
#define df_INTERVAL_POP		30
#define df_INTERVAL_PUSH	1000

bool	g_bShutdown;
HANDLE	g_hSaveEvent;
std::list<int>		g_lstData;
CRITICAL_SECTION	g_csData;


int64_t rand(const int64_t& min, const int64_t& max);

// PrintThread
//	1. 1초마다 리스트 출력
UINT __stdcall PrintThread(LPVOID lpParam);

// PopThread
//	1. df_INTERVAL_POP마다 리스트에서 pop
UINT __stdcall PopThread(LPVOID lpParam);

// PushThread x 3
//	1. df_INTERVAL_PUSH마다 난수를 리스트에 push
UINT __stdcall PushThread(LPVOID lpParam);

// SaveThread
//	1. 메인 스레드의 호출로 깨어남
//	2. 깨어나면 *.txt에 현재 리스트의 항목들을 저장
UINT __stdcall SaveThread(LPVOID lpParam);

// MainThread
//	1. `S`를 누르면 SaveThread가 깨어남
//	2. `Q`를 누르면 모든 스레드를 종료함
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

int64_t rand(const int64_t & min, const int64_t & max)
{
	// 시드 설정
	static std::random_device g_rnd;	// 외부 장치에서 랜덤 시퀀스 생성
	static thread_local std::mt19937_64 gen(g_rnd()+GetCurrentThreadId());	// 64-bit 랜덤 생성기 (32-bit 버전은 mt19937)

	// 분포 설정
	std::uniform_int_distribution<int64_t> dist(min, max); // 지정한 범위 내 정수

	// 난수 생성
	return dist(gen);
}

UINT __stdcall PrintThread(LPVOID lpParam)
{
	wprintf(L"[%d]	PrintThread() Start\n", GetCurrentThreadId());

	while (!g_bShutdown)
	{

		wprintf(L"[%d]	PrintThread()	#", GetCurrentThreadId());
		// 리스트 순회는 반드시 동기화해야함
		EnterCriticalSection(&g_csData);
		for (auto iter = g_lstData.begin(); iter != g_lstData.end(); ++iter)
			wprintf(L"%05d ", *iter);
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
	///srand(time(NULL) + GetCurrentThreadId() + GetCurrentProcessId());
	while (!g_bShutdown)
	{
		iData = (int)rand(0, 99999);/*rand()*/;

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