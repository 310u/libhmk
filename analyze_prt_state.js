const fs = require('fs');

const CSV = process.argv[2] || 'prt_final2_j.csv';
const ACTUATION_POINT = 128;
const RT_DOWN = 20;
const RT_UP = 20;
const RESET_POINT = ACTUATION_POINT;
const DECEL_THRESHOLD = 15;

const buf = fs.readFileSync(CSV);
const text = buf.toString('utf16le').replace(/^\uFEFF/, '');
const lines = text.trim().split(/\r?\n/);
const header = lines.shift();
const rows = lines.map(l => {
  const parts = l.split(',');
  return {
    t: parseFloat(parts[0]),
    adc: parseInt(parts[1], 10),
    dist: parseInt(parts[2], 10),
  };
});

console.log(`Rows: ${rows.length}`);
console.log(`Duration: ${(rows[rows.length - 1].t - rows[0].t).toFixed(1)} ms`);
console.log(`Rest ADC: ${Math.max(...rows.map(r => r.adc))}, Bottom ADC: ${Math.min(...rows.map(r => r.adc))}`);
console.log(`Rest distance: ${Math.max(...rows.map(r => r.dist))}, Bottom distance: ${Math.min(...rows.map(r => r.dist))}`);

// Compute velocity and acceleration
for (let i = 1; i < rows.length; i++) {
  rows[i].vel = rows[i].adc - rows[i - 1].adc;
  rows[i].acc = rows[i].vel - (rows[i - 1].vel || 0);
}

function simulate(prtEnabled) {
  let keyDir = 'INACTIVE';
  let isPressed = false;
  let extremum = rows[0].dist;
  let prevVelocity = 0;
  const events = [];

  for (let i = 1; i < rows.length; i++) {
    const r = rows[i];
    const prev = rows[i - 1];
    const velocity = r.adc - prev.adc;
    const acceleration = velocity - prevVelocity;
    prevVelocity = velocity;

    const prtTrigger = prtEnabled && isPressed && keyDir === 'DOWN' &&
      ((velocity > 0 && acceleration < -DECEL_THRESHOLD) ||
       (velocity < 0 && acceleration > DECEL_THRESHOLD));
    const effectiveRtUp = prtTrigger ? 1 : RT_UP;

    const oldPressed = isPressed;
    let releaseReason = null;

    switch (keyDir) {
      case 'INACTIVE':
        if (r.dist > ACTUATION_POINT) {
          keyDir = 'DOWN';
          isPressed = true;
          extremum = r.dist;
          events.push({ t: r.t, type: 'press', reason: 'actuation', dist: r.dist, adc: r.adc, vel: velocity, acc: acceleration });
        }
        break;
      case 'DOWN':
        if (r.dist <= RESET_POINT) {
          keyDir = 'INACTIVE';
          isPressed = false;
          extremum = r.dist;
          releaseReason = 'reset';
        } else if (r.dist + effectiveRtUp < extremum) {
          keyDir = 'UP';
          isPressed = false;
          extremum = r.dist;
          releaseReason = prtTrigger ? 'prt' : 'rt';
        } else if (r.dist > extremum) {
          extremum = r.dist;
        }
        break;
      case 'UP':
        if (r.dist <= RESET_POINT) {
          keyDir = 'INACTIVE';
          isPressed = false;
          extremum = r.dist;
        } else if (extremum + RT_DOWN < r.dist) {
          keyDir = 'DOWN';
          isPressed = true;
          extremum = r.dist;
          events.push({ t: r.t, type: 'press', reason: 'rt', dist: r.dist, adc: r.adc, vel: velocity, acc: acceleration });
        } else if (r.dist < extremum) {
          extremum = r.dist;
        }
        break;
    }

    if (oldPressed && !isPressed && releaseReason) {
      events.push({ t: r.t, type: 'release', reason: releaseReason, dist: r.dist, adc: r.adc, vel: velocity, acc: acceleration });
    }
  }

  return events;
}

