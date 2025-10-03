/*
 * Copyright (c) 2025, windnerd.net
 * All rights reserved.
 *
 * This source code is licensed under the BSD 3-Clause License found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once
#include "Arduino.h"

#define ROLLING_BUFFER_LENGTH 200 // 10 minutes

typedef struct
{
  volatile uint16_t pulses = 0;
  volatile uint16_t dir = 0;
  bool valid = true;
} wn_raw_wind_sample_t;

class WN_ROLLINGBUFFER
{

public:
  WN_ROLLINGBUFFER();

  void addSample(uint16_t pulses, uint16_t dir);
  wn_raw_wind_sample_t get(size_t index);

private:
  wn_raw_wind_sample_t samples[ROLLING_BUFFER_LENGTH];
  size_t head = 0;
  size_t count = 0;
};