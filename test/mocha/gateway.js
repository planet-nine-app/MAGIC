import { should } from 'chai';
should();
import express from 'express';
import gateway from 'magic-gateway-js';
import fount from 'fount-js';
import sessionless from 'sessionless-node';
fount.baseURL = 'http://127.0.0.1:3006/';

const spellbook = {
  spellbookName: 'test',
  testSpell: {
    cost: 400,
    destinations: [
      {stopName: 'mocha-test', stopURL: 'http://127.0.0.1:2001/magic/spell/'},
      {stopName: 'fount', stopURL: 'http://127.0.0.1:3006/resolve/'}
    ],
    resolver: 'fount'
  }
};

let keys = undefined;

const saveKeys = (k) => {
  keys = k;
};

const getKeys = () => {
  return keys;
};

const fountUser = await fount.createUser(saveKeys, getKeys);

const app = express();
app.use(express.json());

app.use((req, res, next) => {
  console.log('got req');
  next();
});

const onSuccess = (res, result) => {
console.log('onSuccess', result);
  res.send(result);
};

gateway.expressApp(app, fountUser, spellbook, 'mocha-test', sessionless, null, onSuccess);

const spell = {
  timestamp: new Date().getTime() + '',
  spell: 'testSpell',
  casterUUID: fountUser.uuid,
  totalCost: 400,
  mp: true,
  ordinal: fountUser.ordinal
};



app.listen(2001);


it('should cast a spell successfully', async () => {

  const message = spell.timestamp + spell.spell + spell.casterUUID + 400 + true + fountUser.ordinal;
  await sessionless.generateKeys(() => {}, getKeys);
  spell.casterSignature = await sessionless.sign(message);

console.log('sending, ', spell);

  const spellResponse = await fetch('http://127.0.0.1:2001/magic/spell/testSpell', {
    method: 'post',
    body: JSON.stringify(spell),
    headers: {'Content-Type': 'application/json'}
  });

  const spellSuccess = await spellResponse.json();
console.log(spellSuccess);
  spellSuccess.success.should.equal(true);

}).timeout(60000);

it('should have nineum now', async () => {
  const updatedFountUser = await fount.getUserByUUID(fountUser.uuid);
console.log('updated fount user', updatedFountUser);
  updatedFountUser.nineumCount.should.equal(2);
}).timeout(60000);

