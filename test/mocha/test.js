import { should } from 'chai';
should();
import sessionless from 'sessionless-node';
import superAgent from 'superagent';

const baseURL = 'http://127.0.0.1:3000/';
//const baseURL = 'https://thirsty-gnu-80.deno.dev/';

const get = async function(path) {
  //console.info("Getting " + path);
  return await superAgent.get(path).set('Content-Type', 'application/json');
};

const put = async function(path, body) {
  //console.info("Posting " + path);
  return await superAgent.put(path).send(body).set('Content-Type', 'application/json');
};

const post = async function(path, body) {
  //console.info("Putting " + path);
  return await superAgent.post(path).send(body).set('Content-Type', 'application/json');
};

let savedUser = {};
let gatewayUsers = [];
let keys = {};
let casterKeys = {};

it('should register a user', async () => {
  keys = await sessionless.generateKeys(() => { return keys; }, () => {return keys;});
  casterKeys = keys;
  const payload = {
    timestamp: new Date().getTime() + '',
    pubKey: keys.pubKey
  };

  payload.signature = await sessionless.sign(JSON.stringify(payload));

  payload.hash = 'first-hash';

  const res = await put(`${baseURL}user/create`, payload);
  savedUser = res.body;
  res.body.uuid.length.should.equal(36);
});

it('should register gateway user', async () => {
  const gatewayKeys = await sessionless.generateKeys(() => { return keys; }, () => {return keys;});
  const payload = {
    timestamp: new Date().getTime() + '',
    pubKey: gatewayKeys.pubKey
  };

  keys = gatewayKeys;

  payload.signature = await sessionless.sign(JSON.stringify(payload));

  payload.hash = 'first-hash';

  const res = await put(`${baseURL}user/create`, payload);
  const gatewayUser = res.body;
  gatewayUser.keys = gatewayKeys;
  gatewayUsers.push(gatewayUser);
  res.body.uuid.length.should.equal(36);
});

it('should register gateway user', async () => {
  const gatewayKeys = await sessionless.generateKeys(() => { return keys; }, () => {return keys;});
  const payload = {
    timestamp: new Date().getTime() + '',
    pubKey: gatewayKeys.pubKey
  };

  keys = gatewayKeys;

  payload.signature = await sessionless.sign(JSON.stringify(payload));

  payload.hash = 'first-hash';

  const res = await put(`${baseURL}user/create`, payload);
  const gatewayUser = res.body;
  gatewayUser.keys = gatewayKeys;
  gatewayUsers.push(gatewayUser);
  res.body.uuid.length.should.equal(36);
});

it('should resolve a spell', async () => {
  const spell = {
    timestamp: new Date().getTime() + '',
    spell: 'test-spell',
    casterUUID: savedUser.uuid,
    totalCost: 500,
    mp: true,
    ordinal: 1
  };

  keys = casterKeys;
  spell.casterSignature = await sessionless.sign(JSON.stringify(spell));
  spell.gateways = [];

  const gateway1 = {
    timestamp: new Date().getTime() + '',
    uuid: gatewayUsers[0].uuid,
    minimumCost: 30,
    ordinal: 2
  };

  keys = gatewayUsers[0].keys;
  gateway1.signature = await sessionless.sign(JSON.stringify(gateway1));
  spell.gateways.push(gateway1);

  const gateway2 = {
     timestamp: new Date().getTime() + '',
     uuid: gatewayUsers[1].uuid,
     minimumCost: 40,
     ordinal: 3
  };

  keys = gatewayUsers[1].keys;
  gateway2.signature = await sessionless.sign(JSON.stringify(gateway2));
  spell.gateways.push(gateway2);
  
  const res = await post(`${baseURL}resolve/spell/${spell.spell}`, spell);
  res.body.success.should.equal(true);
});
