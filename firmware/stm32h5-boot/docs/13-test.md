# 13. 테스트

두 계층으로 나뉜다. 하나는 하드웨어 없이 돌고, 하나는 실기가 필요하다.

## 계층 1 — 호스트 유닛 테스트 (하드웨어 불필요)

```bash
cd firmware/stm32h5-boot/test/host && ./run.sh
```

`boot.c` / `boot_log.c` 는 `flash.h` 인터페이스에만 의존하므로 목 플래시로 바꿔
끼워 네이티브 gcc 로 그대로 돌린다. ASan/UBSan 을 켠다.

```
test/host/
├── run.sh                  cmake + 실행
├── mock/
│   ├── mock_flash.c        STM32H5 제약을 강제하는 목
│   ├── mock_env.c          logPrintf/millis/reset 스텁
│   └── *.h                 실제 헤더를 가리는 shim (include 경로에서 앞선다)
├── test_slot_logic.c       슬롯 선택 진리표
├── test_apply.c            적용/무효화/훼손 슬롯
├── test_bootlog.c          로그 append/랩/핑퐁
└── test_powerloss.c        ★ 모든 중단 지점 전수 시뮬레이션
```

목의 존재 이유는 동작 흉내가 아니라 **제약 강제**다. 위반이 하나라도 있으면 실패한다.
자세한 내용은 `09b-power-loss.md`.

현재 **44 passed**.

## 계층 2 — 타깃 통합 테스트 (실기 필요)

```bash
cd firmware/stm32h5-boot/test/target
python3 -m pytest                    # 전체
python3 -m pytest test_01_flash.py   # 일부
```

의존: `pytest`, `pyserial`, `hidapi` (`requirements.txt`)

`conftest.py` 가 픽스처로 감싼다.

| 픽스처 | 내용 |
|---|---|
| `cli` | ST-LINK VCP 의 CLI |
| `boot` | `reset boot` 로 부트 모드 진입까지 보장 |
| `hid` | 부트로더 HID cmd 채널 |
| `app_uf2` / `app_bin` | 앱 빌드 산출물 |

| 파일 | 내용 |
|---|---|
| `test_01_flash.py` | 보호/정렬/**쿼드워드 재기록** 가드, 뱅크1·2 소거·기록 |
| `test_02_reset.py` | 소프트 리셋 미집계, 폴트 카운터(`md` 로 하드폴트 유발) |
| `test_03_boot.py` | 슬롯 판정 일관성, 부트 로그 |
| `test_04_cmd.py` | CDC 와 HID 가 같은 값을 돌려주는지 |
| `test_05_update.py` | UF2 드롭 갱신, HID 다운로드 |

현재 `test_01` + `test_03` **11 passed** (2분 9초).

### 물리 버튼은 자동화할 수 없다

ST-LINK 는 `mode=UR` 이든 `-rst` 든 **항상 소프트 리셋을 동반**해서 `PIN + SOFT` 가
같이 선다. 순수 NRST 를 재현할 수 없으므로 더블클릭은 손으로 확인한다.

```bash
python3 test/target/monitor.py 20
```

배너 개수와 `reset_count` 개수를 따로 센다. `reset_count` 는 300ms 대기 **뒤에**
출력되므로 대기 중 리셋된 부팅은 배너만 보인다.

| 케이스 | 기대 |
|---|---|
| 단발 | 배너 1회, `reset_count : 1`, `bits=['RESET_BIT_PIN']` |
| 더블클릭 | 배너 2회, `reset_count : 2` |

## 유용한 수동 도구

```bash
# 부트 로그 모니터
python3 test/target/monitor.py 30

# 앱 .bin 에 TAG 를 붙여 SWD 로 바로 구울 이미지 생성
python3 test/target/mkimage.py <app.bin> <out.bin> [seq]

# cmd 프로토콜 다운로드
python3 ../stm32h5-fw/tools/download/download.py [--hid] [--no-jump]
```
