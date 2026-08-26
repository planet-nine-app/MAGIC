// Big-screen display for the ESP-NOW wand demo. The target POSTs here the
// instant a wandCast spell resolves successfully; this pushes a "cast"
// event to any open browser tabs over SSE so the display reacts instantly
// rather than polling.
import express from 'express';
import path from 'path';
import { fileURLToPath } from 'url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const PORT = process.env.PORT || 4747;

const app = express();
app.use(express.json());
app.use(express.static(path.join(__dirname, 'public')));

const sseClients = new Set();

app.get('/events', (req, res) => {
  res.set({
    'Content-Type': 'text/event-stream',
    'Cache-Control': 'no-cache',
    Connection: 'keep-alive'
  });
  res.flushHeaders();
  res.write(': connected\n\n');

  sseClients.add(res);
  console.log(`Display connected (${sseClients.size} open)`);

  req.on('close', () => {
    sseClients.delete(res);
    console.log(`Display disconnected (${sseClients.size} open)`);
  });
});

const broadcast = (event, data) => {
  const payload = `event: ${event}\ndata: ${JSON.stringify(data)}\n\n`;
  for (const client of sseClients) {
    client.write(payload);
  }
};

app.post('/cast', (req, res) => {
  console.log('✨ Cast received - broadcasting to', sseClients.size, 'display(s)');
  broadcast('cast', { at: Date.now() });
  res.sendStatus(200);
});

app.listen(PORT, () => {
  console.log(`espnow-demo-server listening on http://0.0.0.0:${PORT}`);
  console.log(`Open http://localhost:${PORT} on the big screen.`);
});
