# WindNerd Core API Reference

The WindNerd Core library provides wind speed and direction measurements from the WindNerd Core sensor board or alternative board based on same design.

Internally it:

- counts rotor pulses to compute wind speed
- reads the TMAG5273 magnetic sensor for wind direction
- samples data periodically
- stores samples in a rolling buffer
- computes averaged/min/max wind reports
- controls the diagnostic LEDs: speed LED blinks at each rotation, north LED turns ON when the vane is indicating north

Applications can obtain data using either:

- Pull-based access (poll samples or reports when needed)
- Push-based access (receive updates via callbacks)

## 1. Minimal Setup

The minimal setup runs the anemometer and lets you retrieve wind data from the internal buffer.

The user must:

1. Instantiate WN_Core
2. Call begin()
3. Call loop() frequently
4. Pull samples when needed

Example

```
#include <Windnerd_Core.h>

WN_Core Anemometer;


void setup() {
    Anemometer.begin();
}

void loop() {
    Anemometer.loop();

    // read the latest sample
    wn_instant_wind_sample_t sample = Anemometer.getSampleIndexedFromLast(0);

    Serial.print("Speed: ");
    Serial.print(sample.speed);
    Serial.print(" Direction: ");
    Serial.println(sample.dir);
}

```

What happens internally:
- Rotor pulses are counted using interrupts
- Direction is sampled periodically and averaged over 3 seconds
- Every 3 seconds a new wind sample (instant speed + wind direction) is generated
- Samples are stored in a rolling buffer

If Arduino loop() is blocked too long (>50ms), samples may be dropped.

## 2. Pull Model (Polling Data)

In the pull model, your application retrieves wind data whenever it wants.

This is useful when:

- your firmware already has its own scheduler
- you want full control over when data is processed

### Get the Latest Sample

```
wn_instant_wind_sample_t sample = Anemometer.getSampleIndexedFromLast(0);
```
Structure:

```
wn_instant_wind_sample_t
{
    float speed;
    uint16_t dir;
}
```

Fields:

| Field | Description                  |
| ----- | ---------------------------- |
| speed | wind speed in selected unit (default m/s)  |
| dir   | direction in degrees (0-359) |


The rolling buffer stores samples for the last 20 minutes (400 samples)

index 0 → newest sample

index 1 → previous sample

index N → older samples

Example:
```
wn_instant_wind_sample_t sample = Anemometer.getSampleIndexedFromLast(5);
```

### Compute Average Wind Report

Compute statistics over the most recent period for a duration given in seconds.

Example for the last minute:
```
wn_wind_report_t report = Anemometer.computeReportForRecentPeriodInSec(60);
```

Structure:
```
wn_wind_report_t
{
    float avg_speed;
    float min_speed;
    float max_speed;
    uint16_t avg_dir;
}
```

Fields:

| Field     | Description                    |
| --------- | ------------------------------ |
| avg_speed | average wind speed             |
| min_speed | minimum wind speed             |
| max_speed | maximum wind speed             |
| avg_dir   | vector-averaged wind direction |

Alternatively, `computeReportForPeriodInSecIndexedFromLast` allows to compute a report for an anterior period of time

For example:
```
  wn_wind_report_t report = Anemometer.computeReportForPeriodInSecIndexedFromLast(60, 1);
```
This creates a report for the period between 2 minutes ago and 1 minute ago.


## 3. Push Model (Callbacks)

In the push model, the library calls your code whenever new wind data is available.

This is useful when:

- you want event-driven firmware
- you want to minimize polling
- you want immediate processing of new measurements

### Instant Wind Callback

Called every sampling window (~3 seconds).

```
void onInstantWind(wn_instant_wind_sample_t sample)
{
    Serial.print("Speed: ");
    Serial.println(sample.speed);
}

void setup()
{
    Anemometer.onInstantWindUpdate(onInstantWind);
}

```

### Average Wind Report Callback

Called at the configured reporting interval.
```
void onWindReport(wn_wind_report_t report)
{
    Serial.print("Avg speed: ");
    Serial.println(report.avg_speed);
}

void setup()
{
    Anemometer.onNewWindReport(onWindReport);
}
```

## 4. Configuration

The library exposes configuration functions to adapt behavior to different installations.

### Set Speed Units

```
Anemometer.setSpeedUnit(UNIT_MS);
```

Available units:

| Unit     | Description         |
| -------- | ------------------- |
| UNIT_MS  | meters per second   |
| UNIT_KN  | knots               |
| UNIT_KPH | kilometers per hour |
| UNIT_MPH | miles per hour      |


### Adjust the conversion between rotor frequency and wind speed.

```
Anemometer.setFrequencyToWindSpeedRatio(1.31);

```
Default value `1.31` corresponds to the standard WindNerd rotor.


### Set Averaging Period

Controls how long wind data is averaged when using push mode.

```
Anemometer.setAveragingPeriodInSec(60);
```

Constraints:

period >= 3 seconds

period <= 1200 seconds

### Set Reporting Interval

Controls how often a wind report is produced when using push mode.

```
Anemometer.setReportingIntervalInSec(60);
```
Example:

| Averaging | Reporting |
| --------- | --------- |
| 600s       | 60s       |


This produces a rolling 600-second average updated every 60 seconds.

### Invert Vane Polarity

Depending on how the vane magnet was mounted, you may need to invert the polarity if you notice south and north are inverted.

```
Anemometer.invertVanePolarity(true);
```

## 5. Low Power Mode

Low power mode reduces vane measurement frequency.

```
Anemometer.enableLowPowerMode();
```

Behavior changes:

| Feature       | Normal     | Low Power    |
| ------------- | ---------- | ------------ |
| vane sampling | every 100 ms | every 500 ms |
| LED activity  | enabled    | disabled      |

Disable with:
```
Anemometer.disableLowPowerMode();
```