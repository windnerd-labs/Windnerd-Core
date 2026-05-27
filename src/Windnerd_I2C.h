/*
 * Copyright (c) 2026, windnerd.net
 * All rights reserved.
 *
 * This source code is licensed under the BSD 3-Clause License found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once
#include "Arduino.h"
#include <Wire.h>

// Error codes returned by WN_I2C operations.
// 0..5 match TwoWire::endTransmission() return codes; 6 added by this layer.
#define WN_I2C_OK 0
#define WN_I2C_ERR_DATA_TOO_LONG 1
#define WN_I2C_ERR_NACK_ADDR 2
#define WN_I2C_ERR_NACK_DATA 3
#define WN_I2C_ERR_OTHER 4
#define WN_I2C_ERR_TIMEOUT 5
#define WN_I2C_ERR_SHORT_READ 6

// Thin wrapper around TwoWire that adds bus recovery and a write/read-register
// API with auto-recover-and-retry on failure. One instance per physical bus.
// Default-constructed against the global `Wire`; pass a different bus (e.g. Wire1)
// to drive a secondary bus.
class WN_I2C
{
public:
  WN_I2C(TwoWire &wire = Wire);

  // Configure pins and initialize the bus. Pin numbers are stored so they can be
  // re-applied during bus recovery.
  void begin(uint8_t scl_pin, uint8_t sda_pin);

  // Bitbang the bus to release a slave stuck holding SDA low. Pulses SCL up to
  // 9 times so the slave can finish the in-flight byte, then issues a STOP and
  // re-inits the underlying TwoWire. Relies on external pull-ups.
  void recoverBus();

  // Write `data` to register `reg` on device `addr`. On failure, runs bus
  // recovery and retries once. Returns WN_I2C_OK or one of WN_I2C_ERR_*.
  uint8_t writeRegister(uint8_t addr, uint8_t reg, uint8_t data);

  // Write a single command byte to device `addr` (no register/data split).
  // For chips like MS5607 whose RESET/CONVERT opcodes are single-byte writes.
  // On failure, runs bus recovery and retries once.
  uint8_t writeCommand(uint8_t addr, uint8_t cmd);

  // Read `length` bytes starting at register `reg` on device `addr` into `data`.
  // On failure, runs bus recovery and retries once. Caller must not interpret
  // `data` unless the function returns WN_I2C_OK. Returns WN_I2C_OK or one of
  // WN_I2C_ERR_*.
  uint8_t readRegister(uint8_t addr, uint8_t reg, uint8_t *data, size_t length);

  uint8_t lastError() const { return _last_error; }

private:
  TwoWire &_wire;
  uint8_t _scl_pin = 0;
  uint8_t _sda_pin = 0;
  uint8_t _last_error = WN_I2C_OK;
};
