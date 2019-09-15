#include <Windows.h>
#include <process.h>
#include <cstdio>
#include <ctime>
#include <list>
#include <conio.h>
#pragma comment(lib, "Winmm.lib")
#include "CRingBuffer.h"
#include <random>

using namespace std;

#define df_UPDATE_THREAD_MAX 10
#define df_INTERVAL_PUSH	100

//-----------------------------------------------
// Message Structure
//-----------------------------------------------
// shType 이 0 (AddStr) 인 경우는 헤더 뒤에 shStrLen 길이만큼 문자열이 들어감.
// shType 이 1,2,3 인 경우는 헤더 뒤에 문자열이 없어도 됨.
struct st_MSG_HEADER
{
	short shType;
	WCHAR String[50];
};

//-----------------------------------------------
// Message Type
//-----------------------------------------------
#define df_TYPE_ADD_STR		0
#define df_TYPE_DEL_STR		1
#define df_TYPE_PRINT_LIST	2
#define df_TYPE_QUIT		3	

//-----------------------------------------------
// Contents Part : string list
//-----------------------------------------------
list<wstring>	g_List;
SRWLOCK	g_ListCs;

//-----------------------------------------------
// Thread Message Queue
//-----------------------------------------------
mylib::CRingBuffer	g_MsgQ(50000);

bool g_bShutdown;
HANDLE g_hEvent;

int64_t rand(const int64_t & min, const int64_t & max);

UINT __stdcall UpdateThread(LPVOID lpParam);

void main()
{
	timeBeginPeriod(1);
	InitializeSRWLock(&g_ListCs);
	///srand((unsigned int)time(NULL));

	g_hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	HANDLE hUpdateThread[df_UPDATE_THREAD_MAX];
	for (int iCnt = 0; iCnt < df_UPDATE_THREAD_MAX; ++iCnt)
		hUpdateThread[iCnt] = (HANDLE)_beginthreadex(NULL, 0, UpdateThread, 0, 0, NULL);

	WCHAR*	pStr = L"TESTSTRING";
	int iStrlen = wcslen(pStr);
	while (1)
	{

		// 1. 메세지 생성
		//	st_MSG_HEADER.shType = 0~2(Random)
		//	st_MSG_HEADER.String = wstring(Random)
		//  "문자열"  < 위 고정 문자열의 범위 내에서 랜덤하게 입력.
		int iStrPos = rand(0, iStrlen-1);/// rand() % iStrlen-1 ;
		st_MSG_HEADER		stHeader;
		stHeader.shType = rand(0, 2);/// rand() % 3;
		wcscpy_s(stHeader.String, &pStr[iStrPos]);
		if (_kbhit())
		{
			if (_getwch() == L' ')
			{
				stHeader.shType = df_TYPE_QUIT;
				g_MsgQ.Lock();
				if (g_MsgQ.GetFreeSize() >= sizeof(st_MSG_HEADER))
					g_MsgQ.Enqueue((char*)&stHeader, sizeof(st_MSG_HEADER));
				g_MsgQ.Unlock();

				SetEvent(g_hEvent);
				break;
			}
		}

		g_MsgQ.Lock();
		if (g_MsgQ.GetFreeSize() >= sizeof(st_MSG_HEADER))
			g_MsgQ.Enqueue((char*)&stHeader, sizeof(st_MSG_HEADER));
		g_MsgQ.Unlock();

		SetEvent(g_hEvent);

		// 입력 간격
		Sleep(df_INTERVAL_PUSH);
	}

	WaitForMultipleObjects(df_UPDATE_THREAD_MAX, hUpdateThread, TRUE, INFINITE);
	for (int iCnt = 0; iCnt < df_UPDATE_THREAD_MAX; ++iCnt)
		CloseHandle(hUpdateThread[iCnt]);

	wprintf(L"[%d]	# ", GetCurrentThreadId());
	for (auto iter = g_List.begin(); iter != g_List.end(); ++iter)
		wprintf(L"[%s] ", (*iter).c_str());
	wprintf(L"\nQ FreeSize: %d \n", g_MsgQ.GetFreeSize());

	timeEndPeriod(1);
}


