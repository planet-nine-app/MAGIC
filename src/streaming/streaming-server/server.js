import express from 'express';
import sessionless from 'sessionless-node';
import gateway from 'magic-gateway-js';
import keysHelper from './src/keys.js';
import buildSpellbook from './src/spellbook.js';

// "C" in README-DEV.md's demo (Streaming Server) - a pure pass-through
// gateway. Per the README: "the streaming clients and server don't need to
// *do* anything other than pass the request and the effect along between
// the caster and resolver" - so unlike "D" (../obs-gateway/), this hop has
// no side effect of its own, just forward + relay the response.

let config;
try {
  config = (await import('./config.js')).default;
} catch (err) {
  console.error('Missing config.js - copy config.js.example to config.js and fill in your values.');
  process.exit(1);
}

// ============================================================================
// IDENTITY
// ============================================================================

let persisted = keysHelper.load(config.KEYS_FILE);

const saveKeys = (generated) => {
  if (!persisted) {
    persisted = { privateKey: generated.privateKey, pubKey: generated.pubKey };
    keysHelper.save(config.KEYS_FILE, persisted);
  }
};
const getKeys = () => persisted;

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
    process.exit(1);
  }

  persisted.uuid = body.uuid;
  persisted.ordinal = body.ordinal ?? 0;
  keysHelper.save(config.KEYS_FILE, persisted);
  console.log(`Registered with fount: uuid=${persisted.uuid}`);
} else {
  console.log(`Already registered with fount: uuid=${persisted.uuid}`);
}

const fountUser = { uuid: persisted.uuid, ordinal: persisted.ordinal || 0 };

// ============================================================================
// GATEWAY
// ============================================================================

const spellbook = buildSpellbook(config);

const onSuccess = (req, res, result) => {
  fountUser.ordinal += 1;
  persisted.ordinal = fountUser.ordinal;
  keysHelper.save(config.KEYS_FILE, persisted);

  res.send(result);
};

const app = express();
app.use(express.json());

gateway.expressApp(app, fountUser, spellbook, config.STOP_NAME, sessionless, undefined, onSuccess);

app.get('/health', (req, res) => {
  res.send({ ok: true, registered: !!persisted.uuid });
});

app.listen(config.PORT, () => {
  console.log(`\nstreaming-server listening on http://localhost:${config.PORT}`);
  console.log(`Forwards ${config.SPELL_NAME} to ${config.NEXT_DESTINATIONS[0]?.stopURL}\n`);
});
