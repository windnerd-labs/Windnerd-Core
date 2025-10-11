# 4G Board Integration - Luat Air780E

This document describes how to integrate the WindNerd Core with the Luat Air780E 4G board for remote wind data transmission.

## Overview

The `4g-air780e` example demonstrates how to:
- Wake up the 4G board every 15 minutes
- Transmit wind speed and wind direction data via 4G
- Send data to Windguru API for online wind monitoring
- Minimize power consumption by putting the 4G module to sleep between transmissions

## Hardware Connection

### Pin Connections

| WindNerd Core Pin | Air780E Pin | Function |
|------------------|-------------|----------|
| PA0 | PWRKEY | Power control |
| TX2 (USART2) | RXD | UART TX to 4G module |
| RX2 (USART2) | TXD | UART RX from 4G module |
| GND | GND | Ground |
| 3.3V or 5V | VBAT | Power supply |

### Wiring Diagram

```
WindNerd Core          Luat Air780E
┌────────────┐         ┌────────────┐
│            │         │            │
│     PA0────┼────────►│  PWRKEY    │
│            │         │            │
│     TX2────┼────────►│  RXD       │
│            │         │            │
│     RX2◄───┼─────────│  TXD       │
│            │         │            │
│     GND────┼─────────│  GND       │
│            │         │            │
│  3.3V/5V───┼────────►│  VBAT      │
│            │         │            │
└────────────┘         └────────────┘
```

## Configuration

### Windguru Station Setup

Before using the firmware, you need to register your station on Windguru:

