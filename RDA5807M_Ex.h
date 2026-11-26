// RDA5807M_Ex.h
// Extends the stock mathertel Radio library (v1.2.0) RDA5807M driver so the
// RDS callback also receives the chip's RDS status bits:
//   rds_ready  - Reg 0x0A bit 15 (RDSR), and the block data changed since last call
//                (also true on clearRDS_Ex(), which sends all-zero blocks)
//   rds_sync   - Reg 0x0A bit 12 (RDSS), 1 = RDS decoder synchronized
//   blockA_err - Reg 0x0B bits 3:2 (BLERA), 0 = 0 errors, 1 = 1-2, 2 = 3-5, 3 = 6+ / uncorrectable
//   blockB_err - Reg 0x0B bits 1:0 (BLERB), same scale
// The callback runs on new data and whenever sync or the error levels change,
// so the sketch still sees sync loss without being called every loop pass.
// Works with the unmodified Radio library; nothing in Radio-master.zip changes.

#ifndef RDA5807M_EX_H
#define RDA5807M_EX_H

#include <Arduino.h>
#include <Wire.h>
#include <RDA5807M.h>

extern "C" {
  typedef void (*receiveRDSFunction_Ex)(bool rds_ready, bool rds_sync,
                                        uint8_t blockA_err, uint8_t blockB_err,
                                        uint16_t block1, uint16_t block2,
                                        uint16_t block3, uint16_t block4);
}

class RDA5807M_Ex : public RDA5807M {
public:
  RDA5807M_Ex() : _sendRDS_Ex(nullptr) {}

  // Register the extended RDS processor.
  void attachReceiveRDS_Ex(receiveRDSFunction_Ex newFunction) {
    _sendRDS_Ex = newFunction;
  }

  // Tell the extended processor that RDS data is gone (e.g. after tuning).
  void clearRDS_Ex() {
    for (uint8_t i = 0; i < 4; i++) _lastBlock[i] = 0;
    _lastSync = false;
    _lastErrA = 0;
    _lastErrB = 0;
    // rds_ready=true so the sketch forwards the zero blocks: processData(0,...)
    // resets RDSParser, and gui_PI(0) / gui_PTY(0) clear the display.
    if (_sendRDS_Ex) _sendRDS_Ex(true, false, 0, 0, 0, 0, 0, 0);
  }

  // Reads status regs 0x0A-0x0B, then RDS blocks 0x0C-0x0F only when a group is ready.
  // Falls back to the stock behaviour when only attachReceiveRDS() is used.
  void checkRDS() {
    if (!_sendRDS_Ex) {
      RDA5807M::checkRDS();
      return;
    }

    // Status: sequential read starts at register 0x0A; 2 registers = 4 bytes.
    if (Wire.requestFrom(I2C_SEQ_EX, (uint8_t)4) != 4) {
      _noAnswer();
      return;
    }
    uint16_t regA = _read16_Ex();
    uint16_t regB = _read16_Ex();

    // Block E (RBDS/MMBS only) is not a normal group: skip it, as the original did.
    if (regA & 0x0800) return;

    bool rdsr = regA & 0x8000;
    bool rdss = regA & 0x1000;
    uint8_t blera = (regB >> 2) & 0x03;
    uint8_t blerb = regB & 0x03;

    // Only remember a group the sketch will accept (block A/B error level
    // below RDS_MAX_BLER_EX), so a clean repeat of a rejected group still
    // counts as new data.
    bool changed = false;
    if (rdsr) {
      // Read the blocks (0x0C-0x0F) only when a group is ready, via indexed
      // access with a repeated start, so a check never reads (and possibly
      // acknowledges) a group it has not seen flagged as ready.
      Wire.beginTransmission(I2C_INDX_EX);
      Wire.write(0x0C);
      Wire.endTransmission(false);
      if (Wire.requestFrom(I2C_INDX_EX, (uint8_t)8, (uint8_t)1) != 8) {
        _noAnswer();
        return;
      }
      uint16_t block[4];
      for (uint8_t i = 0; i < 4; i++) block[i] = _read16_Ex();

      if (blera < RDS_MAX_BLER_EX && blerb < RDS_MAX_BLER_EX) {
        for (uint8_t i = 0; i < 4; i++) {
          if (block[i] != _lastBlock[i]) { _lastBlock[i] = block[i]; changed = true; }
        }
      }
    }

    _report(changed, rdss, blera, blerb);
  }

private:
  static const uint8_t I2C_SEQ_EX = 0x10;  // RDA5807M sequential-access address
  static const uint8_t I2C_INDX_EX = 0x11; // RDA5807M indexed-access address
  static const uint8_t RDS_MAX_BLER_EX = 2; // BLER 2 (3-5 errors) and up is rejected
  receiveRDSFunction_Ex _sendRDS_Ex;
  uint16_t _lastBlock[4] = {0, 0, 0, 0};
  bool    _lastSync = false;
  uint8_t _lastErrA = 0;
  uint8_t _lastErrB = 0;

  // Call back only on new data or a change in sync / error levels, so the
  // sketch is not redrawing the display on every loop pass.
  void _report(bool newData, bool sync, uint8_t errA, uint8_t errB) {
    if (!newData && sync == _lastSync && errA == _lastErrA && errB == _lastErrB) return;
    _lastSync = sync;
    _lastErrA = errA;
    _lastErrB = errB;
    _sendRDS_Ex(newData, sync, errA, errB,
                _lastBlock[0], _lastBlock[1], _lastBlock[2], _lastBlock[3]);
  }

  // Chip did not answer: drop any partial bytes and report "no RDS sync".
  void _noAnswer() {
    while (Wire.available()) Wire.read();
    _report(false, false, 0, 0);
  }

  static uint16_t _read16_Ex() {
    uint8_t hi = Wire.read();
    uint8_t lo = Wire.read();
    return (uint16_t)((hi << 8) | lo);
  }
};

#endif
