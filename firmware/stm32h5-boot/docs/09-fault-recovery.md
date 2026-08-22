# 09. 폴트 자동 복구와 부트 이벤트 로그

## 목적

새 펌웨어가 **부팅 후 죽는** 경우까지 잡는다. CRC 검증만으로는 이미지가 온전한지만
알 뿐, 실행했을 때 하드폴트가 나는지는 알 수 없다.

그리고 복구가 일어났다는 사실을 **전원을 뽑아도 남게** 기록해 나중에 CLI 로 확인한다.

## 대상 파일

- `src/ap/ap.c` (`bootUp()`)
- `src/hw/driver/reset.c` (카운터), `src/hw/driver/fault.c`
- `src/ap/modules/boot/{boot_log.c, boot_log.h}`

## bootUp() 판정 순서

```
(0) 지난 부팅의 ECC 2비트 오류 영역 정리
(1) MODE_BIT_BOOT        앱이 resetToBoot() 호출
(2) 리셋 더블클릭
(3) 폴트 반복       -> 자동 복구   BOOT_EVT_FAULT_RECOVER
(4) 부팅 미확인 반복 -> 롤백       BOOT_EVT_ROLLBACK
(5) MODE_BIT_UPDATE      슬롯 적용
(6) 점프 (직전에 boot_try++)
```

(3)(4)는 같은 경로를 쓴다.

```c
slot = bootGetRollbackSlot();
if (slot >= 0) {
  bootApplySlot(slot);
  bootLogWrite(evt, slot, firm.fw_crc, info.fw_crc, faultGetPc());
  bootInvalidateSlot(실패한 이미지가 든 슬롯);
} else {
  // 되돌아갈 곳이 없다. 엉뚱한 버전으로 되돌리는 것보다 UF2 모드가 낫다.
  run_fw = false;
}
```

## 두 개의 카운터

| 카운터 | 저장 | 증가 시점 | 초기화 |
|---|---|---|---|
| `boot_try` | `HW_RTC_BOOT_TRY` | 부트로더가 앱으로 **점프하기 직전** | 앱의 `resetConfirmBoot()` |
| `fault_cnt` | `HW_RTC_FAULT_CNT` | `faultReset()` 이 리셋 직전 | 앱의 `resetConfirmBoot()` |

**`.noinit`(SRAM)이 아니라 RTC 백업 레지스터에 둔다.** 기존 `fault.c` 의
`fault_log_t` 는 SRAM 이라 전원을 뽑으면 사라진다. `HAL_RTCEx_BKUPWrite` 는 사실상
`TAMP->BKPxR` 직접 쓰기라 **HardFault 컨텍스트에서도 안전**하다.

`boot_try` 가 안전망이고 `fault_cnt` 는 폴트를 더 빨리 잡는 보조다. 실제로 폴트
펌웨어를 넣었을 때 `BOOT TRY x3` 이 먼저 걸렸다(당시 앱에 카운터가 없었다).

### confirm 시점이 중요하다

`resetConfirmBoot()` 을 `hwInit()` 직후에 부르면 "부팅 5초 뒤 항상 죽는" 펌웨어는
매번 confirm 에 성공해서 카운트가 쌓이지 않는다. `HW_BOOT_CONFIRM_MS`(기본 10초)
만큼 정상 동작한 뒤에 확정한다.

반대로 너무 길면 confirm 전에 전원이 끊길 때 오탐 롤백이 늘어난다. 실기에서
조정할 값이다.

> **앱 수정이 선택이 아니다.** 앱이 `resetConfirmBoot()` 을 부르지 않으면 `boot_try`
> 가 계속 쌓여 **3회 부팅마다 오탐 롤백**이 난다. 실제로 `boot try : 3` 까지
> 올라간 것을 확인하고 12단계를 앞당겨 처리했다.

## 부트 이벤트 로그

`FLASH_ADDR_BOOT_LOG`(0x081E0000), 8KB 섹터 2개 핑퐁. 32바이트 레코드 × 256/섹터.

### 기록 시점 — 매 부팅이 아니다

정상 부팅은 기록하지 않는다. 상태 변화만 남긴다.

```
BOOT_EVT_UPDATE          슬롯 -> FIRM 정상 적용
BOOT_EVT_ROLLBACK        boot_try 초과
BOOT_EVT_FAULT_RECOVER   fault_cnt 초과
BOOT_EVT_VERIFY_FAIL     검증/적용 실패
BOOT_EVT_ECC_CLEAN       ECC 2비트 오류 영역 정리
```

