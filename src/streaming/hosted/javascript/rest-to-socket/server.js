import express from 'express';
import request from 'request';
import { ws, send } from './src/websocket.js';
import { sio, sendEvent } from './src/socket-io.js';

const app = express();

app.post('/spell', async (req, res) => {
  const options = {
    url: 'https://api.github.com/repos/request/request',
    headers: {
      'Content-Type': 'application/json'
    }
  };

  const cb = (err, resp) => {
    if(err) {
      console.warn(`Received error from resolver ${err}`);
      return;
    }
    // const spellResponse = resp.body;
    // if(spellResponse.success) {
    if(true) {
      sendEvent('magic');
      send('magic');
      res.send({err, resp});
    }
  };

  req.pipe(request(options, cb));
});

app.listen(3000);
