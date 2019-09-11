# 기초연습-스레드 동기화
## 📢 개요
 멀티스레드가 필요한 상황을 가정하여 스레드 프로그래밍 연습
  
## 📑 구성
  자세한 내용은 하위 디렉토리 참조

### 💻 [1 Thread_Basic](https://github.com/kbm0996/-CppBasic-ThreadBasic/tree/master/1%20Thread_Basic)
 int형 데이터 g_Connect를 증가시키는 `AcceptThread`, g_Connect를 감소시키는 `DisconnectThread`, g_Data를 증가시키고 1000 단위로 출력하는 `n개의 UpdateThread`, g_Connect를 모니터링하는 `메인스레드`. 가장 단순한 동기화 기법인 CriticalSection 실습 및 올바른 스레드 종료 실습

  
### 💻 [2 Thread_list](https://github.com/kbm0996/-CppBasic-ThreadBasic/tree/master/2%20Thread_list)
 리스트를 순회하며 출력하는 `PrintThread`, 리스트에서 데이터를 뽑는(pop) `PopThread`, 리스트에 데이터를 집어넣는(push) `n개의 PushThread`, 리스트를 파일에 쓰는 `SaveThread`. SaveThread를 키고 모든 스레드들의 종료 시점을 지정하는 `메인스레드`. 리스트 동기화 실습


### 💻 [2 Thread_queue](https://github.com/kbm0996/-CppBasic-ThreadBasic/tree/master/2%20Thread_queue)

  
### 💻 [3 Thread_DBSave](https://github.com/kbm0996/-CppBasic-ThreadBasic/tree/master/3%20Thread_DBSave)

  