int64_t rand(const int64_t & min, const int64_t & max)
{
	static std::random_device rnd;
	static thread_local std::mt19937_64 gen(rnd() + GetCurrentThreadId());
	std::uniform_int_distribution<int64_t> dist(min, max);
	return dist(gen);
}

UINT WINAPI UpdateThread(LPVOID lpParam)
{
	wprintf(L"[%d]	UpdateThread() #	Start\n", GetCurrentThreadId());

	bool bShutdown = false;
	while (1)
	{
		WaitForSingleObject(g_hEvent, INFINITE);

		while (1)
		{
			g_MsgQ.Lock();
			if (g_MsgQ.GetUseSize() < sizeof(st_MSG_HEADER))
			{
				g_MsgQ.Unlock();
				break;
			}
			st_MSG_HEADER stHeader;
			g_MsgQ.Dequeue((char*)&stHeader, sizeof(st_MSG_HEADER));
			g_MsgQ.Unlock();

			switch (stHeader.shType)
			{
			case df_TYPE_ADD_STR:
				///wprintf(L"[%d]	Add_str # [%s]\n", GetCurrentThreadId(), stHeader.String);
				AcquireSRWLockExclusive(&g_ListCs);
				g_List.push_back(stHeader.String);
				ReleaseSRWLockExclusive(&g_ListCs);
				break;
			case df_TYPE_DEL_STR:
				///wprintf(L"[%d]	Pop_back # \n", GetCurrentThreadId());
				if (!g_List.empty())
				{
					AcquireSRWLockExclusive(&g_ListCs);
					g_List.pop_back();
					ReleaseSRWLockExclusive(&g_ListCs);
				}
				break;
			case df_TYPE_PRINT_LIST:
				AcquireSRWLockShared(&g_ListCs);
				wprintf(L"[%d]	# ", GetCurrentThreadId());
				for (auto iter = g_List.begin(); iter != g_List.end(); ++iter)
					wprintf(L"[%s] ", (*iter).c_str());
				///wprintf(L"\n");
				wprintf(L"Q FreeSize: %d \n", g_MsgQ.GetFreeSize());
				ReleaseSRWLockShared(&g_ListCs);
				break;
			case df_TYPE_QUIT:
				// TODO: 종료 규칙이 현재 두 개임.
				// 1. QUIT 메시지를 받은 UpdateThread가 g_bShutdown을 false로 변경하여 while문 탈출. 다른 UpdateThread를 깨움
				// 2. 깨어난 UpdateThread는 g_bShutdown이 false가 되었기 때문에 탈출
				//  - 이보다는 종료 규칙이 한 개인게 보다 깔끔함
				// A. QUIT 메시지를 받은 UpdateThread가 다른 UpdateThread에게 QUIT 메시지를 뿌린다던가..
				// B. ManualReset으로 해서 모두가 하나의 Msg를 받게 한다던가..
				bShutdown = true;
				break;
			default:
				wprintf(L"[%d]	UpdateThread() #	Unknown Packet\n", GetCurrentThreadId());
				break;
			}
		}

		if (bShutdown)
		{
			st_MSG_HEADER stHeader;

			stHeader.shType = df_TYPE_QUIT;
			g_MsgQ.Lock();
			if (g_MsgQ.GetFreeSize() >= sizeof(st_MSG_HEADER))
				g_MsgQ.Enqueue((char*)&stHeader, sizeof(st_MSG_HEADER));
			g_MsgQ.Unlock();

			SetEvent(g_hEvent);
			break;
		}
	}

	wprintf(L"[%d]	UpdateThread() #	Exit\n", GetCurrentThreadId());
	return 0;
}