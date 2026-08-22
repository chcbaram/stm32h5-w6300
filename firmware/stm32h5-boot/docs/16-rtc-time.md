# 16. 보드 시각 (RTC epoch)

## 목적

부트 이벤트 로그에 **언제** 일어난 일인지 남긴다. 그리고 웹에서 보드 시각을
보고 PC 와 맞출 수 있게 한다.

`boot_log_t` 에는 처음부터 `timestamp` 자리가 있었지만 0 만 채우고 있었다.
이번에 실제로 채운다.

## 대상 파일

```
src/common/core/util_core.{c,h}          달력 <-> epoch 계산 (호스트 테스트도 이걸 쓴다)
src/common/hw/include/rtc.h              rtcGetEpoch / rtcSetEpoch / rtcEpochToInfo
src/hw/driver/rtc.c                      위 세 함수 구현
src/ap/modules/boot/boot_log.c           기록 시각 채우기
src/ap/modules/boot/boot.c               `boot log` CLI 에 TIME 열
src/ap/modules/cmd/process/cmd_boot.c    BOOT_CMD_RTC (0x0012)
web/boot.js, web/panels/board.js         보드 시각 표시 · PC 동기화 · 1초 자동 갱신
```

## 설계 결정과 근거

### 왜 epoch 한 개인가

`boot_log_t` 의 `timestamp` 가 이미 `uint32_t` 한 칸이다. 달력 필드로 바꾸면
레코드 포맷이 바뀌고, 32바이트 쿼드워드 정렬도 다시 봐야 한다. epoch 이면
그 칸에 그대로 들어간다.

호스트에서도 `Date.now()/1000` 한 줄이다.

### 지역시를 어떻게 다루나 ← 헷갈리기 쉬운 곳

보드 RTC 는 **지역시**를 담는다. SNTP 가 KST 로 맞추기 때문이다
(`SNTP_init(..., 40, ...)`).

그래서 약속을 이렇게 정했다.

> **달력 필드를 UTC 로 간주해 만든 epoch** 을 주고받는다.

- 펌웨어 : `rtcGetEpoch()` 이 RTC 의 Y/M/D h:m:s 를 그대로 UTC 로 보고 변환한다
- 호스트 : 표시할 때 `getUTC*()` 를 쓰고, 보낼 때 자기 지역시를 같은 방식으로
  인코딩한다 (`Date.now() - getTimezoneOffset()*60000`)

이러면 화면에 보이는 값이 **보드의 벽시계와 정확히 일치**한다. 타임존 변환을
어느 쪽에서도 하지 않으므로 어긋날 여지가 없다. 진짜 UTC 가 필요해지면 그때
타임존을 별도 필드로 넘기면 된다.

**대신 지역시 접근자를 쓰면 그만큼 밀린다.** 부트 이벤트 표에서 `getHours()` 로
찍었다가 KST 만큼(9시간) 앞선 값이 나왔다. 화면에서 바로 티가 나서 다행이었지
조용히 틀렸으면 오래 갔을 오류다. 시각을 찍는 곳은 전부 `epochToText()` 를
거치게 하고, 다른 곳에 남아 있지 않은지 `getFullYear|getMonth|getDate|getHours|
getMinutes|getSeconds` 로 훑어 확인했다.

### 시각을 모르는 경우

보드에 코인셀이 없다(VBAT = VDD). **전원을 뽑으면 RTC 가 초기화된다.**
리셋에는 살아남는다.

맞춘 적 없는 RTC 는 리셋 기본값(2000년대 초)에 머물기 때문에, "0년"과
"진짜 2001년"을 구분할 수 없다. 그래서 `RTC_EPOCH_YEAR_MIN (2024)` 보다 이르면
**모르는 것으로 본다.**

- `rtcGetEpoch()` → 0
- 부트 로그 → `timestamp = 0`
- CLI / 웹 → `-` 로 표시

앱이 SNTP 나 웹의 "PC 시간과 맞추기" 로 한 번 맞춰두면, 그 뒤의 롤백/폴트 복구
기록에는 제대로 된 시각이 남는다. 그게 정작 알고 싶은 경우다.

### 계산을 util_core.c 에 둔 이유

`utilEpochFromCivil()` / `utilCivilFromEpoch()` 는 Howard Hinnant 의
days_from_civil / civil_from_days 다. 윤년·윤일이 분기 없이 끝나서 짧고 틀릴
여지가 적다.

`rtc.c` 에 두면 HAL 의존 때문에 호스트 유닛 테스트가 컴파일할 수 없다.
`util_core.c` 는 이미 호스트 빌드에 들어가 있어서, **테스트가 실물과 같은
구현을 시험한다.** 목은 RTC 읽기만 흉내낸다.

## 커맨드

`BOOT_CMD_RTC (0x0012)` — convex 의 `CMD_RTC` 와 같은 형태다.

```
요청 : [0] op   (0 = GET, 1 = SET)
       SET 이면 [1..4] epoch (uint32 LE)
응답 : [0..3] 현재 epoch (0 = 시각 모름)
```

**읽기든 쓰기든 현재 값을 되돌려준다.** 쓰기 직후 확인이 한 번에 끝나서,
호스트는 GET/SET 응답을 같은 코드로 처리한다.

## 웹 UI

보드 정보 탭에 넣었다.

- 보드 시각 / PC 시각 / 차이 (일치하면 초록, 아니면 빨강)
- `PC 시간과 맞추기` 버튼
- `1초마다 갱신` 체크박스 (기본 켜짐)

자동 갱신 타이머는 패널에 unmount 훅이 없어서 **스스로 물러난다.**
`isActive(id)` 가 거짓이거나 채널이 없으면 타이머를 끈다. 응답을 기다리는 중이면
그 회차를 건너뛴다 — 겹쳐 보내면 채널이 밀린다.

## 검증

### 호스트 유닛 (`test_bootlog.c`)

| 항목 | 기대 |
|---|---|
| RTC 미설정 | `timestamp == 0` |
| 2001년(리셋 기본값) | `timestamp == 0` |
| 2026-08-22 18:07:57 | `timestamp == 1787422077` |

epoch 기대값은 Python 으로 교차 확인했다. 처음에 손으로 계산한 값이 4일
틀렸는데, 펌웨어가 맞고 내 계산이 틀렸다.

### 실기

```
RTC GET  ->  2026-08-22 18:21:59      (리셋 뒤에도 살아있다)
RTC SET  ->  2026-08-22 18:22:21      PC 와 차이 0초
2초 뒤 GET  경과 2초                   (RTC 가 실제로 돈다)

boot log : 36 records (max 512)
 35   36  2026-08-22 18:21:45 UPDATE  0  0xE277  0xA92F  rst=0xA
```

## 부트 이벤트 로그 용량

8KB 섹터 하나에 32바이트 레코드 **256개**, 섹터 2개 핑퐁이라 **최대 512개**를
조회한다. 한쪽이 차면 반대쪽을 지우고 이어 쓰므로 가장 오래된 256개가 한 번에
사라진다. 항상 최소 한 세대는 온전히 남는다.

웹에서는 행이 많아 `.tbl-scroll` 로 높이를 320px 로 묶고 머리글을 sticky 로
붙였다. 열면 최신 기록이 보이도록 끝으로 스크롤한다.
