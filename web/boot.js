//-- 부트로더 커맨드 셋 (src/ap/modules/cmd/process/cmd_boot.c 와 일치)
//
import { str32 } from './proto.js';

export const BOOT_CMD = {
  INFO:      0x0000,
  VERSION:   0x0001,
  FW_BEGIN:  0x0002,
  FW_ERASE:  0x0003,
  FW_WRITE:  0x0004,
  FW_READ:   0x0005,
  FW_END:    0x0006,
  FW_VERIFY: 0x0007,
  FW_UPDATE: 0x0008,
  FW_JUMP:   0x0009,
  LOG_COUNT: 0x000A,
  LOG_READ:  0x000B,
  CLI:       0x0010,
  CLI_MORE:  0x0011,
  RTC:       0x0012,
};

export const ITEM_SIZE = 84;      // boot_ver_item_t

export const EVT_NAME = ['?', 'UPDATE', 'ROLLBACK', 'FAULT_RECOVER', 'VERIFY_FAIL', 'ECC_CLEAN'];

export const DEV_MODE_BOOT = 0;
export const DEV_MODE_APP  = 1;

export const RTC_OP_GET = 0;
export const RTC_OP_SET = 1;

//-- 보드 시각은 uint32 epoch 로 주고받는다.
//
//   보드 RTC 는 지역시(SNTP 가 KST 로 맞춘다)를 담고 있어서, 펌웨어는 달력
//   필드를 UTC 로 간주해 epoch 을 만든다. 그래서 표시할 때는 getUTC*() 를 쓰고,
//   보낼 때는 브라우저 지역시를 같은 방식으로 인코딩한다. 그러면 화면 값이
//   보드의 벽시계와 정확히 일치한다.
//
export function epochToText(epoch) {
  if (!epoch) return '-';
  const d = new Date(epoch * 1000);
  const p = (v) => String(v).padStart(2, '0');
  return `${d.getUTCFullYear()}-${p(d.getUTCMonth() + 1)}-${p(d.getUTCDate())} ` +
         `${p(d.getUTCHours())}:${p(d.getUTCMinutes())}:${p(d.getUTCSeconds())}`;
}

// 브라우저의 지역시를 보드와 같은 규칙으로 인코딩한다.
export function localEpoch() {
  return Math.floor((Date.now() - new Date().getTimezoneOffset() * 60000) / 1000);
}

//-- 연결 직후 가장 먼저 부른다.
//
//   부트로더와 앱이 같은 VID/PID 로 열거되므로 USB 만으로는 구분할 수 없다.
//   mode 로 어느 쪽인지 판별하고, 그에 따라 보여줄 패널을 정한다.
//
export function parseInfo(d) {
  if (d.length < 96) throw new Error(`INFO 응답이 짧다 (${d.length}B)`);
  const dv = new DataView(d.buffer, d.byteOffset);
  return {
    magic:    dv.getUint32(0,  true),
    mode:     dv.getUint32(4,  true),
    bootAddr: dv.getUint32(8,  true),
    firmAddr: dv.getUint32(12, true),
    firmSize: dv.getUint32(16, true),
    slotSize: dv.getUint32(20, true),
    slotMax:  dv.getUint32(24, true),
    familyId: dv.getUint32(28, true),
    name:     str32(d, 32),
    version:  str32(d, 64),
  };
}

function parseItem(d, off) {
  const dv = new DataView(d.buffer, d.byteOffset + off);
  return {
    valid: dv.getUint8(0) === 1,
    index: dv.getUint8(1),
    addr:  dv.getUint32(4,  true),
    seq:   dv.getUint32(8,  true),
    size:  dv.getUint32(12, true),
    crc:   dv.getUint32(16, true),
    name:  str32(d, off + 20),
    ver:   str32(d, off + 52),
  };
}

export function parseVersion(d, slotMax = 2) {
  if (d.length < ITEM_SIZE * (slotMax + 1) + 4)
    throw new Error(`VERSION 응답이 짧다 (${d.length}B)`);
  const slot = [];
  for (let i = 0; i < slotMax; i++) slot.push(parseItem(d, ITEM_SIZE * (i + 1)));
  const dv = new DataView(d.buffer, d.byteOffset + ITEM_SIZE * (slotMax + 1));
  return {
    firm: parseItem(d, 0),
    slot,
    writeSlot:    dv.getInt8(0),
    pendingSlot:  dv.getInt8(1),
    rollbackSlot: dv.getInt8(2),
  };
}

export function parseLog(d) {
  if (d.length < 32) throw new Error('log 레코드가 짧다');
  const dv = new DataView(d.buffer, d.byteOffset);
  return {
    seq:       dv.getUint32(4,  true),
    event:     dv.getUint8(8),
    slot:      dv.getUint8(9),
    resetBits: dv.getUint16(10, true),
    timestamp: dv.getUint32(12, true),   // RTC epoch. 모르면 0
    fromCrc:   dv.getUint32(16, true),
    toCrc:     dv.getUint32(20, true),
    faultPc:   dv.getUint32(24, true),
  };
}
