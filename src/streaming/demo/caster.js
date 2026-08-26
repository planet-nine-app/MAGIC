// "A" (User's Client / Caster) in README-DEV.md's demo:
// A[User's Client<Caster>] -> B[TV<Target>] -> C{Streaming Server<Gateway>} ->
// D[Streamer's Client<Gateway>] -> E[Game Client<Gateway>] -> F{Server<Resolver>}
//
// Registers a fresh throwaway caster identity with fount and casts
// throwPotion at "B" - the tvOS app in ../../appletv/MagicTV/, which relays
// through every other hop in the chain and back. Same signed-message
// contract this whole repo already uses (see
// sharon/tests/magic/wand-cast-hosted.js), just aimed at the TV's local
// gateway listener instead of fount directly.
//
// Usage: node caster.js [tvHost:port]   (defaults to localhost:8787)

import sessionless from 'sessionless-node';

const FOUNT_BASE = process.env.FOUNT_BASE || 'http://localhost:3006';
const tvAddress = process.argv[2] || 'localhost:8787';
const SPELL_NAME = 'throwPotion';
const SPELL_COST = 50;

let caster = null;
await sessionless.generateKeys(
  (keys) => { caster = keys; },
  () => caster
);

const register = async () => {
  const timestamp = Date.now().toString();
  const message = timestamp + caster.pubKey;
  const signature = await sessionless.sign(message);

  const res = await fetch(`${FOUNT_BASE}/user/create`, {
    method: 'PUT',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ timestamp, pubKey: caster.pubKey, signature })
  });
  const body = await res.json();
  if (!res.ok || !body.uuid) {
    throw new Error(`Caster registration failed (${res.status}): ${JSON.stringify(body)}`);
  }
  console.log(`✅ Caster registered: uuid=${body.uuid} mp=${body.mp}`);
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
    effect: 'potion' // read by "D" (../obs-gateway/) to pick which OBS action fires, if OBS is connected - see obs-gateway/README.md
  };

  const tvURL = `http://${tvAddress}/magic/spell/${SPELL_NAME}`;
  console.log(`🧪 Throwing a potion at ${tvURL}...`);

  const res = await fetch(tvURL, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(spell)
  });

  const body = await res.json().catch((err) => ({ parseError: err.message }));
  console.log(`⬅️  HTTP ${res.status}:`, JSON.stringify(body, null, 2));
  return { status: res.status, body };
};

const main = async () => {
  console.log('🧪 throwPotion - the README-DEV.md demo\n');
  const casterUser = await register();
  const result = await cast(casterUser);

  if (result.status === 200 && result.body.success) {
    console.log('\n✅ The potion made it all the way through: caster -> TV -> streaming server -> streamer\'s client -> game client -> resolver, and back.');
  } else {
    console.log('\n❌ The spell did not resolve - see response above, and check every hop in ../README.md is running.');
    process.exitCode = 1;
  }
};

main().catch((err) => {
  console.error('\n💥 Script failed:', err);
  process.exitCode = 1;
});
