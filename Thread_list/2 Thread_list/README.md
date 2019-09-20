# 멀티스레드 환경에서 Queue 동기화
## 📑 구성

**📋 _main** : 메인 함수, UpdateThread 콜백 함수, DBWriteThread

**📋 [CRingBuffer](https://github.com/kbm0996/-DataStructure-RingBuffer)** : 링버퍼 클래스

## 📋 _main
### ⚙ main() : 메세지를 전송하는 `생산자`

명시된 3가지 메세지를 랜덤으로 생성하여 메세지 Queue에 삽입, 스레드 제어

```cpp
void main()
{
	timeBeginPeriod(1); // 정밀한 시간을 얻기 위한 설정
	InitializeSRWLock(&g_ListCs); // SRW(Slim Reader/Writer)Lock 초기화. SpinLock 방식
	g_hEvent = CreateEvent(NULL, FALSE, FALSE, NULL); // 특정 스레드를 깨울 이벤트 설정

  	// 스레드 실행
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
      			// Space 입력 시
			if (_getwch() == L' ')
			{
        			// Queue에 스레드 종료 메시지 삽입
				stHeader.shType = df_TYPE_QUIT;
        
				////////////////////////////////////
				// Queue에 삽입할 경우, 동기화 필수
				////////////////////////////////////
				g_MsgQ.Lock(); 
				if (g_MsgQ.GetFreeSize() >= sizeof(st_MSG_HEADER))
					g_MsgQ.Enqueue((char*)&stHeader, sizeof(st_MSG_HEADER));
				g_MsgQ.Unlock();

				SetEvent(g_hEvent);
				break;
			}
		}

	   	// 2. Queue에 메세지 삽입
	    	////////////////////////////////////
	    	// Queue에 삽입할 경우, 동기화 필수
	    	////////////////////////////////////
		g_MsgQ.Lock();
		if (g_MsgQ.GetFreeSize() >= sizeof(st_MSG_HEADER))
			g_MsgQ.Enqueue((char*)&stHeader, sizeof(st_MSG_HEADER));
		g_MsgQ.Unlock();

    		// 3. UpdateThread 깨우기
		SetEvent(g_hEvent);

		// Queue에 메세지 삽입 간격
		Sleep(df_INTERVAL_PUSH);
	}

  	// 스레드 종료 대기
	WaitForMultipleObjects(df_UPDATE_THREAD_MAX, hUpdateThread, TRUE, INFINITE);
	for (int iCnt = 0; iCnt < df_UPDATE_THREAD_MAX; ++iCnt)
		CloseHandle(hUpdateThread[iCnt]); // 스레드 핸들 반환

  	// list에 남은 문자열이 있는지 확인
	for (auto iter = g_List.begin(); iter != g_List.end(); ++iter)
		wprintf(L"[%s] ", (*iter).c_str());
  
  	// 메세지 Queue에 처리 못한 메세지가 있는지 확인
	wprintf(L"\nQ FreeSize: %d \n", g_MsgQ.GetFreeSize());

	timeEndPeriod(1);
}
```

### ⚙ UpdateThread() : 메세지를 처리하는 `소비자`

 외부로부터 매세지를 받고 메세지의 헤더에 따라 데이터를 처리하는 콜백 함수

```cpp
UINT WINAPI UpdateThread(LPVOID lpParam)
{
	wprintf(L"[%d]	UpdateThread() #	Start\n", GetCurrentThreadId());

	bool bShutdown = false;
	while (1)
	{
		WaitForSingleObject(g_hEvent, INFINITE);

		while (1)
		{
		      	////////////////////////////////////
		      	// Queue를 뽑을 경우, 동기화 필수
		      	////////////////////////////////////
			g_MsgQ.Lock();
			if (g_MsgQ.GetUseSize() < sizeof(st_MSG_HEADER))
			{
				// 동기화가 필요없을 경우, 반드시 Lock 해제
				// -> 해제 안할 경우 Deadlock 발생
				g_MsgQ.Unlock();
				break;
			}
			st_MSG_HEADER stHeader;
			g_MsgQ.Dequeue((char*)&stHeader, sizeof(st_MSG_HEADER));
			g_MsgQ.Unlock();

			switch (stHeader.shType)
			{
			case df_TYPE_ADD_STR:
				////////////////////////////////////
				// list에 삽입할 경우, 동기화 필수
				////////////////////////////////////
				AcquireSRWLockExclusive(&g_ListCs);
				g_List.push_back(stHeader.String);
				ReleaseSRWLockExclusive(&g_ListCs);
        
				break;
			case df_TYPE_DEL_STR:
				if (!g_List.empty())
				{
					////////////////////////////////////
					// list에서 뽑을 경우, 동기화 필수
					////////////////////////////////////
					AcquireSRWLockExclusive(&g_ListCs);
					g_List.pop_back();
					ReleaseSRWLockExclusive(&g_ListCs);
				}
				break;
			case df_TYPE_PRINT_LIST:
				////////////////////////////////////
				// list를 순회할 경우, 동기화 필수
				////////////////////////////////////
				AcquireSRWLockShared(&g_ListCs);
				wprintf(L"[%d]	# ", GetCurrentThreadId());
				for (auto iter = g_List.begin(); iter != g_List.end(); ++iter)
					wprintf(L"[%s] ", (*iter).c_str());
				wprintf(L"Q FreeSize: %d \n", g_MsgQ.GetFreeSize());
				ReleaseSRWLockShared(&g_ListCs);
				break;
			case df_TYPE_QUIT:
				// 생략
			default:
				// 생략
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
	return 0;
}
```
