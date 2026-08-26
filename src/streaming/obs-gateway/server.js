import express from 'express';
import sessionless from 'sessionless-node';
import gateway from 'magic-gateway-js';
import keysHelper from './src/keys.js';
import buildSpellbook from './src/spellbook.js';
import Effects from './src/effects.js';

let config;
try {
  config = (await import('./config.js')).default;
} catch (err) {
  console.error('Missing config.js - copy config.js.example to config.js and fill in your values.');
  process.exit(1);
}

// ============================================================================
// IDENTITY - load or generate this gateway's own Sessionless keypair, and
// register it with fount if it hasn't been already. File-backed, the
// desktop analog of the NVS storage the embedded gateways use.
// ============================================================================

let persisted = keysHelper.load(config.KEYS_FILE);

const saveKeys = (generated) => {
  if (!persisted) {
    persisted = { privateKey: generated.privateKey, pubKey: generated.pubKey };
    keysHelper.save(config.KEYS_FILE, persisted);
  }
};
const getKeys = () => persisted;

// sessionless-node's generateKeys() always generates fresh key material and
// primes its internal getKeys() singleton with whatever callback is passed
// here - saveKeys above only actually persists it the first time this
// gateway ever runs, so every later boot keeps signing with the same
// identity (and the same fount registration) rather than becoming a new
// user on every restart.
await sessionless.generateKeys(saveKeys, getKeys);

if (!persisted.uuid) {
  console.log('Not yet registered with fount - registering now...');
  const timestamp = Date.now().toString();
  const message = timestamp + persisted.pubKey;
  const signature = await sessionless.sign(message);

  const res = await fetch(`${config.FOUNT_BASE}/user/create`, {
    method: 'PUT',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ timestamp, pubKey: persisted.pubKey, signature })
  });
  const body = await res.json();

  if (!res.ok || !body.uuid) {
    console.error(`Registration with fount failed (${res.status}):`, body);
    console.error(`Is fount running at ${config.FOUNT_BASE}? (see README.md for how to run one locally)`);
    process.exit(1);
  }

  persisted.uuid = body.uuid;
  persisted.ordinal = body.ordinal ?? 0;
  keysHelper.save(config.KEYS_FILE, persisted);
  console.log(`Registered with fount: uuid=${persisted.uuid}`);
} else {
  console.log(`Already registered with fount: uuid=${persisted.uuid}`);
}

// fountUser is a live object, not a snapshot - magic-gateway-js reads
// fountUser.ordinal fresh on every incoming spell (see gatewayForSpell in
// ../server/javascript/magic-adapter/src/magic.js), and onSuccess below
// bumps it after each successful relay so it doesn't sign every gateway
// entry with the same ordinal.
const fountUser = { uuid: persisted.uuid, ordinal: persisted.ordinal || 0 };

// ============================================================================
// OBS
// ============================================================================

const effects = new Effects(config);
await effects.connect(); // best-effort - see src/effects.js

// ============================================================================
// GATEWAY
// ============================================================================

const spellbook = buildSpellbook(config);

const onSuccess = async (req, res, result) => {
  fountUser.ordinal += 1;
  persisted.ordinal = fountUser.ordinal;
  keysHelper.save(config.KEYS_FILE, persisted);

  const effectName = req.body.effect;
  let effectResult = { triggered: false, reason: 'no "effect" field on the cast spell' };

  if (result.success && effectName) {
    effectResult = await effects.trigger(effectName);
  } else if (result.success) {
    console.log('Spell resolved but no "effect" field was given - nothing to trigger in OBS.');
  }

  // Must make it back to the original caster for the spell to complete -
  // see magic-gateway-js's README.
  res.send({ ...result, obsGateway: effectResult });
};

const app = express();
app.use(express.json());

gateway.expressApp(app, fountUser, spellbook, config.STOP_NAME, sessionless, undefined, onSuccess);

app.get('/health', (req, res) => {
  res.send({ ok: true, registered: !!persisted.uuid, obsConnected: effects.connected });
});

app.listen(config.PORT, () => {
  console.log(`\nobs-gateway listening on http://localhost:${config.PORT}`);
  console.log(`Cast a spell at http://localhost:${config.PORT}/magic/spell/${config.SPELL_NAME}`);
  console.log(`Forwards to ${config.NEXT_DESTINATIONS[0]?.stopURL}`);
  console.log(`(standalone test: node test/cast.js)\n`);
});
