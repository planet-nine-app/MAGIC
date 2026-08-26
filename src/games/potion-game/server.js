import express from 'express';
import path from 'path';
import { fileURLToPath } from 'url';
import sessionless from 'sessionless-node';
import gateway from 'magic-gateway-js';
import keysHelper from './src/keys.js';
import buildSpellbook from './src/spellbook.js';

// "E" in README-DEV.md's demo (Game Client) - the last gateway hop before
// fount, and where the "effect" actually becomes visible: a thrown potion
// refills the player's HP in a tiny stand-in battle scene (open
// http://localhost:PORT in a browser).

const __dirname = path.dirname(fileURLToPath(import.meta.url));

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
// BATTLE STATE + SSE (same "server pushes, browser reacts instantly" shape
// as ../../embedded/espnow-demo-server/)
// ============================================================================

let hp = Math.round(config.MAX_HP * 0.35); // start mid-boss-fight, not full HP
const sseClients = new Set();

const broadcast = (event, data) => {
  const payload = `event: ${event}\ndata: ${JSON.stringify(data)}\n\n`;
  for (const client of sseClients) client.write(payload);
};

// ============================================================================
// GATEWAY
// ============================================================================

const spellbook = buildSpellbook(config);

const onSuccess = (req, res, result) => {
  fountUser.ordinal += 1;
  persisted.ordinal = fountUser.ordinal;
  keysHelper.save(config.KEYS_FILE, persisted);

  if (result.success) {
    hp = Math.min(config.MAX_HP, hp + config.POTION_HEAL);
    console.log(`✨ Potion thrown - HP ${hp}/${config.MAX_HP}`);
    broadcast('potion', { hp, maxHp: config.MAX_HP });
  }

  res.send(result);
};

const app = express();
app.use(express.json());
app.use(express.static(path.join(__dirname, 'public')));

gateway.expressApp(app, fountUser, spellbook, config.STOP_NAME, sessionless, undefined, onSuccess);

app.get('/state', (req, res) => {
  res.send({ hp, maxHp: config.MAX_HP });
});

app.get('/events', (req, res) => {
  res.set({ 'Content-Type': 'text/event-stream', 'Cache-Control': 'no-cache', Connection: 'keep-alive' });
  res.flushHeaders();
  res.write(': connected\n\n');
  sseClients.add(res);
  req.on('close', () => sseClients.delete(res));
});

app.listen(config.PORT, () => {
  console.log(`\npotion-game listening on http://localhost:${config.PORT}`);
  console.log(`Open http://localhost:${config.PORT} to watch the battle scene.\n`);
});
