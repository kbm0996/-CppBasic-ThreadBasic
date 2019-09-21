# 기초연습-스레드 동기화
## 📢 개요
 멀티스레드가 필요한 상황을 가정하여 스레드 프로그래밍 연습
  
## 📑 구성
  자세한 내용은 하위 디렉토리 참조

### 💻 [Thread_Basic](https://github.com/kbm0996/-CppBasic-ThreadBasic/blob/master/Thread_Basic/1%20Thread_Basic/main.cpp)
 g_Connect를 모니터링하는 `메인스레드`, int형 데이터 g_Connect를 증가시키는 `AcceptThread`, g_Connect를 감소시키는 `DisconnectThread`, g_Data를 증가시키고 1000 단위로 출력하는 `n개의 UpdateThread`. **CriticalSection**을 이용한 동기화 시험

  
### 💻 [Thread_list](https://github.com/kbm0996/-CppBasic-ThreadBasic/tree/master/Thread_list)
 SaveThread를 이벤트 방식으로 깨우고 모든 스레드들의 종료 시점을 지정하는 `메인스레드`, 리스트를 순회하며 출력하는 `PrintThread`, 리스트에서 데이터를 뽑는(pop) `PopThread`, 리스트에 데이터를 집어넣는(push) `n개의 PushThread`, 리스트를 파일에 쓰는 `SaveThread`. **리스트 동기화** 시험


### 💻 [Thread_queue](https://github.com/kbm0996/-CppBasic-ThreadBasic/tree/master/Thread_queue)
 큐(Queue)에 헤더와 페이로드로 구성된 메세지 구조체를 WorkerThread에 전달하고 Event로 깨워 일을 시키는 `메인스레드`, 평소에는 대기 상태에 있다가 메인스레드가 전달한 메세지에 따라 그때그때 깨어나 일을 하는 `WorkerThread`. 멀티스레드 환경에서 **자작 링버퍼 동기화 시험**, **SRWLock 시험**
 
  
### 💻 [Thread_DBSave](https://github.com/kbm0996/-CppBasic-ThreadBasic/tree/master/Thread_DBSave)
 1초마다 DB Write TPS와 DB Queue Size를 출력해주는 `메인스레드`, 평소에는 대기 상태에 있다가 외부에서 DB 입력에 대한 메세지를 받으면 쿼리를 생성하여 DB에 입력하는 `DBSaveThread`, 임의의 메세지를 생성하여 DBSaveThread에 전송하는 `n개의 UpdateThread`. 멀티스레드 환경에서의 **DB 연동 시험**
