/*
 * Copyright (c) 2025, windnerd.net
 * All rights reserved.
 *
 * This source code is licensed under the BSD 3-Clause License found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once
#include "Windnerd_Rolling_Buffer.h"

typedef struct
{
  float pulses_avg = 0;
  uint16_t dir_avg = 0;
  uint32_t pulses_max = 0;
  uint32_t pulses_min = 0;
} wn_raw_wind_report_t;

class WN_VECTOR_AVERAGER
{

public:
  WN_VECTOR_AVERAGER();

  void accumulate(uint32_t pulses, uint16_t dir);
  void accumulate(wn_raw_wind_sample_t sample);
  void computeReportFromAccumulatedValues(wn_raw_wind_report_t *report);

private:
  float x = 0;
  float y = 0;
  uint32_t cnt = 0;
  uint32_t wind_max = 0;
  uint32_t wind_min = 0xFFFFFFFF;
};
