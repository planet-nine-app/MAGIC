import expressSession from 'express-session';
import express from 'express';
import superagent from 'superagent';
import cors from 'cors';
import bodyParser from 'body-parser';
import chalk from 'chalk';
import sessionless from 'sessionless-node';
import fs from 'fs';
import { readFile } from 'node:fs/promises';
import path from 'path';
import url from 'url';
import { getUser, saveUser } from './src/persistence/user.js';
import { addRoutes } from './src/demo/demo.js';

const magic = () => {
  const str = 'MAGIC';
  let currentIndex = -1;
  const writer = () => {
    process.stdout.write(chalk.bgCyan(currentIndex > -1 ? str[currentIndex] : ''));
    currentIndex++;
    if(currentIndex < 6) {
      setTimeout(writer, 400);
    }
  };
  setTimeout(writer, 400);
};

const __filename = url.fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const app = express();

/**
 * Uncomment this when debugging what client implementations are sending
 */
//app.use(express.raw({ type: '*/*', limit: '10mb' }));
/*app.use((req) => {
  console.log(req.body.toString('utf-8'));
});*/

app.get('/', (req, res, next) => {
  console.log(req.socket.remoteAddress);
});

app.use(expressSession({
  secret: 'foo bar baz',
  cookie: {
    name: 'don\'t use this name',
    maxAge: 24 * 60 * 60 * 1000,
    httpOnly: true,
    signed: false
  }
}));

let users = {};
let currentPrivateKey = '';
let getKeys = () => {
  return (() => { return {privateKey: currentPrivateKey} })();
};

const webSignature = async (req, message) => {
  currentPrivateKey = req.session.user;
  return await sessionless.sign(message);
};

const handleWebRegistration = async (req, res) => {

  const keys = await sessionless.generateKeys((keys) => {
    keys[keys.publicKey] = keys.privateKey;
  }, getKeys);

  req.session.user = keys.privateKey;

  const uuid = sessionless.generateUUID();
  users[uuid] = keys.publicKey;

  await saveUser(uuid, keys.publicKey);

  res.send({
    uuid,
    welcomeMessage: "Welcome to Sessionless!"
  });
};

app.use(bodyParser.json());

app.post('/register', async (req, res) => {
//console.log(req.body);
  const payload = req.body;
  const signature = payload.signature;

  if(!signature) {
    return handleWebRegistration(req, res);
  }
  
  const pubKey = payload.pubKey; 
  
  const message = JSON.stringify({ 
    pubKey, 
    enteredText: payload.enteredText, 
    timestamp: payload.timestamp 
  });
//console.log(message);
//console.log(signature);
//console.log(pubKey);

const verified = await sessionless.verifySignature(signature, message, pubKey);
//console.log('verified: ' + verified);

  if(await sessionless.verifySignature(signature, message, pubKey)) {
    const uuid = sessionless.generateUUID();
    await saveUser(uuid, pubKey);
    const user = {
      uuid,
      welcomeMessage: "Welcome to this example!"
    };
    console.log(chalk.blue(`\n\nuser registered with uuid: ${uuid}`));
    res.send(user);
  } else {
    console.log(chalk.red('unverified!'));
    res.send({error: 'auth error'});
  }
});

app.post('/cool-stuff', async (req, res) => {
console.log(req.body);
  const payload = req.body;
  const message = JSON.stringify({ uuid: payload.uuid, coolness: payload.coolness, timestamp: payload.timestamp });
console.log(req.body);
  const publicKey = getUser(payload.uuid).pubKey; 
  const signature = payload.signature || (await webSignature(req, message));

console.log(message);
console.log(signature);
console.log(publicKey);

  if(sessionless.verifySignature(signature, message, publicKey)) {
    return res.send({
      doubleCool: 'double cool'
    });
  }
  return res.send({error: 'auth error'});
});

app.post('/pass-through', async (req, res) => {
  try {
  const resolution = await superagent.post(req.body.spell.paths.pop())
    .send(req.body)
    .set('Content-Type', 'application/json')
    .set('Accept', 'application/json');

  magic();

  res.send(resolution.body);
  } catch(err) {
    return res.send({error: 'spell fizzled'});
  }
});

app.post('/spell', async (req, res) => {
  const keys = await sessionless.generateKeys(() => {return keys;}, () => {return keys;});
  
  const message = {
    pubKey: keys.pubKey,
    enteredText: "Foo",
    timestamp: new Date().getTime() + ''
  };

  message.signature = await sessionless.sign(JSON.stringify(message));

  try {
    const nextPath = req.body.spell.paths.pop();
    const registration = await superagent.post(nextPath)
      .send(message)
      .set('Content-Type', 'application/json')
      .set('Accept', 'application/json');

    const uuid = registration.body.uuid;

    const gatewayAddition = {
      timestamp: new Date().getTime(),
      uuid: uuid,
      minimumCost: 5,
      ordinal: 1
    };

    gatewayAddition.signature = await sessionless.sign(JSON.stringify(gatewayAddition));
 
    const spell = req.body.spell;
    spell.gateways.push(gatewayAddition);

    const resolution = await superagent.post(nextPath)
      .send(spell)
      .set('Content-Type', 'application/json')
      .set('Accept', 'application/json');

    magic();

    res.send(resolution.body);
  } catch(err) {
    res.send({error: 'spell fizzled'});
  }
});

app.post('/resolve', async (req, res) => {
  const spell = req.body.spell;
  let shouldResolve = true;
  spell.gateways.forEach(async (gateway) => {
    const signature = gateway.signature;
    delete gateway.signature;
    if(!(await sessionless.verifySignature(signature, JSON.stringify(gateway), getUser(gateway.uuid).pubKey))) {
      shouldResolve = false;
      console.log(chalk.red('It\'s the gateway signature'));
    }
  });
  if(shouldResolve) {console.log(chalk.green('First check passed'))}
  const casterSignature = spell.casterSignature;
  delete spell.casterSignature;
  delete spell.gateways;
  if(!(await sessionless.verifySignature(casterSignature, JSON.stringify(spell), getUser(spell.casterUUID).pubKey))) {
    shouldResolve = false;
    console.log(chalk.red('It\'s the casterSignature'));
  }

  if(shouldResolve) {console.log(chalk.green('Second check passed'))}
  // Here is where you would deduct points

  if(shouldResolve) {
    console.log(chalk.green('MAGICAL Success!'));
    return res.send({magic: "MAGIC!"});
  }
  res.send({error: 'Spell fizzled'});
});

addRoutes(app);

app.use(express.static('../web'));

app.listen(3002);






