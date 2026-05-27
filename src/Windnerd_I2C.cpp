/*
 * Copyright (c) 2026, windnerd.net
 * All rights reserved.
 *
 * This source code is licensed under the BSD 3-Clause License found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "Windnerd_I2C.h"

WN_I2C::WN_I2C(TwoWire &wire) : _wire(wire) {}

void WN_I2C::begin(uint8_t scl_pin, uint8_t sda_pin)
{
  _scl_pin = scl_pin;
  _sda_pin = sda_pin;
  _wire.setSDA(_sda_pin);
  _wire.setSCL(_scl_pin);
  _wire.begin();
}

void WN_I2C::recoverBus()
{
  _wire.end();

  pinMode(_scl_pin, INPUT_PULLUP);
  pinMode(_sda_pin, INPUT_PULLUP);
  delayMicroseconds(10);

  for (uint8_t i = 0; i < 9; i++)
  {
    if (digitalRead(_sda_pin) == HIGH) break;
    pinMode(_scl_pin, OUTPUT);
    digitalWrite(_scl_pin, LOW);
    delayMicroseconds(5);
    pinMode(_scl_pin, INPUT_PULLUP);
    delayMicroseconds(5);
  }

  // STOP condition: SDA rises while SCL is high
  pinMode(_sda_pin, OUTPUT);
  digitalWrite(_sda_pin, LOW);
  delayMicroseconds(5);
  pinMode(_scl_pin, INPUT_PULLUP);
  delayMicroseconds(5);
  pinMode(_sda_pin, INPUT_PULLUP);
  delayMicroseconds(5);

  _wire.setSDA(_sda_pin);
  _wire.setSCL(_scl_pin);
  _wire.begin();
}

uint8_t WN_I2C::writeRegister(uint8_t addr, uint8_t reg, uint8_t data)
{
  _wire.beginTransmission(addr);
  _wire.write(reg);
  _wire.write(data);
  _last_error = _wire.endTransmission();

  if (_last_error != WN_I2C_OK)
  {
    recoverBus();
    _wire.beginTransmission(addr);
    _wire.write(reg);
    _wire.write(data);
    _last_error = _wire.endTransmission();
  }
  return _last_error;
}

uint8_t WN_I2C::writeCommand(uint8_t addr, uint8_t cmd)
{
  _wire.beginTransmission(addr);
  _wire.write(cmd);
  _last_error = _wire.endTransmission();

  if (_last_error != WN_I2C_OK)
  {
    recoverBus();
    _wire.beginTransmission(addr);
    _wire.write(cmd);
    _last_error = _wire.endTransmission();
  }
  return _last_error;
}

uint8_t WN_I2C::readRegister(uint8_t addr, uint8_t reg, uint8_t *data, size_t length)
{
  _wire.beginTransmission(addr);
  _wire.write(reg);
  _last_error = _wire.endTransmission();

  if (_last_error != WN_I2C_OK)
  {
    recoverBus();
    _wire.beginTransmission(addr);
    _wire.write(reg);
    _last_error = _wire.endTransmission();
    if (_last_error != WN_I2C_OK) return _last_error;
  }

  if (_wire.requestFrom(addr, length) != length)
  {
    _last_error = WN_I2C_ERR_SHORT_READ;
    recoverBus();
    return _last_error;
  }

  for (size_t i = 0; i < length; i++)
  {
    if (!_wire.available())
    {
      _last_error = WN_I2C_ERR_SHORT_READ;
      return _last_error;
    }
    data[i] = _wire.read();
  }
  return _last_error;
}
