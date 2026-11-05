# QuickTouch에 A31G336 DAPLink 연결하기 (1차)

목표: QuickTouch의 기존 기능(Connect, Channel Set, Function Test, Tuning)을 **E-OCD 대신 A31G336 DAPLink**로 동작시키는 것.
E-OCD 경로는 그대로 남기고, `TouchTransportFactory.UseDapLink` 하나로 전환한다.

검증 상태 (2026-11-03, A96T418 보드, `DapLinkTransportTest.exe`)
- 버전 읽기, Channel Set(CS0..CS9, 기본 파라미터), Function Test 시작, DIFF 스트리밍: 동작 확인
- QuickTouch 프로젝트 자체 빌드는 이 PC에 VS/.NET SDK가 없어 미확인 (추가 파일 3개는 C# 5로 컴파일 확인)

## 1. 파일 추가 (그대로 복사)

| 이 폴더 | QuickTouch |
|---|---|
| `QuickTouch/I2C/ITouchTransport.cs` | `I2C/ITouchTransport.cs` |
| `QuickTouch/I2C/DapLinkTransport.cs` | `I2C/DapLinkTransport.cs` |
| `QuickTouch/I2C/TouchTransportFactory.cs` | `I2C/TouchTransportFactory.cs` |

`QuickTouch.csproj` (Compile 목록에 명시해야 빌드됨, 169~170행 근처):

```xml
    <Compile Include="I2C\I2CTransport.cs" />
    <Compile Include="I2C\I2CConfig.cs" />
    <Compile Include="I2C\ITouchTransport.cs" />
    <Compile Include="I2C\DapLinkTransport.cs" />
    <Compile Include="I2C\TouchTransportFactory.cs" />
```

## 2. `I2C/I2CTransport.cs` — E-OCD 전송을 인터페이스에 맞춤

```csharp
    public class I2CTransport : ITouchTransport            // 변경
    {
        private readonly HIDController _hid;
        private byte I2C_SLAVE_ID = 0;

        public event EventHandler Removed;                  // 추가

        public I2CTransport(HIDController hidController, byte slaveId)
        {
            ...
            _hid = hidController;
            _hid.HidRemoved += (s, e) => Removed?.Invoke(this, EventArgs.Empty);   // 추가
            I2C_SLAVE_ID = slaveId;
        }

        public void Close()                                 // 추가
        {
            _hid?.Close();
        }

        public bool IsOpen()                                // 추가
        {
            return _hid != null && _hid.IsOpen();
        }
```

Open/Write/Read/PowerSet은 시그니처가 이미 같으므로 그대로 둔다.

## 3. 프로토콜 3개 — `_hid` 대신 `ITouchTransport`

`TouchProtocol/A96T418/Protocol/TouchProtocolA96T418.cs` (A96T430, A96T448도 동일, 슬레이브 ID 변수명만 다름)

```csharp
        // 삭제: private HIDController _hid;
        private ITouchTransport _i2c;                        // I2CTransport -> ITouchTransport

        public TouchProtocolA96T418()
        {
            _i2c = TouchTransportFactory.Create(A96T418_SLAVE_ID);   // 0x40 그대로, DAPLink는 내부에서 0x20
            _i2c.Removed += HandleHidDeviceRemoved;
        }
        ...
        public void Disconnect()
        {
            _i2c.Close();                                    // _hid.Close() -> _i2c.Close()
        }
```

`_hid`를 쓰는 곳은 위 3곳(필드, 생성자, Disconnect)뿐이다. `_i2c.Read/Write/PowerSet/Open` 호출(3개 파일 합계 228곳)은 수정 없음.

## 4. 폼 3개 — VID/PID

`Forms/A96T418/FormA96T418.cs` 26~27행 (A96T430, A96T448도 동일)

```csharp
        int vendorId = TouchTransportFactory.VendorId;      // E-OCD 0x1A29 / DAPLink 0x1A29
        int productId = TouchTransportFactory.ProductId;    // E-OCD 0xAA06 / DAPLink 0xA809
```

## 5. 전환

`TouchTransportFactory.UseDapLink` (기본 `true`). E-OCD로 돌아가려면 `false`.
1차는 상수로 두고, 필요하면 나중에 메뉴/설정으로 뺀다. SCL은 `DapLinkClockHz` (기본 100 kHz, A96T418은 400 kHz에서 응답 없음).

## DAPLink 사용 시 동작 차이 (코드 수정 불필요)

| 항목 | E-OCD | DAPLink |
|---|---|---|
| PowerSet (Connect, Tuning) | VDD off/on | 전원 스위치 없음: 로그만 남기고 성공 반환. Connect의 500 ms 대기는 그대로 |
| 읽기 실패 | slave NACK 시 응답 없음 → `Read()` 무한 대기 | 항상 status 응답 → 즉시 `false` |
| 동시 호출 | 보호 없음 | 명령 단위 lock |
| 장치 분리 | HidSharp DeviceList | USB 전송이 "장치 없음"으로 실패할 때 `Removed` |
| 레지스터 읽기 | write reg, STOP, START, read | 동일 (터치 IC가 repeated START에 응답하지 않음) |
| 디버거와 동시 사용 | - | 가능 (아래) |

### Keil/OpenOCD와 동시 사용 (DAPLink F/W bcdDevice 0x0101 이상)

DAPLink에 I2C 전용 USB 인터페이스 "A-Link I2C"(WinUSB, EP5, GUID `{6E0C9B7A-3F42-4D1E-A5B8-91C7D2E4F036}`)가 있다.
`DapLinkTransport`는 이 인터페이스를 먼저 찾고, 없으면(이전 F/W) CMSIS-DAP v2 인터페이스를 쓴다.

| F/W | QuickTouch가 여는 인터페이스 | Keil/OpenOCD 동시 사용 | 로그 |
|---|---|---|---|
| I2C 인터페이스 있음 | A-Link I2C | 가능 | `[opened] A-Link I2C interface, I2C 99585 Hz` |
| 이전 F/W | CMSIS-DAP v2 | 불가 (먼저 연 쪽만 사용) | `[opened] A-Link CMSIS-DAP (CMSIS-DAP interface: ...)` (F/W 2.1.1부터 `A-Link-MINI-V1 CMSIS-DAP`) |

- I2C 명령은 디버거 명령 사이에 처리된다 (명령 하나당 수 ms 이하).
- I2C 인터페이스 패킷은 128 B: 한 번에 읽기 최대 124 B, 쓰기 최대 122 B (QuickTouch 최대 약 64 B).
- 새 F/W를 처음 꽂으면 Windows가 장치를 다시 설치한다 (bcdDevice 변경). 장치 관리자에 "A-Link I2C"가 별도 장치로 보인다.

## 2차 (이번 범위 아님)

1. 그래프: Function Test 루프의 THD/DIFF/KEY 3회 읽기를 `DapLinkTransport.StreamStart/StreamRead` (주기 또는 INT 트리거)로 교체. 터치 IC 데이터 갱신은 기본 파라미터 10채널에서 약 20 ms(≈50 Hz).
2. 1차 리뷰 항목: UI 크로스 스레드(H4), 동기 `Invoke`(M7), CSV 로그 경합(M6), E-OCD `Read()` 타임아웃(H1).
3. ~~DAPLink F/W: 스트림 전송 중 가끔 발생하는 TIMEOUT~~ 수정됨 (2026-11-05): I2C2가 STOP 후 STOPD를 가끔(약 1/300) 세트하지 않음 → ISR에서 BUSY 해제를 폴링.
   10 ms x 10 s: errors 0, missed 0, interval 9.99~10.01 ms.
