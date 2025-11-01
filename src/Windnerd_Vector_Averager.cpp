/*
 * Copyright (c) 2025, windnerd.net
 * All rights reserved.
 *
 * This source code is licensed under the BSD 3-Clause License found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "Windnerd_Vector_Averager.h"

WN_VECTOR_AVERAGER::WN_VECTOR_AVERAGER()
{
}

void WN_VECTOR_AVERAGER::accumulate(wn_raw_wind_sample_t sample)
{
  accumulate(sample.pulses, sample.dir);
}

void WN_VECTOR_AVERAGER::accumulate(uint32_t pulses, uint16_t dir)
{
  // Convert dir (degrees) into radians
  float rad = dir * (M_PI / 180.0f);

  // Add to vector components (weighted by pulses = speed proxy)
  x += pulses * cosf(rad);
  y += pulses * sinf(rad);

  // Track counts and min/max
  cnt++;
  if (pulses > wind_max)
    wind_max = pulses;
  if (pulses < wind_min)
    wind_min = pulses;
}

void WN_VECTOR_AVERAGER::computeReportFromAccumulatedValues(wn_raw_wind_report_t *report)
{
  if (cnt == 0)
  {
    return;
  }

  // Average vector direction
  float avgX = x / cnt;
  float avgY = y / cnt;
  float dir = atan2f(avgY, avgX) * 180.0f / M_PI;
  if (dir < 0)
  {
    dir += 360.0f;
  }

  report->pulses_avg = sqrtf(avgX * avgX + avgY * avgY);
  report->dir_avg = (uint16_t)(dir + 0.5f);
  report->pulses_max = wind_max;
  report->pulses_min = wind_min;
  x = 0;
  y = 0;
  cnt = 0;
}
