# How to read WindNerd Core serial from another device

## From a Computer

![Wiring WindNerd Core to computer using USB-serial-TTL adapter](img/USB-TTL-adapter-wiring.jpg)


### Serial terminal


#### Linux

`minicom -D /dev/ttyUSB0 -b 9600`

The serial device name can also be enumerated as `/dev/ttyUSB1`, `/dev/ttyUSB2` etc if several serial adpaters are present. Some serial adapters can be listed as `/dev/ttyACM0`, `/dev/ttyACM1`...

#### Windows

- Download and install [Putty](https://pbxbook.com/voip/sputty.html)
- Open PuTTY
- Select Serial
- Enter COM1 (or COM2, COM3 etc)
- Set Speed to 9600
- Click Open

#### MacOS

- Install minicom with `brew install minicom`
- Find the serial adapter name with  `ls /dev/cu.* /dev/tty.* 2>/dev/null`
- Run minicom with the adapter name found, eg: `minicom -D /dev/cu.usbserial-xxxx -b 9600`


### NodeJS

```
const { SerialPort, ReadlineParser } = require('serialport');

// Change this to your detected serial port, e.g. "/dev/ttyUSB0" or "/dev/cu.usbserial-1420"
const PORT = '/dev/ttyUSB0';
const BAUD = 9600;

// Open serial port
const port = new SerialPort({ path: PORT, baudRate: BAUD });

// Create a line-based parser (splits on newline)
const parser = port.pipe(new ReadlineParser({ delimiter: '\n' }));

// Listen for parsed lines
parser.on('data', (line) => {

  console.log(line);
  line = line.trim();
  if (!line) return;

  const parts = line.split(',');
  const tag = parts[0];

  if (tag === 'WNI' && parts.length === 3) {
    const speed = parseFloat(parts[1]);
    const dir = parseFloat(parts[2]);
    console.log(`[Instant Wind] speed=${speed.toFixed(1)} km/h, dir=${dir}°`);
  }

  else if (tag === 'WNA' && parts.length === 5) {
    const avgSpeed = parseFloat(parts[1]);
    const avgDir = parseFloat(parts[2]);
    const minSpeed = parseFloat(parts[3]);
    const maxSpeed = parseFloat(parts[4]);
    console.log(`[Wind Report] avg=${avgSpeed.toFixed(1)} km/h, dir=${avgDir}°, min=${minSpeed.toFixed(1)}, max=${maxSpeed.toFixed(1)}`);
  }

  else {
    console.warn('Unknown line:', line);
  }
});

port.on('open', () => {
  console.log(`Serial connected on ${PORT} at ${BAUD} baud`);
});

port.on('error', (err) => {
  console.error('Serial error:', err.message);
});
```

### Python


## From an Arduino dev board

### Hardware Serial


### Software Serial
