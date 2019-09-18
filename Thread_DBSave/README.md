# 유니티xPHP 연동
## 📑 구성

**📋 _main** : 메인 함수

**📋 _DBDefine** : DB 접속 옵션, 메세지 헤더 정보

#### 📂 CBuffer : 각종 버퍼
> **📋 CLFMemoryPool** : 락프리(Lock-free) 메모리풀 클래스
>
> **📋 CLFMemoryPool_TLS** : TLS버전 락프리 메모리풀 클래스
>
> **📋 CRingBuffer** : 링버퍼 클래스
>
> **📋 CLFStack** : 락프리 스택 클래스

#### 📂 CDB : DB 관련
> **📋 CallHttp** : HTTP 송수신을 위한 함수, WCHAR과 CHAR 상호 변환 함수
>
> **📋 CDBConnector** : DB 연결, 쿼리 요청, 쿼리 결과 등 MySQL 클래스
>
> **📋 CDBConnector_TLS** : TLS버전 MySQL 클래스

#### 📂 CSystem : 로그, 미니덤프 관련
> 📋 CSystemLog, 📋 APIHook, 📋 CrashDump

## NetWWW.cs
### ⚙ 데이터 클래스

```c#
    /*********************************************************
     * 데이터 클래스 인터페이스
    **********************************************************/
    public interface IWebData
    {
        void Recv(JsonData JsonObject); // 받은 데이터 처리
        string URL();
    }
    
    /*********************************************************
     * 데이터 클래스
     *  
     * 용도별로 작성
     * (Login, Register, UpdateSession, StageClear, UserInfo)
    **********************************************************/
    public class WebLogin : IWebData
    {
        public string id { get; set; }
        public string password { get; set; }

        public string URL() { return "Login.php"; }

        public void Recv(JsonData JsonObject)
        {
            NetWWW.INSTANCE().MessageBox("Login Success");
            Debug.Log("Login Success");

            // 받은 session키, accountno 저장
            NetWWW.INSTANCE().session = JsonObject["session"].ToString();
            NetWWW.INSTANCE().accountno = Convert.ToInt32(JsonObject["accountno"].ToString());
        }
    }
    
    ...
```

### 🔄 코루틴
```C#
    IEnumerator SendURL(IWebData SendData)
    {
        Debug.Log("REQUEST : " + (PHP_URL + SendData.URL()));

        //-----------------------------------------------------
        // SendData 클래스를 JSON 형식으로 변환
        //-----------------------------------------------------
        UTF8Encoding utf8 = new UTF8Encoding();   // 유니코드 문자의 UTF-8 인코딩

        // TODO: [LitJson] JsonMapper.ToJson(this); 지정한 객체를 JSON 문자열로 변환하여 리턴
        string szJsonData = JsonMapper.ToJson(SendData);

        //-----------------------------------------------------
        // JSON 데이터 전송 및 결과 컨텐츠 수신
        //-----------------------------------------------------
        // WWW 클래스 인자로 문자열 전달 불가능(포인터도, 메모리도 접근 불가능)
        // 따라서, String을 Byte로 Convert
        byte[] bytes = utf8.GetBytes(szJsonData);

        // TODO: [Unity Script] WWW 클래스; URL에 메세지를 보내고, 컨텐츠를 받아오는 유틸리티 모듈
        // * 요약 : 지정한 URL에 데이터를 POST 형식으로 Request를 보내고 그 컨텐츠를 받아옴. 
        //         JSON으로 보낸 메시지이므로 웹페이지에서 JSON으로 받는 경우에만 작동
        // * 반환값 : 새로운 WWW 오브젝트(복사가 일어남). 컨텐츠 다운로드 완료시, 
        //           생성된 오브젝트로부터 그 결과를 가져올 수 있다(fetch)
        // * 문제점 : 웹 요청에 대한 반응이 바로 오지 않아서 일시적으로 유니티가 멈춤
        //           = WWW를 OnClick으로 구현하면 안되는 이유
        //          -> 코루틴으로 빠져서 웹 처리를 하도록 해야함
        WWW www = new WWW(PHP_URL + SendData.URL(), bytes);

        // * yield return : 현 상태 저장 후 리턴
        // * yield break : Iteration 루프 탈출
        yield return www;   // 답이 오지 않으면 계속 돌면서 답이 왔는지 검사
        
        //-----------------------------------------------------
        // 응답 처리
        //-----------------------------------------------------
        Response(SendData, (www.error != null));    // www.error이 null이면 true, 아니면 false
        if (www.error == null)
        {
            //-----------------------------------------------------
            // Json 데이터 파싱
            //-----------------------------------------------------
            /* !!주의!! php측에서 UTF8 + BOM 코드로 인코딩된 다른 php를 include할 경우 에러 발생 */
            JsonData JsonResponse = JsonMapper.ToObject(www.text);

            //-----------------------------------------------------
            // 기본적으로 ResultCode / ResultMsg가 있으므로 확인
            //
            //-----------------------------------------------------
            int ResultCode = Convert.ToInt32(JsonResponse["ResultCode"].ToString());
            string ResultMsg = JsonResponse["ResultMsg"].ToString();

            //-----------------------------------------------------
            // 각각의 WebData 내 Recv()에서 데이터 처리
            //
            //-----------------------------------------------------
            if (ResultCode == 1)
            {
                Debug.Log("RESPONSE : url:" + SendData.URL() + " | Contents:" + www.text);
                SendData.Recv(JsonResponse);
            }
            else
            {
                // 웹페이지 측에서 요청 처리 실패 
                MessageBox(ResultMsg);
            }
        }
        else
        {
            // 전송 에러
            Debug.Log("REQUEST FAILED : url:" + SendData.URL() + " | error:" + www.error);
            MessageBox(www.error);
        }
    }
```
