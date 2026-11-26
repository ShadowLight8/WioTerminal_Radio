// RDA5807M_Ex.h
// Extends the stock mathertel Radio library (v1.2.0) RDA5807M driver so the
// RDS callback also receives the chip's RDS status bits:
//   rds_ready  - Reg 0x0A bit 15 (RDSR), and the block data changed since last call
//   rds_sync   - Reg 0x0A bit 12 (RDSS), 1 = RDS decoder synchronized
//   blockA_err - Reg 0x0B bits 3:2 (BLERA), 0 = 0 errors, 1 = 1-2, 2 = 3-5, 3 = 6+ / uncorrectable
//   blockB_err - Reg 0x0B bits 1:0 (BLERB), same scale
// The callback runs on every checkRDS() so the sketch can show sync loss.
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
    if (_sendRDS_Ex) _sendRDS_Ex(false, false, 0, 0, 0, 0, 0, 0);
  }

  // Reads status regs 0x0A-0x0B and RDS blocks 0x0C-0x0F in one sequential read.
  // Falls back to the stock behaviour when only attachReceiveRDS() is used.
  void checkRDS() {
    if (!_sendRDS_Ex) {
      RDA5807M::checkRDS();
      return;
    }

    // Sequential read always starts at register 0x0A: 6 registers = 12 bytes.
    if (Wire.requestFrom(I2C_SEQ_EX, (uint8_t)12) != 12) return;
    uint16_t regA = _read16_Ex();
    uint16_t regB = _read16_Ex();
    uint16_t block[4];
    for (uint8_t i = 0; i < 4; i++) block[i] = _read16_Ex();

    bool rdsr = regA & 0x8000;
    bool rdss = regA & 0x1000;
    uint8_t blera = (regB >> 2) & 0x03;
    uint8_t blerb = regB & 0x03;

    bool changed = false;
    if (rdsr) {
      for (uint8_t i = 0; i < 4; i++) {
        if (block[i] != _lastBlock[i]) { _lastBlock[i] = block[i]; changed = true; }
      }
    }

    _sendRDS_Ex(rdsr && changed, rdss, blera, blerb,
                _lastBlock[0], _lastBlock[1], _lastBlock[2], _lastBlock[3]);
  }

private:
  static const uint8_t I2C_SEQ_EX = 0x10;  // RDA5807M sequential-access address
  receiveRDSFunction_Ex _sendRDS_Ex;
  uint16_t _lastBlock[4] = {0, 0, 0, 0};

  static uint16_t _read16_Ex() {
    uint8_t hi = Wire.read();
    uint8_t lo = Wire.read();
    return (uint16_t)((hi << 8) | lo);
  }
};

#endif