const eventsPrt = simulate(true);
const eventsNoPrt = simulate(false);

console.log('\n=== With PRT ===');
console.log(`Total events: ${eventsPrt.length}`);
const pressesPrt = eventsPrt.filter(e => e.type === 'press');
const releasesPrt = eventsPrt.filter(e => e.type === 'release');
console.log(`Presses: ${pressesPrt.length}`);
console.log(`Releases: ${releasesPrt.length}`);
const prtReleases = releasesPrt.filter(e => e.reason === 'prt');
const rtReleases = releasesPrt.filter(e => e.reason === 'rt');
const resetReleases = releasesPrt.filter(e => e.reason === 'reset');
// Count triggers separately: PRT condition fired while DOWN but may not cause immediate release
let prtTriggers = 0;
let lastKeyDir = 'INACTIVE';
let prevV = 0;
for (let i = 1; i < rows.length; i++) {
  const r = rows[i];
  const prev = rows[i - 1];
  const v = r.adc - prev.adc;
  const a = v - prevV;
  prevV = v;
  // Approximate state direction based on distance trend
  if (r.dist > ACTUATION_POINT && lastKeyDir !== 'DOWN') lastKeyDir = 'DOWN';
  if (r.dist < rows[i-1].dist && lastKeyDir === 'DOWN') lastKeyDir = 'UP';
  if (r.dist <= RESET_POINT) lastKeyDir = 'INACTIVE';
  if (lastKeyDir === 'DOWN' && ((v > 0 && a < -DECEL_THRESHOLD) || (v < 0 && a > DECEL_THRESHOLD))) {
    prtTriggers++;
  }
}
console.log(`PRT releases: ${prtReleases.length}`);
console.log(`PRT triggers (approx): ${prtTriggers}`);
console.log(`RT releases: ${rtReleases.length}`);
console.log(`Reset releases: ${resetReleases.length}`);
if (prtReleases.length > 0) {
  console.log('PRT release details (first 10):');
  for (const e of prtReleases.slice(0, 10)) {
    console.log(`  ${e.t.toFixed(3)} ms: dist=${e.dist} adc=${e.adc} vel=${e.vel} acc=${e.acc}`);
  }
}

console.log('\n=== Without PRT ===');
console.log(`Total events: ${eventsNoPrt.length}`);
const pressesNoPrt = eventsNoPrt.filter(e => e.type === 'press');
const releasesNoPrt = eventsNoPrt.filter(e => e.type === 'release');
console.log(`Presses: ${pressesNoPrt.length}`);
console.log(`Releases: ${releasesNoPrt.length}`);

// Compare release points: match PRT release to nearest no-PRT release
console.log('\n=== Release comparison ===');
const noPrtReleaseList = eventsNoPrt.filter(e => e.type === 'release');
let compared = 0;
for (const p of prtReleases) {
  const nearest = noPrtReleaseList.reduce((best, np) => {
    const d = Math.abs(np.t - p.t);
    return d < best.d ? { np, d } : best;
  }, { np: null, d: Infinity }).np;
  if (nearest) {
    const dt = p.t - nearest.t;
    const dd = p.dist - nearest.dist;
    console.log(`  ${p.t.toFixed(3)} ms: PRT dist=${p.dist} vs no-PRT dist=${nearest.dist} (${nearest.reason}) Δt=${dt.toFixed(3)}ms Δdist=${dd}`);
    compared++;
  }
}
console.log(`Compared ${compared} PRT releases to no-PRT releases`);

// Print first 30 press/release events with PRT
console.log('\n=== First 30 PRT events ===');
for (const e of eventsPrt.slice(0, 30)) {
  console.log(`  ${e.t.toFixed(3)} ms: ${e.type} (${e.reason}) dist=${e.dist} adc=${e.adc} vel=${e.vel} acc=${e.acc}`);
}