1. **Create a Windguru Account**: Visit [Windguru Stations](https://stations.windguru.cz/) and create an account
2. **Register Your Station**: Add a new weather station and obtain:
   - **Station UID**: Your unique station identifier
   - **Station Password**: Authentication password for API access
3. **Configure the Firmware**: Edit the configuration section in the sketch:

```cpp
#define WINDGURU_STATION_UID "YOUR_STATION_UID"    // Replace with your station UID
#define WINDGURU_STATION_PASSWORD "YOUR_PASSWORD"  // Replace with your station password
```

**Example:**
```cpp
#define WINDGURU_STATION_UID "mystation123"
#define WINDGURU_STATION_PASSWORD "MySecurePass456"
```

### 4G Module Settings

The example uses the following default settings:
- **UART Baud Rate**: 115200 bps
- **Wake-up Interval**: 15 minutes (900,000 ms)
- **Startup Delay**: 3 seconds (time for module to boot)
- **Command Timeout**: 5 seconds

You can modify these settings in the sketch:
```cpp
#define WAKEUP_INTERVAL_MS (15 * 60 * 1000)  // 15 minutes
#define AIR780E_STARTUP_DELAY_MS 3000        // 3 seconds
#define AIR780E_COMMAND_TIMEOUT_MS 5000      // 5 seconds
```

### Power Control Pin

The example uses PA0 as the power control pin. If you need to use a different pin, modify:
```cpp
#define AIR780E_POWER_PIN PA0  // Change to your preferred GPIO pin
```

## How It Works

### Initialization

1. The STM32 initializes USART2 for communication with the Air780E at 115200 baud
2. The power control pin (PA0) is configured as output
3. The 4G module is initially powered off to save energy
4. Wind sensor data collection begins

### Operation Cycle

Every 15 minutes, the system:

1. **Wake-up Phase**
   - Sends power-on pulse to Air780E (100ms HIGH pulse)
   - Waits for module to boot (3 seconds)
   - Sends AT commands to verify module is ready

2. **Data Transmission Phase**
   - Checks network registration
   - Generates authentication hash (MD5) using salt, UID, and password
   - Converts wind speed from km/h to knots (Windguru requirement)
   - Formats Windguru API URL with wind data
   - Sends HTTP GET request to: `http://www.windguru.cz/upload/api.php`
   - Example URL: `http://www.windguru.cz/upload/api.php?uid=mystation&salt=123456&hash=abc123&wind_avg=12.5&wind_direction=270`

3. **Sleep Phase**
   - Terminates HTTP connection
   - Sends sleep command: `AT+CSCLK=2`
   - Powers down the module (1200ms HIGH pulse)
   - Returns to low-power mode

### Wind Data Format

The data is transmitted to Windguru API using the following format:

**Windguru API Parameters:**
- **uid**: Station unique identifier
- **salt**: Random string (timestamp) for security
- **hash**: MD5 hash of `salt + uid + password` for authentication
- **wind_avg**: Average wind speed in **knots** (automatically converted from km/h)
- **wind_direction**: Wind direction in degrees (0-359, where 0 = North)

**Conversion Notes:**
- The firmware automatically converts wind speed from km/h to knots (1 km/h = 0.539957 knots)
- Wind direction is transmitted directly in degrees as measured by the sensor

**Example Transmission:**
```
URL: http://www.windguru.cz/upload/api.php?uid=mystation&salt=1234567890&hash=c9441d30280f4f6f4946fe2b2d360df5&wind_avg=6.7&wind_direction=45
```

This means: 6.7 knots (12.4 km/h) wind from 45° (northeast).

## AT Commands Used

The example uses standard AT commands compatible with the Air780E:

| Command | Purpose | Expected Response |
|---------|---------|-------------------|
| `AT` | Check module status | `OK` |
| `AT+CREG?` | Check network registration | `+CREG:` |
| `AT+CSCLK=2` | Enable auto sleep mode | `OK` |
| `AT+HTTPINIT` | Initialize HTTP service | `OK` |
| `AT+HTTPPARA="URL","..."` | Set HTTP URL parameter | `OK` |
| `AT+HTTPACTION=0` | Execute HTTP GET request | `OK` |
| `AT+HTTPREAD` | Read HTTP response | `OK` |
| `AT+HTTPTERM` | Terminate HTTP service | `OK` |

## Customization

### Windguru Integration Options

**Change Station Credentials:**
```cpp
#define WINDGURU_STATION_UID "newstation"
#define WINDGURU_STATION_PASSWORD "newpassword"
```

**Add Additional Weather Parameters:**

Windguru API supports additional optional parameters. You can extend the URL in `transmitWindData()`:

```cpp
// Add temperature (in Celsius)
float temperature = 25.5;

// Add wind gust (maximum wind speed in knots)
float windGustKnots = kphToKnots(latestWind.maxSpeed);

// Build extended URL
snprintf(urlBuffer, sizeof(urlBuffer),
         "http://www.windguru.cz/upload/api.php?uid=%s&salt=%s&hash=%s&wind_avg=%.1f&wind_max=%.1f&wind_direction=%d&temperature=%.1f",
         WINDGURU_STATION_UID, salt, md5Output, windSpeedKnots, windGustKnots, latestWind.direction, temperature);
```

**Supported Optional Parameters:**
- `wind_max`: Maximum wind speed (knots)
- `wind_min`: Minimum wind speed (knots)
- `temperature`: Air temperature (°C)
- `rh`: Relative humidity (%)
- `mslp`: Mean sea level pressure (hPa)
- `precip`: Precipitation (mm)

### Change Transmission Protocol

The default example sends raw data. You can modify it to use:

**HTTP POST:**
```cpp
Serial4G.println("AT+HTTPINIT");
Serial4G.println("AT+HTTPPARA=\"URL\",\"http://yourserver.com/api/wind\"");
Serial4G.print("AT+HTTPDATA=");
Serial4G.print(strlen(dataBuffer));
Serial4G.println(",10000");
Serial4G.println(dataBuffer);
Serial4G.println("AT+HTTPACTION=1");
```

**MQTT:**
```cpp
Serial4G.println("AT+MCONFIG=\"clientID\",\"broker.hivemq.com\",1883");
Serial4G.println("AT+MCONNECT=1,60");
Serial4G.println("AT+MPUB=\"wind/data\",0,0,\"" + String(dataBuffer) + "\"");
```

### Change Wake-up Interval

To transmit data more or less frequently:

```cpp
// Every 5 minutes
#define WAKEUP_INTERVAL_MS (5 * 60 * 1000)

// Every 30 minutes
#define WAKEUP_INTERVAL_MS (30 * 60 * 1000)

// Every hour
#define WAKEUP_INTERVAL_MS (60 * 60 * 1000)
```

### Add Data Buffering

To store multiple readings before transmission:

```cpp
#define MAX_BUFFER_SIZE 10
WindData windBuffer[MAX_BUFFER_SIZE];
int bufferIndex = 0;

void instantWindCallback(wn_instant_wind_report_t instant_report)
{
  if (bufferIndex < MAX_BUFFER_SIZE) {
    windBuffer[bufferIndex].speed = instant_report.speed;
    windBuffer[bufferIndex].direction = instant_report.dir;
    windBuffer[bufferIndex].timestamp = millis();
    windBuffer[bufferIndex].valid = true;
    bufferIndex++;
  }
}

void transmitWindData()
{
  // Transmit all buffered data
  for (int i = 0; i < bufferIndex; i++) {
    // Send windBuffer[i]
  }
  bufferIndex = 0;  // Reset buffer
}
```

## Power Consumption

The system is optimized for low power consumption:

- **WindNerd Core in sleep**: ~1.5 mA
- **Air780E in sleep**: ~1 mA
- **Air780E active**: ~100-300 mA (during transmission)
- **Average consumption**: ~2-3 mA (depending on transmission duration)

With a typical transmission lasting 5-10 seconds every 15 minutes, the average power consumption remains very low, suitable for battery operation.

## Troubleshooting

### Module Not Responding

If the 4G module doesn't respond to AT commands:
1. Check UART connections (TX/RX may be swapped)
2. Verify power supply voltage and current capacity
3. Increase `AIR780E_STARTUP_DELAY_MS` to allow more boot time
4. Check the baud rate matches the Air780E configuration

### Network Registration Fails

If `AT+CREG?` doesn't show network registration:
1. Ensure SIM card is properly inserted
2. Check signal strength with `AT+CSQ`
3. Verify antenna connection
4. Allow more time for network registration

### Data Not Transmitted

If data transmission fails:
1. Check network connectivity
2. Verify Windguru station credentials (UID and password)
3. Check that the URL is properly formatted (enable debug serial output)
4. Verify the HTTP commands are working with simple AT+HTTPINIT test
5. Check Windguru station status on the website
6. Ensure the 4G module has internet connectivity (test with AT+CIPSTART)

## Examples

### Windguru Integration (Default)

The default example transmits wind data to Windguru API:

```cpp
void transmitWindData()
{
  // Wake up module
  wakeup4GModule();
  
  // Check module status
  if (!sendATCommand("AT", "OK", AIR780E_COMMAND_TIMEOUT_MS)) {
    return;
  }
  
  // Generate authentication
  char salt[32];
  snprintf(salt, sizeof(salt), "%lu", millis());
  
  char hashInput[256];
  snprintf(hashInput, sizeof(hashInput), "%s%s%s", 
           salt, WINDGURU_STATION_UID, WINDGURU_STATION_PASSWORD);
  
  char md5Output[33];
  md5Hash(hashInput, md5Output);
  
  // Convert speed to knots
  float windSpeedKnots = kphToKnots(latestWind.speed);
  
  // Build Windguru URL
  char urlBuffer[384];
  snprintf(urlBuffer, sizeof(urlBuffer),
           "http://www.windguru.cz/upload/api.php?uid=%s&salt=%s&hash=%s&wind_avg=%.1f&wind_direction=%d",
           WINDGURU_STATION_UID, salt, md5Output, windSpeedKnots, latestWind.direction);
  
  // Send HTTP GET request
  sendATCommand("AT+HTTPINIT", "OK", AIR780E_COMMAND_TIMEOUT_MS);
  // ... (continue with HTTP commands)
  
  sleep4GModule();
}
```

### Basic Wind Data Logger

```cpp
void transmitWindData()
{
  wakeup4GModule();
  
  // Send simple data string
  Serial4G.print("Wind: ");
  Serial4G.print(latestWind.speed);
  Serial4G.print(" km/h at ");
  Serial4G.println(latestWind.direction);
  
  sleep4GModule();
}
```

### JSON Format

```cpp
void transmitWindData()
{
  wakeup4GModule();
  
  // Create JSON
  Serial4G.print("{\"speed\":");
  Serial4G.print(latestWind.speed);
  Serial4G.print(",\"dir\":");
  Serial4G.print(latestWind.direction);
  Serial4G.println("}");
  
  sleep4GModule();
}
```

## References

- [Windguru Stations](https://stations.windguru.cz/) - Register your station
- [Windguru API Documentation](https://stations.windguru.cz/upload_api.php) - API specification
- [Luat Air780E Documentation](https://doc.openluat.com/)
- [AT Command Set](https://doc.openluat.com/wiki/37?wiki_page_id=4653)
- [WindNerd Core Documentation](../docs/PROGRAM.md)

## License

This example is licensed under the BSD 3-Clause License, same as the WindNerd Core library.
