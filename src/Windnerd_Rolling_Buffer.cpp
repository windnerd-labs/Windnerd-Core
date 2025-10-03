/*
 * Copyright (c) 2025, windnerd.net
 * All rights reserved.
 *
 * This source code is licensed under the BSD 3-Clause License found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "Windnerd_Rolling_Buffer.h"

WN_ROLLINGBUFFER::WN_ROLLINGBUFFER()
{
}

void WN_ROLLINGBUFFER::addSample(uint16_t pulses, uint16_t dir)
{
  head = (head + 1) % ROLLING_BUFFER_LENGTH;
  samples[head] = {pulses, dir, true};
  if (count < ROLLING_BUFFER_LENGTH)
  {
    count++;
  }
}

// get a sample reversely indexed from last inserted position
wn_raw_wind_sample_t WN_ROLLINGBUFFER::get(size_t index)
{
  if (index >= count)
  {
    return {0, 0, false};
  }
  size_t pos = (head + ROLLING_BUFFER_LENGTH - index) % ROLLING_BUFFER_LENGTH;
  return samples[pos];
}
