# 4G Air780E Integration Example

This example demonstrates how to integrate the WindNerd Core with a Luat Air780E 4G module for remote wind data transmission.

## Features

- Periodic wake-up of 4G module every 15 minutes
- Automatic wind speed and direction data transmission
- Power-efficient sleep mode between transmissions
- AT command-based communication protocol
- Configurable data transmission format

## Hardware Requirements

- WindNerd Core board (STM32G031F8Px)
- Luat Air780E 4G module
- SIM card with data plan
- Power supply (3.3V or 5V, capable of 500mA+ for 4G module)
- Connecting wires

## Quick Start

1. **Connect the Hardware**
   - Connect PA0 to Air780E PWRKEY
   - Connect TX2 to Air780E RXD
   - Connect RX2 to Air780E TXD
   - Connect GND to GND
   - Connect power to VBAT

2. **Upload the Sketch**
   - Open `4g-air780e.ino` in Arduino IDE
   - Select board: Generic STM32G031F8Px
   - Upload to WindNerd Core

3. **Monitor Operation**
   - Open Serial Monitor at 115200 baud on USART1
   - Watch for wind data updates and 4G transmission logs

## Configuration

Default settings:
- Wake-up interval: 15 minutes
- UART baud rate: 115200
- Wind unit: km/h
- Power control pin: PA0

See the [full documentation](../../docs/4G-INTEGRATION.md) for customization options.

## Data Format

Transmitted data format:
```
WIND,<speed>,<direction>,<timestamp>
```

Example:
```
WIND,12.5,270,123456
```

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
