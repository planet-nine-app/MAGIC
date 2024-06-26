import http from 'http';
import { Server } from 'socket.io';

const server = http.createServer();
const io = new Server(server);
const obs = new OBSWebSocket();

io.on('connection', client => {
  client.on('event', data => { /* … */ });
  obs.connect('ws://127.0.0.1:4455')
   .then(foo => {
     obs.call('BroadcastCustomEvent', {
       eventData: {
         bar: 'baz' // Put custom data here
       }
     });
   });
   client.on('disconnect', () => { /* noop for now */ });
});

io.listen(3001);


import OBSWebSocket from 'obs-websocket-js';

obs.on('CustomEvent', (data) => {
  io.emit('obs', data); // Just pass through the spell for now
});