"펌웨어 업데이트 1회 = 1레코드" 이므로 하루 50번 업데이트해도 한 섹터(256개)를
채우는 데 5일이다. 섹터 endurance 10K cycles 대비 여유가 압도적이다.

초안에 있던 `UF2_ENTER` 는 **제외했다.** 개발 중 더블클릭으로 수시로 들어가는데
그때마다 쌓이면 금방 한 바퀴 돈다.

### 왜 핑퐁인가

단일 섹터 순환은 두 가지가 위험하다.

1. 섹터가 찰 때마다 소거 → **기존 256개 레코드가 통째로 사라진다.** 복구 이력을
   보려는 게 목적인데 주기적으로 전부 날리는 셈이다.
2. 소거 도중(~4ms) 전원이 끊기면 반쯤 지워진 섹터가 남고 **ECC 2비트 오류**가 난다.
   로그를 쓰는 시점은 대부분 이미 뭔가 잘못된 상황이라 전원이 불안정할 개연성도 크다.

컴팩션 없이 두 섹터를 번갈아 쓴다. A 가 차면 B 를 지우고 B 에 append 하며 A 는
이전 세대로 보존된다. 최대 512개를 조회할 수 있고 컴팩션 로직이 없다.

### 레코드 기록 순서

커밋 규칙을 따른다. ② 둘째 쿼드워드 → ① **magic 이 든 첫 쿼드워드를 마지막**.
중간에 끊기면 magic 이 없어 빈 자리로 보이고, 그 자리는 이미 프로그램되어 있으므로
다음 기록은 다음 자리로 건너뛴다.

> **호스트 전원손실 시험이 여기서 버그를 잡았다.** `bootLogScanSect()` 가 첫 무효
> 레코드에서 스캔을 멈췄다. 그러면 부분 기록으로 생긴 빈틈 **뒤의 정상 레코드가
> 영영 보이지 않는다.** 섹터 전체를 훑고 빈틈을 건너뛰도록 고쳤다.

## ECC 오류 영역 정리

`NMI_Handler` 는 위치만 백업 레지스터에 남기고 리셋한다(`03-flash-driver.md`).
실제 정리는 다음 부팅의 `bootUp()` (0)단계에서 한다.

```c
if (resetGetEccAddr(&ecc_addr)) {
  // 슬롯 안이면 그 슬롯을 무효화, 부트 로그 안이면 로그를 지운다
  resetClearEccAddr();
  bootLogWrite(BOOT_EVT_ECC_CLEAN, -1, ecc_addr, 0, 0);
}
```

## 검증

```bash
cd firmware/stm32h5-boot/test/target && python3 -m pytest test_02_reset.py test_03_boot.py
```

폴트는 `md` 로 없는 주소를 읽으면 낼 수 있다. 시험용 펌웨어를 빌드할 필요가 없다.

```
cli# md 0xFFFFFFF0 16
```

## 실측 결과

부팅 직후 하드폴트를 내는 펌웨어를 UF2 로 투입한 결과:

```
[  ] uf2 begin -> slot0 (553 blocks)
[  ] bootApplySlot(0) 138 KB
Booting..Ver : VFAULT-T1
Fault Message   Msg : HardFault        -> 리셋
Booting..Ver : VFAULT-T1
Fault Message   Msg : HardFault        -> 리셋
[!!] BOOT TRY x3 -> rollback
[  ] bootApplySlot(1) 138 KB
[OK] rollback -> slot1
Booting..Ver : V251024R1                <- 정상 펌웨어로 자동 복구
```

부트 로그:

```
cli# boot log
IDX  SEQ  EVENT          SLOT  FROM       TO      DETAIL
  0    1  ECC_CLEAN      -   0x8100000  0x0000  rst=0xA
  1    2  ROLLBACK       1   0x5128     0xC8E7  PC=0x08021CFC rst=0xA
```

폴트 카운터는 `md` 로 확인했다. `md 0xFFFFFFF0` → HardFault(PC=0x080046BC) → 리셋,
`fault count` 가 1 → 2 로 누적.

앱이 `resetConfirmBoot()` 을 부른 뒤에는 `boot try : 0 / fault count : 0` 으로
정상 유지된다.
