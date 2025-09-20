(function() {
var _gateway = require('magic-gateway-js');
var _fount = require('fount-js');
var _bdo = require('bdo-js');
var sessionless = require('sessionless-node');
var fs = require('fs');

const gateway = _gateway.default; 
const fount = _fount.default;
const bdo = _bdo.default;

fount.baseURL = 'http://localhost:3006/';
bdo.baseURL = 'http://localhost:3003/';

const HASH = 'MAGIC';

let fountUser;

async function startServer(params) {
  const app = params.app;
  const argv = params.argv;

  let allyabaseUser = {};
  
  const allyabaseKeys = {
    privateKey: argv.contract_private_key,
    pubKey: argv.contract_pub_key
  };

  const saveKeys = () => {};
  const getKeys = () => allyabaseKeys;

  await fount.createUser(saveKeys, getKeys);
  await bdo.createUser({}, HASH, saveKeys, getKeys);

  await sessionless.generateKeys(saveKeys, getKeys);

//  app.get('/plugin/MAGIC/user', async function(req, res) {
console.log('getting called here');
    let fountUser;
    let bdoUUID;

    if(!allyabaseUser || !fountUser) {
console.log('fount looks like', fount);
      fountUser = await fount.getUserByPublicKey(argv.pub_key);
      if(!fountUser || !fountUser.uuid) {
        fountUser = await fount.createUser(saveKeys, getKeys);   
      }
    }

    if(!allyabaseUser || !allyabaseUser.bdoUser) {
      bdoUUID = await bdo.createUser(HASH, {}, saveKeys, getKeys);
      allyabaseUser.bdoUser = {bdo: {}, uuid: bdoUUID};
    }
    
    allyabaseUser.bdoUser = await bdo.getBDO(allyabaseUser.bdoUser.uuid, HASH);
    allyabaseUser.fountUser = fountUser;
    allyabaseUser.nineum = await fount.getNineum(allyabaseUser.fountUser.uuid);
console.log('allyabaseUser is: ', allyabaseUser);

//    res.send(allyabaseUser);
//  });

  app.post('/plugin/MAGIC/resolve', async function(req, res) {
    const payload = req.body;
    const message = JSON.stringify({
      timestamp: payload.timestamp,
      spell: payload.spell,
      casterUUID: payload.casterUUID,
      totalCost: payload.totalCost,
      mp: payload.mp,
      ordinal: payload.ordinal,
    });

    payload.casterSignature = await sessionless.sign(message);

    const resolution = await fount.resolve(payload);
    if(resolution.success) {
      const updatedUser = await fount.getUserByUUID(payload.casterUUID);
      return res.send(updatedUser);
    }
    res.send(resolution);
  });

  app.get('/plugin/MAGIC/user/:pubKey', async function(req, res) {
    fountUser = await fount.getUserByPublicKey(req.params.pubKey);
console.log('getting the user on the server, it looks like: ', fountUser);
    fountUser.nineum = await fount.getNineum(fountUser.uuid);
    res.send(fountUser);
  });

  app.get('/plugin/MAGIC/spellbooks', async function(req, res) {
    try {
     const spellbooksResp = await bdo.getSpellbooks(allyabaseUser.bdoUser.uuid, HASH, req.query.pubKey);
     res.send(spellbooksResp);
   } catch(err) {
     res.status(404);
     res.send(err);
   }
  });

  const spellbook = {
    spellbookName: 'ReaLocalize', 
    contract: {
      cost: 1,
      destinations: [
        {stopName: 'contract-wiki', stopURL: 'http://127.0.0.1:4444/plugin/magic/spell/'},
        {stopName: 'fount', stopURL: 'http://127.0.0.1:3006/resolve/'}
      ]
    }
  };

  const myStopName = 'contract-wiki';

  const extraForGateway = (spellName) => {
    if(spellName === 'contract') {
      return 'contractUUID';
    }    
  };

  const onSuccess = (req, res, result) => {
    result.whatToDo = 'Transfer some nineum and stuff';    
    const spell = req.body;
    const casterUUID = spell.casterUUID;
    const targetUUID = spell.gateways[0].uuid;

    fetch('http://127.0.0.1:4444/plugin/ftt/grant-nineum', {
      method: 'post',
      body: JSON.stringify({toUUID: casterUUID, flavor: 'c1c1c1c1c1c1'}),
      headers: {'Content-Type': 'application/json'}
    });

    fetch('http://127.0.0.1:4444/plugin/ftt/grant-nineum', {
      method: 'post',
      body: JSON.stringify({toUUID: targetUUID, flavor: 'ea10c411133c'}),
      headers: {'Content-Type': 'application/json'}
    });

    res.send(result); // the result needs to make it back to the caster for the spell to complete
  };

console.log('gateway looks like', gateway);

  gateway.expressApp(app, allyabaseUser.fountUser, spellbook, myStopName, sessionless, extraForGateway, onSuccess);
}

module.exports = {startServer};
}).call(this);
