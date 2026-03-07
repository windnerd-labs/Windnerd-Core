# Air780E Dual Upload - WindNerd WTP + Windguru

This example uploads wind data to **both** WindNerd WTP and Windguru platforms using the Air780E 4G LTE modem. Designed for **long-term unattended deployment** with solar + battery power.

## Features

- **Dual platform upload**: Sends data to WindNerd WTP (POST) and Windguru (GET)
- **Auto APN detection**: Modem auto-detects carrier APN (fallback configurable)
- **Unique salt generation**: STM32 UID + 32-bit random (64-bit total, collision-proof)
- **Deep sleep**: Both MCU and modem enter low power modes between uploads
- **Bearer management**: Proper SAPBR setup for Air780E module
- **Configurable intervals**: Easy to adjust reporting frequency
- **Data efficient**: Optional sample upload toggle saves ~90% data

### Reliability Features (for unattended operation)

- **Independent Watchdog (IWDG)**: Auto-resets MCU if firmware hangs (10s timeout)
- **State timeout**: Aborts upload cycle if any state takes >90 seconds
- **Periodic modem reset**: AT+CFUN=1,1 every 96 cycles (~24 hours at 15-min intervals)

## Configuration

Edit these values in the sketch:

\`\`\`cpp
// WindNerd WTP
#define WTP_SECRET_KEY "your-secret-key"  // From windnerd.net device settings
#define ENABLE_WTP_SAMPLES false          // true = send 3-sec samples, false = reports only

// Windguru  
#define WINDGURU_UID "your-station-uid"   // From windguru.cz station settings
#define WINDGURU_PASSWORD "your-password" // Upload password
#define ENABLE_WINDGURU true              // Set false to disable Windguru uploads

// Network
#define APN_FALLBACK "internet"           // Fallback APN (usually auto-detected)

// Timing
#define REPORT_INTERVAL_MN 15             // Minutes between uploads

// Reliability
#define WATCHDOG_TIMEOUT_SEC 10           // MCU reset if stuck for 10s
#define STATE_TIMEOUT_SEC 90              // Abort cycle if state exceeds 90s
#define MODEM_RESET_CYCLES 96             // Full modem reset every N cycles
\`\`\`

## Data Sent

### WindNerd WTP (HTTP POST)
- \`k=\` - Device secret key
- \`r,wa=,wd=,wn=,wx=\` - Report lines (avg, direction, min, max)
- \`s,wi=,wd=\` - Sample lines (optional, if ENABLE_WTP_SAMPLES=true)

### Windguru (HTTP GET)
| Parameter | Description |
|-----------|-------------|
| \`uid\` | Station UID |
| \`salt\` | Unique ID (STM32 UID + random) |
| \`hash\` | MD5(salt + uid + password) |
| \`interval\` | Measurement interval in seconds |
| \`wind_avg\` | Average wind speed (knots) |
| \`wind_max\` | Maximum wind speed (knots) |
| \`wind_min\` | Minimum wind speed (knots) |
| \`wind_direction\` | Wind direction (degrees) |

## Upload Sequence

1. **Diagnostics**: CSQ (signal), CREG (GSM), CEREG (LTE), CGATT (GPRS)
2. **APN Query**: AT+CGCONTRDP (auto-detect carrier APN)
3. **Bearer setup**: SAPBR APN and open connection
4. **WindNerd WTP**: HTTP POST with wind reports (and optional samples)
5. **Windguru**: HTTP GET with wind data + interval
6. **Sleep**: AT+CSCLK=2 for modem, HAL_PWR_EnterSLEEPMode for MCU

## Reliability Mechanisms

### Watchdog Timer (IWDG)
The STM32's Independent Watchdog runs on the internal LSI clock (~32kHz) and operates independently of the main clock. If the firmware hangs for any reason:

- \`kickWatchdog()\` must be called within 10 seconds
- Called automatically in \`loop()\` and \`processModem()\`
- MCU auto-resets if watchdog times out
- Watchdog persists across software resets (only power cycle disables)

### State Timeout
If any modem state takes longer than 90 seconds (e.g., stuck waiting for network):

- Current upload cycle is aborted
- HTTP session is cleaned up (\`AT+HTTPTERM\`)
- State machine jumps to SLEEP state
- Next cycle starts fresh on next interval

### Periodic Modem Reset
Every 96 upload cycles (~24 hours at 15-min intervals):

- Full modem reset via \`AT+CFUN=1,1\`
- Clears accumulated connection issues
- Prevents long-term modem memory leaks
- Modem restarts fresh for next cycle

## Salt Generation

The Windguru API requires a unique salt for each upload. This implementation uses:

\`\`\`
salt = STM32_UID[31:0] + random(32-bit)
\`\`\`

- **STM32 UID**: 32-bit portion of the chip's unique 96-bit factory ID (survives reflash)
- **Random**: 32-bit random seeded from full 96-bit UID (different each upload)
- **Total**: 64-bit uniqueness = ~18 quintillion combinations

At 35,000 uploads/year, collision probability is negligible for centuries.

## Data Usage Estimates

| Interval | With Samples | Reports Only |
|----------|--------------|--------------|
| 2 min | ~1.8 GB/year | ~180 MB/year |
| 15 min | ~245 MB/year | **~25 MB/year** |
| 30 min | ~123 MB/year | ~13 MB/year |

## Hardware

- **MCU**: STM32G031F8 @ 8MHz (low power clock config)
- **Modem**: Air780E 4G LTE on USART2
- **Debug**: USART1 @ 115200 baud
- **Power**: 2x 18650 batteries + solar panel (unattended operation)

## Debug Output

Connect FTDI RX to modem TX (USART2) to see AT commands and responses.
Debug serial (USART1) shows state transitions and error messages.

## Memory Usage

- **Flash**: ~62KB (94% of 64KB)
- **RAM**: ~5KB (60% of 8KB)

Optimizations:
- No EEPROM library (uses STM32 UID directly)
- Char arrays instead of String class
- Reduced buffer sizes
- No sample storage (reports only mode)
