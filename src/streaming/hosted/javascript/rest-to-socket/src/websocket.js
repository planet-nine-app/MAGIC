import WebSocket, { WebSocketServer } from 'ws';

const wss = new WebSocketServer({ port: 4001 });

let server;

wss.on('connection', function connection(ws) {
  ws.on('error', console.error);

  server = ws;
});

export const ws = server;

export const send = (eventName, eventData) => {
  if(!server) {
    console.warn('no server');
  }
  server.send(eventName, eventData);
};
