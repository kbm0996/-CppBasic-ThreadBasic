# 멀티스레드 환경에서 list 동기화
## 📑 구성

**📋 _main** : 메인 함수, PrintThread 콜백 함수, PopThread 콜백 함수, PushThread 콜백 함수, SaveThread 콜백 함수

## 📋 _main
### ⚙ main() 

1. "S"를 누르면 SaveThread가 깨어남

2. "Q"를 누르면 모든 스레드를 종료함

```cpp
void main()
{
	timeBeginPeriod(1);	// 정밀한 시간 사용을 위한 설정
	InitializeCriticalSection(&g_csData);	// CriticalSection 초기화
	
	// 스레드를 깨우는 이벤트 설정
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
	
	// 스레드 종료 대기
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

```

### ⚙ PrintThread() 

 1. 1초마다 리스트 출력

```cpp
UINT __stdcall PrintThread(LPVOID lpParam)
{
	while (!g_bShutdown)
	{

		wprintf(L"[%d]	PrintThread()	#", GetCurrentThreadId());
		// 리스트 순회는 반드시 동기화
		EnterCriticalSection(&g_csData);
		for (auto iter = g_lstData.begin(); iter != g_lstData.end(); ++iter)
			wprintf(L"%05d ", *iter);
		LeaveCriticalSection(&g_csData);
		wprintf(L"\n");

		Sleep(df_INTERVAL_PRINT);
	}
	return 0;
}
```


### ⚙ PopThread() 

 1. df_INTERVAL_POP마다 리스트에서 pop

```cpp
UINT __stdcall PopThread(LPVOID lpParam)
{
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
	return 0;
}
```


### ⚙ PushThread() 

 1. df_INTERVAL_PUSH마다 난수를 리스트에 push

```cpp
UINT __stdcall PushThread(LPVOID lpParam)
{
	while (!g_bShutdown)
	{
		int iData = (int)rand(0, 99999);

		EnterCriticalSection(&g_csData);
		g_lstData.push_front(iData);
		LeaveCriticalSection(&g_csData);

		Sleep(df_INTERVAL_PUSH);
	}
	return 0;
}
```


### ⚙ SaveThread() 

 1. 메인 스레드의 호출로 깨어남
 
 2. 깨어나면 *.txt에 현재 리스트의 항목들을 저장

```cpp
UINT __stdcall SaveThread(LPVOID lpParam)
{
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
	return 0;
}
```
