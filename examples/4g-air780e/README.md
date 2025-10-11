# 4G Air780E Integration Example - Windguru API

This example demonstrates how to integrate the WindNerd Core with a Luat Air780E 4G module for remote wind data transmission to Windguru.

## Features

- Periodic wake-up of 4G module every 15 minutes
- Automatic wind speed and direction data transmission to Windguru API
- Power-efficient sleep mode between transmissions
- MD5 hash authentication for secure data upload
- Automatic unit conversion (km/h to knots)
- AT command-based HTTP communication protocol

## Hardware Requirements

- WindNerd Core board (STM32G031F8Px)
- Luat Air780E 4G module
- SIM card with data plan
- Power supply (3.3V or 5V, capable of 500mA+ for 4G module)
- Connecting wires

## Quick Start

### 1. Register on Windguru

1. Visit [Windguru Stations](https://stations.windguru.cz/)
2. Create an account (if you don't have one)
3. Register a new weather station
4. Note down your **Station UID** and **Password**

### 2. Configure the Firmware

Edit the configuration in `4g-air780e.ino`:

```cpp
#define WINDGURU_STATION_UID "YOUR_STATION_UID"       // Replace with your UID
#define WINDGURU_STATION_PASSWORD "YOUR_PASSWORD"     // Replace with your password
```

### 3. Connect the Hardware

   - Connect PA0 to Air780E PWRKEY
   - Connect TX2 to Air780E RXD
   - Connect RX2 to Air780E TXD
   - Connect GND to GND
   - Connect power to VBAT

### 4. Upload the Sketch

   - Open `4g-air780e.ino` in Arduino IDE
   - Select board: Generic STM32G031F8Px
   - Upload to WindNerd Core

### 5. Monitor Operation

   - Open Serial Monitor at 115200 baud on USART1
   - Watch for wind data updates and Windguru transmission logs
   - Check your Windguru station dashboard to see the data

## Configuration

Default settings:
- Wake-up interval: 15 minutes
- UART baud rate: 115200
- Wind unit: km/h (automatically converted to knots for Windguru)
- Power control pin: PA0
- API endpoint: http://www.windguru.cz/upload/api.php

**Important:** You must configure your Windguru station credentials before use!

See the [full documentation](../../docs/4G-INTEGRATION.md) for customization options.

## Data Format

The firmware transmits data to Windguru using HTTP GET requests with these parameters:

- **uid**: Your station unique identifier
- **salt**: Random timestamp for security
- **hash**: MD5(salt + uid + password) for authentication
- **wind_avg**: Wind speed in knots (converted from km/h)
- **wind_direction**: Wind direction in degrees (0 = North)

Example URL:
```
http://www.windguru.cz/upload/api.php?uid=mystation&salt=1234567890&hash=abc123def456&wind_avg=6.7&wind_direction=270
```

This transmits: 6.7 knots (12.4 km/h) wind from west (270°)

## Power Consumption

- Sleep mode: ~2-3 mA average
- Active transmission: ~100-300 mA for 5-10 seconds every 15 minutes
- Suitable for battery-powered applications

## Documentation

For detailed information, see:
- [Complete 4G Integration Guide](../../docs/4G-INTEGRATION.md)
- [How to Program the WindNerd Core](../../docs/PROGRAM.md)
- [How to Read Serial Data](../../docs/READSERIAL.md)

## Support

For issues and questions:
- GitHub Issues: https://github.com/Glider95/Windnerd-Core/issues
- WindNerd Website: https://windnerd.net

## License

BSD 3-Clause License - see LICENSE.txt in the root directory
