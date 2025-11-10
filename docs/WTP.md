# WindNerd Transfer Protocol

## Introduction

The WindNerd Transfer Protocol (WTP) enables recording and broadcasting of fast-changing wind and other weather data even without a permanent internet connection.

Reports with intervals as short as 1 minute can be aggregated and sent in a single request to be recorded and published for your station on **windnerd.net**

High-frequency samples, every 3 seconds, can be sent in real-time or periodically aggregated to be replayed with a delay.

## Authentication

A 16 characters hexadecimal secret key is required, this key is available to WindNerd Core users in their [station management console](https://windnerd.net/en/management). Key can be regenerated at any time.


## Post data

#### Endpoint (use HTTPS if possible):

HTTPS: https://wtp.windnerd.net/post

HTTP:  http://wtp.windnerd.net/post

Two kinds of data can be posted:
 - reports: these reports are stored in windnerd.net database. Average, minimum, and maximum values over longer periods are derived from them.
 - high-frequency wind samples: these samples are broadcast in real time to visualize and feel the wind as it happens. Aggregated samples are queued for delayed replay.

#### JSON

**Method:** `POST`  
**Content-Type:** `application/json`  
**Responses:** `200 Request accepted`,  `400 Bad Request` or `401 Wrong Authentication`

```json
{
  "key": "3122fd880084fd55",
  "interval_mn": 1,
  "wind_unit": "ms",
  "temp_unit": "C",
  "reports": [
    { "wind_avg": 3.1, "wind_dir": 90,  "wind_min": 0.5, "wind_max": 3.5, "temperature": 22, "humidity": 70, "pressure": 1012 }, // most recent
    { "wind_avg": 1.8, "wind_dir": 45,  "wind_min": 0.0, "wind_max": 2.0, "temperature": 28, "humidity": 85, "pressure": 1016 },
    { "wind_avg": 4.0, "wind_dir": 180, "wind_min": 1.0, "wind_max": 5.0, "temperature": 21, "humidity": 60, "pressure": 1010 },
    { "wind_avg": 2.0, "wind_dir": 200, "wind_min": 0.5, "wind_max": 2.5, "temperature": 23, "humidity": 72, "pressure": 1011 },
    { "wind_avg": 3.5, "wind_dir": 310, "wind_min": 1.5, "wind_max": 4.5, "temperature": 20, "humidity": 65, "pressure": 1009 } // most ancient
  ],
    "samples": [
    { "wind_inst": 3.1, "wind_dir": 90 }, // most recent
    { "wind_inst": 1.8, "wind_dir": 75 },
    { "wind_inst": 4.0, "wind_dir": 110 },
    { "wind_inst": 2.5, "wind_dir": 80 },
    { "wind_inst": 3.3, "wind_dir": 82 },
    { "wind_inst": 0.8, "wind_dir": 101 },
    { "wind_inst": 2.0, "wind_dir": 105 },
    { "wind_inst": 3.5, "wind_dir": 120 },
    { "wind_inst": 1.2, "wind_dir": 130 },
    { "wind_inst": 2.8, "wind_dir": 125 },
    { "wind_inst": 3.0, "wind_dir": 100 },
    { "wind_inst": 2.2, "wind_dir": 92 },
    { "wind_inst": 1.5, "wind_dir": 95 },
    { "wind_inst": 4.2, "wind_dir": 102 },
    { "wind_inst": 3.8, "wind_dir": 120 },
    { "wind_inst": 2.6, "wind_dir": 104 },
    { "wind_inst": 1.9, "wind_dir": 96 },
    { "wind_inst": 3.2, "wind_dir": 85 },
    { "wind_inst": 2.0, "wind_dir": 93 },
    { "wind_inst": 2.1, "wind_dir": 90 }, // most ancient
  ]
}
```

##### Parameters Fields

| Field         | Alt. | Type    | Required | Description                                               |
| ------------- | ----- | ------- | -------- | --------------------------------------------------------- |
| `key`         | `k`     | string  | Yes      | Secret key for authentication                             |
| `interval_mn` | `i`     | integer | No       | Interval in minutes between reports (max: `60`, default: `1`)      |
| `wind_unit`   | `wu`    | string  | No       | Wind speed unit. Accepts `"kmh"`, `"ms"`, `"mph"` (default: `"ms"`) |
| `temp_unit`   | `tu`   | string  | No       | Temperature unit. Accepts `"C"`, `"F"` (default: `"C"`)    |
| `reports`     | `rp`    | array   | Yes      | Array of aggregated reports (max: 20 items)                             |


##### Report Fields

| Field         | Alt. | Type   | Required | Min  | Max  | Description                                    |
| ------------- | ---- | ------ | -------- | ---- | ---- | ---------------------------------------------- |
| `wind_avg`    | `wa` | number | Yes      | 0    | 360  | Vector-averaged wind speed during the interval |
| `wind_min`    | `wn` | number | Yes      | 0    | 360  | Minimum wind speed during the interval         |
| `wind_max`    | `wx` | number | Yes      | 0    | 360  | Maximum wind speed during the interval         |
| `wind_dir`    | `wd` | number | Yes      | 0    | 360  | Average wind direction in degrees (0–359)      |
| `temperature` | `tp` | number | No       | -60  | 80   | Ambient temperature                            |
| `humidity`    | `hu` | number | No       | 0    | 100  | Relative humidity (%)                          |
| `pressure`    | `pr` | number | No       | 200  | 1100 | Atmospheric pressure (hPa)                     |
| `voltage`     | `vo` | number | No       | 0    | 999  | Voltage (V)                                    |
| `rssi`        | `rs` | number | No       | -999 | 999  | Signal strength indicator (RSSI)               |


##### Sample Fields

| Field         | Alt. | Type   | Required | Min  | Max  | Description                                    |
| ------------- | ---- | ------ | -------- | ---- | ---- | ---------------------------------------------- |
| `wind_inst`    | `wi` | number | Yes      | 0    | 360  | Wind speed during the 3 sec interval |
| `wind_dir`    | `wd` | number | Yes      | 0    | 360  | Wind direction during the 3 sec interval          |

---

#### TEXT

**Method:** `POST`  
**Content-Type:** `text/plain`  
**Responses:** `200 Request accepted`,  `400 Bad Request` or `401 Wrong Authentication`

Alternatively, the payload can be encoded as plain text. 

The first line defines the parameters, and each subsequent line represents a report (line starting with `r`) or sample (line starting with `s`).

Lines are separated by semicolons; end-of-line and carriage return characters are optional.

Sample and report lines must be ordered from most recent to most ancient.

```text
k=3122fd880084fd55,i=1,wu=ms,tu=C;
r,wa=3.1,wd=90,wn=0.5,wx=3.5,tp=22,hu=70,pr=1012;
r,wa=1.8,wd=45,wn=0.0,wx=2.0,tp=28,hu=85,pr=1016;
r,wa=4.0,wd=180,wn=1.0,wx=5.0,tp=21,hu=60,pr=1010;
r,wa=2.0,wd=200,wn=0.5,wx=2.5,tp=23,hu=72,pr=1011;
r,wa=3.5,wd=310,wn=1.5,wx=4.5,tp=20,hu=65,pr=1009;
s,wi=3.1,wd=90;
s,wi=1.8,wd=75;
s,wi=4.0,wd=110;
s,wi=2.5,wd=80;
s,wi=3.3,wd=82;
s,wi=0.8,wd=101;
s,wi=2.0,wd=105;
s,wi=3.5,wd=120;
s,wi=1.2,wd=130;
s,wi=2.8,wd=125;
s,wi=3.0,wd=100;
s,wi=2.2,wd=92;
s,wi=1.5,wd=95;
s,wi=4.2,wd=102;
s,wi=3.8,wd=120;
s,wi=2.6,wd=104;
s,wi=1.9,wd=96;
s,wi=3.2,wd=85;
```



## Examples in context

### Permanent connection
A Raspberry Pi in a ceiling, connected to WindNerd Core, has continuous power and internet access. JSON format is handled easily and we can send data without limitation. 

We post high-frequency samples every 3 seconds:

```json
{
  "v": 1,
  "k": "3122fd880084fd55",
  "wind_unit": "ms",
  "samples": [
    { "wind_inst": 3.1, "wind_dir": 90 }
  ]
}
```

We post a report every minute:
```json
{
  "key": "3122fd880084fd55",
  "interval_mn": 1,
  "wind_unit": "ms",
  "temp_unit": "C",
  "reports": [
    { "wind_avg": 3.1, "wind_dir": 90,  "wind_min": 0.5, "wind_max": 3.5, "temperature": 22, "humidity": 70, "pressure": 1012 },
  ]
}
```
---

### Limited radio use
A WindNerd Core is connected to a LoRa development board and installed at a paragliding takeoff. The LoRa receiver has permanent power and internet access, but fair-use limitations allow only one transmission per minute.

We connect every minute to send aggregated reports and high-frequency samples. To keep messages short and easy to construct, we use the plain-text format. Pilots can still see a real-time wind animation, with a 1-minute delay due to the transmission interval.

We post 20 samples and a report every minute:

```text
k=3122fd880084fd55,wu=ms;
r,wa=3.1,wd=90,wn=0.5,wx=3.5;
s,wi=3.1,wd=90;
s,wi=1.8,wd=75;
s,wi=4.0,wd=110;
s,wi=2.5,wd=80;
s,wi=3.3,wd=82;
s,wi=0.8,wd=101;
s,wi=2.0,wd=105;
s,wi=3.5,wd=120;
s,wi=1.2,wd=130;
s,wi=2.8,wd=125;
s,wi=3.0,wd=100;
s,wi=2.2,wd=92;
s,wi=1.5,wd=95;
s,wi=4.2,wd=102;
s,wi=3.8,wd=120;
s,wi=2.6,wd=104;
s,wi=1.9,wd=96;
s,wi=3.2,wd=85;
s,wi=2.0,wd=93;
s,wi=2.1,wd=90;
```
---

### Limited power

A WindNerd Core is connected to a 4G modem development board and installed at a remote location. Power is limited because it relies on a small solar cell and battery, and winter nights are long.

We activate a power-saving mode, making a connection only every 20 minutes. During each connection, we send 20 1-minute reports. Data are no longer real-time, but all detailed history is preserved.

We post 20 reports every 20 minute:

```text
k=3122fd880084fd55,i=1,wu=ms;
r,wa=2.1,wd=180,wn=1.0,wx=2.5,tp=0.2,hu=88,pr=1018.0;
r,wa=1.8,wd=190,wn=0.5,wx=2.0,tp=0.3,hu=89,pr=1018.1;
r,wa=2.5,wd=175,wn=1.2,wx=3.0,tp=0.2,hu=87,pr=1018.0;
r,wa=1.9,wd=200,wn=0.8,wx=2.3,tp=0.1,hu=90,pr=1018.1;
r,wa=2.2,wd=185,wn=0.9,wx=2.8,tp=0.2,hu=88,pr=1018.0;
r,wa=2.0,wd=170,wn=0.7,wx=2.5,tp=0.2,hu=89,pr=1018.1;
r,wa=2.8,wd=195,wn=1.0,wx=3.2,tp=0.3,hu=87,pr=1018.0;
r,wa=1.5,wd=180,wn=0.5,wx=1.9,tp=0.1,hu=91,pr=1018.0;
r,wa=2.3,wd=210,wn=1.0,wx=2.7,tp=0.2,hu=88,pr=1018.1;
r,wa=2.6,wd=190,wn=1.2,wx=3.0,tp=0.2,hu=89,pr=1018.0;
r,wa=2.0,wd=175,wn=0.8,wx=2.4,tp=0.1,hu=90,pr=1018.0;
r,wa=2.7,wd=200,wn=1.0,wx=3.1,tp=0.2,hu=87,pr=1018.1;
r,wa=1.9,wd=185,wn=0.7,wx=2.5,tp=0.2,hu=88,pr=1018.0;
r,wa=2.4,wd=170,wn=1.0,wx=2.9,tp=0.3,hu=89,pr=1018.1;
r,wa=2.1,wd=195,wn=0.8,wx=2.6,tp=0.2,hu=90,pr=1018.0;
r,wa=2.5,wd=180,wn=1.1,wx=3.0,tp=0.2,hu=88,pr=1018.0;
r,wa=2.2,wd=200,wn=0.9,wx=2.7,tp=0.2,hu=87,pr=1018.1;
r,wa=1.8,wd=190,wn=0.5,wx=2.2,tp=0.1,hu=90,pr=1018.0;
r,wa=2.3,wd=175,wn=0.8,wx=2.8,tp=0.2,hu=89,pr=1018.1;
r,wa=2.0,wd=185,wn=0.7,wx=2.5,tp=0.2,hu=88,pr=1018.0;

```

Samples are omitted.