const HID = require('node-hid');

const RAW_HID_USAGE_PAGE = 0xFFAB;
const RAW_HID_USAGE = 0xAB;
const RAW_HID_EP_SIZE = 64;

const COMMAND_RECALIBRATE = 4;
const COMMAND_ANALOG_INFO = 5;
const COMMAND_SET_ACTUATION_MAP = 131;

const KEY = 16;
const CALIBRATION_SECONDS = 10;
const CAPTURE_SECONDS = 60;

function findRawHidDevice(vid, pid) {
  const devs = HID.devices(vid, pid).filter(
    d => d.usagePage === RAW_HID_USAGE_PAGE && d.usage === RAW_HID_USAGE
  );
  if (devs.length === 0) return null;
  return devs[0];
}

function sendCommand(device, commandId, payload) {
  const report = Buffer.alloc(RAW_HID_EP_SIZE, 0);
  report[0] = commandId;
  if (payload) payload.copy(report, 1);
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

async function sleep(ms) {
  return new Promise(r => setTimeout(r, ms));
}

async function readAnalogInfo(device, key) {
  const response = await sendCommandAndReadResponse(device, COMMAND_ANALOG_INFO, Buffer.from([key]));
  return {
    adc: response.readUInt16LE(1),
    distance: response[3],
  };
}

async function recalibrate(device) {
  await sendCommandAndReadResponse(device, COMMAND_RECALIBRATE, Buffer.alloc(0));
  // Recalibrate takes ~500ms; wait for it to complete
  await sleep(600);
}

async function setActuationMap(device, key, actuationPoint, rtDown, rtUp, continuous = false, profile = 0) {
  const payload = Buffer.from([
    profile, key, 1,
    actuationPoint, rtDown, rtUp, continuous ? 1 : 0,
  ]);
  await sendCommandAndReadResponse(device, COMMAND_SET_ACTUATION_MAP, payload);
}

async function main() {
  const devInfo = findRawHidDevice(0x0108, 0x0111);
  if (!devInfo) {
    console.error('Raw HID device not found');
    process.exit(1);
  }
  const device = new HID.HID(devInfo.path);

  console.error(`Opened ${devInfo.product} (${devInfo.vendorId.toString(16).padStart(4,'0')}:${devInfo.productId.toString(16).padStart(4,'0')})`);

  // Set actuation map
  await setActuationMap(device, KEY, 128, 20, 20, false);
  console.error('Set actuation map: ap=128 rt_down=20 rt_up=20 continuous=false');

  // Repeated recalibration window
  console.error(`\n>>> Please press and release the j key once within the next ${CALIBRATION_SECONDS} seconds <<<`);
  const start = Date.now();
  let bestMin = 0;
  let bestMax = 0;
  while (Date.now() - start < CALIBRATION_SECONDS * 1000) {
    await recalibrate(device);
    const info = await readAnalogInfo(device, KEY);
    if (info.distance < bestMin) bestMin = info.distance;
    if (info.distance > bestMax) bestMax = info.distance;
    console.error(`  recalibrate done: adc=${info.adc} dist=${info.distance} (range ${bestMin}..${bestMax})`);
  }
  console.error('Calibration window ended.');

  if (bestMax - bestMin < 50) {
    console.error('WARNING: distance range is small; key may not have been pressed during calibration.');
  }

  // Capture
  console.error('\nStarting 60-second capture. Please press the j key multiple times.');
  console.log('timestamp_ms,adc_filtered,distance');
  const captureStart = process.hrtime.bigint();
  const durationNs = BigInt(Math.floor(CAPTURE_SECONDS * 1e9));
  const intervalNs = BigInt(Math.floor(0.001 * 1e9));
  let nextSample = captureStart;

  while (process.hrtime.bigint() - captureStart < durationNs) {
    const now = process.hrtime.bigint();
    if (now < nextSample) {
      const waitMs = Number(nextSample - now) / 1e6;
      await sleep(Math.max(0, waitMs));
      continue;
    }
    nextSample += intervalNs;

    try {
      const info = await readAnalogInfo(device, KEY);
      const t = Number(process.hrtime.bigint() - captureStart) / 1e6;
      console.log(`${t.toFixed(3)},${info.adc},${info.distance}`);
    } catch (e) {
      console.error(`# read error: ${e.message}`);
    }
  }

  device.close();
  console.error('Capture done.');
}

main().catch(e => {
  console.error(e);
  process.exit(1);
});
