import http from 'http';
import { Server } from 'socket.io';

const server = http.createServer();
const io = new Server(server);

io.on('connection', client => {
  // No ops for now.
  client.on('event', data => { /* … */ });
  client.on('disconnect', () => { /* … */ });
});

io.listen(4002);

export const sio = io;

export const sendEvent = (eventName, eventData) => {
  io.emit(eventName, eventData);
};

