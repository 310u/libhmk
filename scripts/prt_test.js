const HID = require('node-hid');

const RAW_HID_USAGE_PAGE = 0xFFAB;
const RAW_HID_USAGE = 0xAB;
const RAW_HID_EP_SIZE = 64;

const COMMAND_ANALOG_INFO = 5;
const COMMAND_SET_ACTUATION_MAP = 131;

function findRawHidDevice(vid, pid) {
  const devs = HID.devices(vid, pid).filter(
    d => d.usagePage === RAW_HID_USAGE_PAGE && d.usage === RAW_HID_USAGE
  );
  if (devs.length === 0) return null;
  if (devs.length > 1) {
    console.error('Multiple Raw HID interfaces found; using the first one.');
  }
  return devs[0];
}

function sendCommand(device, commandId, payload) {
  const report = Buffer.alloc(RAW_HID_EP_SIZE, 0);
  report[0] = commandId;
  if (payload) payload.copy(report, 1);
  // Prepend report ID 0 for Windows
  device.write([0, ...report]);
}

function readResponse(device, expectedCommandId, timeoutMs = 1000) {
  return new Promise((resolve, reject) => {
    const start = Date.now();
    const tryRead = () => {
      const data = device.readTimeout(timeoutMs);
      if (data && data.length >= RAW_HID_EP_SIZE) {
        if (data[0] !== expectedCommandId) {
          return reject(new Error(`Unexpected response command id: ${data[0]} (expected ${expectedCommandId})`));
        }
        return resolve(Buffer.from(data));
      }
      if (Date.now() - start >= timeoutMs) {
        return reject(new Error('No response from device (timeout)'));
      }
      setTimeout(tryRead, 1);
    };
    tryRead();
  });
}

async function sendCommandAndReadResponse(device, commandId, payload, timeoutMs = 1000) {
  sendCommand(device, commandId, payload);
  return readResponse(device, commandId, timeoutMs);
}

async function setActuationMap(device, key, actuationPoint, rtDown, rtUp, continuous = false, profile = 0) {
  const payload = Buffer.from([
    profile, key, 1, // profile, offset, len=1
    actuationPoint, rtDown, rtUp, continuous ? 1 : 0,
  ]);
  await sendCommandAndReadResponse(device, COMMAND_SET_ACTUATION_MAP, payload);
}

async function readAnalogInfo(device, key) {
  const response = await sendCommandAndReadResponse(device, COMMAND_ANALOG_INFO, Buffer.from([key]));
  // response[0] = command_id
  // response[1..2] = adc_value (uint16 LE)
  // response[3] = distance
  const adcValue = response.readUInt16LE(1);
  const distance = response[3];
  return { adcValue, distance };
}

async function sleep(ms) {
  return new Promise(r => setTimeout(r, ms));
}

async function main() {
  const args = parseArgs(process.argv.slice(2));

  const devInfo = findRawHidDevice(args.vid, args.pid);
  if (!devInfo) {
    console.error('Raw HID interface not found. Is the keyboard connected and enumerated?');
    process.exit(1);
  }

  console.error(`Opened ${devInfo.manufacturer} ${devInfo.product} (${devInfo.vendorId.toString(16).padStart(4, '0')}:${devInfo.productId.toString(16).padStart(4, '0')})`);

  const device = new HID.HID(devInfo.path);

  if (args.rtDown !== null || args.rtUp !== null) {
    const rtDown = args.rtDown !== null ? args.rtDown : 10;
    const rtUp = args.rtUp !== null ? args.rtUp : 10;
    await setActuationMap(device, args.key, args.actuationPoint, rtDown, rtUp, args.continuous);
    console.error(`Set actuation map for key ${args.key}: ap=${args.actuationPoint} rt_down=${rtDown} rt_up=${rtUp} continuous=${args.continuous}`);
  }

  if (args.waitForPress) {
    console.error('Waiting for key press...');
    while (true) {
      const { distance } = await readAnalogInfo(device, args.key);
      if (distance > 0) break;
      await sleep(10);
    }
    console.error('Key pressed. Starting capture.');
  }

  console.log('timestamp_ms,adc_filtered,distance');
  const start = process.hrtime.bigint();
  const durationNs = BigInt(Math.floor(args.duration * 1e9));
  const intervalNs = BigInt(Math.floor(args.interval * 1e9));
  let nextSample = start;

  while (process.hrtime.bigint() - start < durationNs) {
    const now = process.hrtime.bigint();
    if (now < nextSample) {
      const waitMs = Number(nextSample - now) / 1e6;
      await sleep(Math.max(0, waitMs));
      continue;
    }
    nextSample += intervalNs;

    try {
      const { adcValue, distance } = await readAnalogInfo(device, args.key);
      const timestampMs = Number(process.hrtime.bigint() - start) / 1e6;
      console.log(`${timestampMs.toFixed(3)},${adcValue},${distance}`);
    } catch (e) {
      console.error(`# read error: ${e.message}`);
    }
  }

  device.close();
}

function parseArgs(argv) {
  const args = {
    key: 0,
    duration: 5.0,
    interval: 0.001,
    rtDown: null,
    rtUp: null,
    actuationPoint: 128,
    continuous: false,
    vid: 0x0108,
    pid: 0x0111,
    waitForPress: false,
  };

  for (let i = 0; i < argv.length; i++) {
    switch (argv[i]) {
      case '--key': args.key = parseInt(argv[++i]); break;
      case '--duration': args.duration = parseFloat(argv[++i]); break;
      case '--interval': args.interval = parseFloat(argv[++i]); break;
      case '--rt-down': args.rtDown = parseInt(argv[++i]); break;
      case '--rt-up': args.rtUp = parseInt(argv[++i]); break;
      case '--actuation-point': args.actuationPoint = parseInt(argv[++i]); break;
      case '--continuous': args.continuous = true; break;
      case '--vid': args.vid = parseInt(argv[++i]); break;
      case '--pid': args.pid = parseInt(argv[++i]); break;
      case '--wait-for-press': args.waitForPress = true; break;
      default:
        console.error(`Unknown argument: ${argv[i]}`);
        process.exit(1);
    }
  }
  return args;
}

main().catch(e => {
  console.error(e);
  process.exit(1);
});
