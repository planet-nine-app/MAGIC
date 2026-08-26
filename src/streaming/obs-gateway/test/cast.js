// Standalone test caster - registers a fresh throwaway identity with fount
// and casts obsCast at this gateway, so the full relay (caster -> obs-gateway
// -> fount -> back through obs-gateway -> OBS effect) can be exercised
// without any embedded hardware. Mirrors the exact signed-message contract
// validated by sharon/tests/magic/wand-cast-hosted.js (the ground-truth
// reference the ESP-NOW wand/target firmware was built against) - same
// message shapes, just pointed at a local gateway/fount instead of the
// hosted one.
//
// Usage: node test/cast.js [effect]   (effect defaults to "potion")

import sessionless from 'sessionless-node';

let config;
try {
  config = (await import('../config.js')).default;
} catch (err) {
  console.error('Missing config.js - copy config.js.example to config.js first.');
  process.exit(1);
}

const SPELL_NAME = config.SPELL_NAME;
const SPELL_COST = config.SPELL_COST;
const effect = process.argv[2] || 'potion';

let caster = null;
await sessionless.generateKeys(
  (keys) => { caster = keys; },
  () => caster
);

const register = async () => {
  const timestamp = Date.now().toString();
  const message = timestamp + caster.pubKey;
  const signature = await sessionless.sign(message);

  const res = await fetch(`${config.FOUNT_BASE}/user/create`, {
    method: 'PUT',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ timestamp, pubKey: caster.pubKey, signature })
  });
  const body = await res.json();
  if (!res.ok || !body.uuid) {
    throw new Error(`Caster registration failed (${res.status}): ${JSON.stringify(body)}`);
  }
  console.log(`✅ Test caster registered: uuid=${body.uuid} mp=${body.mp}`);
  return body;
};

const cast = async (casterUser) => {
  const timestamp = Date.now().toString();
  const totalCost = SPELL_COST;
  const mp = true;
  const ordinal = Date.now();

  // Must match fount's resolve() reconstruction exactly:
  // spell.timestamp + spell.spell + spell.casterUUID + spell.totalCost + spell.mp + spell.ordinal
  const message = timestamp + SPELL_NAME + casterUser.uuid + totalCost + mp + ordinal;
  const casterSignature = await sessionless.sign(message);

  const spell = {
    timestamp,
    spell: SPELL_NAME,
    casterUUID: casterUser.uuid,
    totalCost,
    mp,
    ordinal,
    casterSignature,
    gateways: [],
    effect // spell-specific optional field - which OBS effect to trigger, see config.js EFFECTS
  };

  const gatewayURL = `http://localhost:${config.PORT}/magic/spell/${SPELL_NAME}`;
  console.log(`📤 Casting ${SPELL_NAME} (effect="${effect}") against ${gatewayURL}...`);

  const res = await fetch(gatewayURL, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(spell)
  });

  const body = await res.json().catch((err) => ({ parseError: err.message }));
  console.log(`⬅️  HTTP ${res.status}:`, JSON.stringify(body, null, 2));
  return { status: res.status, body };
};

const main = async () => {
  console.log('🔮 obs-gateway test cast\n');
  const casterUser = await register();
  const result = await cast(casterUser);

  if (result.status === 200 && result.body.success) {
    console.log('\n✅ obsCast resolved successfully.');
    if (result.body.obsGateway?.triggered) {
      console.log('✨ OBS effect triggered.');
    } else {
      console.log(`ℹ️  OBS effect not triggered: ${result.body.obsGateway?.reason ?? 'unknown'}`);
    }
  } else {
    console.log('\n❌ obsCast did not resolve - see response above.');
    process.exitCode = 1;
  }
};

main().catch((err) => {
  console.error('\n💥 Script failed:', err);
  process.exitCode = 1;
});
