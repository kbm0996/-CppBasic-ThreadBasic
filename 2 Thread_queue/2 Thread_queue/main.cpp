#include <Windows.h>
#include <process.h>
#include <cstdio>
#include <ctime>
#include <list>
#include <conio.h>
#pragma comment(lib, "Winmm.lib")
#include "CRingBuffer.h"

using namespace std;

#define df_WORKER_MAX 10

//-----------------------------------------------
// Message Structure
//-----------------------------------------------
// shType �� 0 (AddStr) �� ���� ��� �ڿ� shStrLen ���̸�ŭ ���ڿ��� ��.
// shType �� 1,2,3 �� ���� ��� �ڿ� ���ڿ��� ��� ��.
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

UINT __stdcall WorkerThread(LPVOID lpParam);

void main()
{
	timeBeginPeriod(1);
	InitializeSRWLock(&g_ListCs);
	srand((unsigned int)time(NULL));

	g_hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	HANDLE hWorkerThread[df_WORKER_MAX];
	for (int iCnt = 0; iCnt < df_WORKER_MAX; ++iCnt)
		hWorkerThread[iCnt] = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, 0, 0, NULL);

	WCHAR*	pStr = L"TESTSTRING";
	while (1)
	{

		// 1. Create Message
		//	st_MSG_HEADER.shType = 0~2(Random)
		//	st_MSG_HEADER.String = wstring(Random)
		//  "���ڿ�"  < �� ���� ���ڿ��� ���� ������ �����ϰ� �Է�.
		int iStrPos = rand() % 9;
		st_MSG_HEADER		stHeader;
		stHeader.shType = rand() % 3;

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

		// Input Interval
		//TODO: Testing while changing the insertion time
		Sleep(1);
	}

	WaitForMultipleObjects(df_WORKER_MAX, hWorkerThread, TRUE, INFINITE);
	for (int iCnt = 0; iCnt < df_WORKER_MAX; ++iCnt)
		CloseHandle(hWorkerThread[iCnt]);

	wprintf(L"[%d]	# ", GetCurrentThreadId());
	for (auto iter = g_List.begin(); iter != g_List.end(); ++iter)
		wprintf(L"[%s] ", (*iter).c_str());
	wprintf(L"\nQ FreeSize: %d \n", g_MsgQ.GetFreeSize());

	timeEndPeriod(1);
}


UINT WINAPI WorkerThread(LPVOID lpParam)
{
	wprintf(L"[%d]	WorkerThread() #	Start\n", GetCurrentThreadId());

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
				// TODO: ���� ��Ģ�� ���� �� ����.
				// 1. QUIT �޽����� ���� WorkerThread�� g_bShutdown�� false�� �����Ͽ� while�� Ż��. �ٸ� WorkerThread�� ����
				// 2. ��� WorkerThread�� g_bShutdown�� false�� �Ǿ��� ������ Ż��
				//  - �̺��ٴ� ���� ��Ģ�� �� ���ΰ� ���� �����
				// A. QUIT �޽����� ���� WorkerThread�� �ٸ� WorkerThread���� QUIT �޽����� �Ѹ��ٴ���..
				// B. ManualReset���� �ؼ� ��ΰ� �ϳ��� Msg�� �ް� �Ѵٴ���..
				bShutdown = true;
				break;
			default:
				wprintf(L"[%d]	WorkerThread() #	Unknown Packet\n", GetCurrentThreadId());
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

	wprintf(L"[%d]	WorkerThread() #	Exit\n", GetCurrentThreadId());
	return 0;
}