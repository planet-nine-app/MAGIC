import bdo from 'bdo-js';
import fount from 'fount-js';
import { should } from 'chai';
import sessionless from 'sessionless-node';
should();

const savedUser = {};
const savedUser2 = {};
let keys = {
  privateKey: '489d2c7d4875fd9244b715676986226cf9a9897b73b397f86d4f18369b7c2382',
  pubKey: '02469a93cec5a035cb1349971908bbaf48746b563feed831ced0532c07f25281d2'
};
let keys2 = {
  privateKey: '489d2c7d4875fd9244b715676986226cf9a9897b73b397f86d4f18369b7c2382',
  pubKey: '02469a93cec5a035cb1349971908bbaf48746b563feed831ced0532c07f25281d2'
};
let keysToReturn = {
  privateKey: '489d2c7d4875fd9244b715676986226cf9a9897b73b397f86d4f18369b7c2382',
  pubKey: '02469a93cec5a035cb1349971908bbaf48746b563feed831ced0532c07f25281d2'
};
const hash = 'firstHash';
const anotherHash = 'secondHash';

bdo.baseURL = `http://localhost:3003/`;
fount.baseURL = `http://localhost:3006/`;

it('should register the contractor with bdo and fount', async () => {
  const newBDO = {};
  const bdoUUID = await bdo.createUser(hash, newBDO, (k) => { keys = k; keysToReturn = k; }, () => { return keysToReturn; });
  const fountUser = await fount.createUser((_) => {}, () => { return keysToReturn; });

console.log('bdoUUID', bdoUUID, 'fountUser', fountUser);

  savedUser.bdoUUID = bdoUUID;
  savedUser.fountUUID = fountUser.uuid;

  bdoUUID.length.should.equal(savedUser.fountUUID.length);
});

it('should assign a galactic nineum to the contractor', async () => {
  const user = await fount.grantGalacticNineum(savedUser.fountUUID, '28880014');
console.log('galactic user', user);
  user.experiencePool.should.equal(0);  
});

it('should have the transferer post a bdo with how to initiate the transfer', async () => {
  const updatedBDO = {
    type: 'contract',
    id: '12341234',
    text: 'Party Time'
  };

  const user = await bdo.updateBDO(savedUser.bdoUUID, hash, updatedBDO); 
  user.uuid.length.should.equal(36);
});

it('should register the contractee with bdo and fount', async () => {
  keysToReturn = undefined;
  const newBDO = {};
  const bdoUUID = await bdo.createUser(hash, newBDO, (k) => { keys2 = k; keysToReturn = k; }, () => { return keysToReturn; });
  const fountUser = await fount.createUser((_) => {}, () => { return keysToReturn; });

  savedUser2.bdoUUID = bdoUUID;
  savedUser2.fountUUID = fountUser.uuid;

  bdoUUID.length.should.equal(savedUser2.fountUUID.length);
});

/*it('should have the contractee register a bdo for the transferer', async () => {
  const updatedBDO = {
    bdoUUID: savedUser2.bdoUUID,
    pub: true,
    pubKey: keys2.pubKey
  };

  const user = await bdo.updateBDO(savedUser2.bdoUUID, hash, updatedBDO);
  user.uuid.length.should.equal(36);
});*/

/*it('should have the transferer get a bdo for the trnsferee', async () => {
  keysToReturn = keys;
  const bdo = await bdo.getBDO(savedUser.bdoUUID, HASH, keys2.pubKey);
  bdo.bdo.bdoUUID.should.equal(savedUser2.bdoUUID);
});*/

it('should create a signature for contractor, and add to bdo', async () => {
  keysToReturn = keys;
  const updatedBDO = {
    type: 'contract',
    id: '12341234',
    text: 'Party Time'
  };

  updatedBDO.contractorSignature = await sessionless.sign(JSON.stringify(updatedBDO));

  const user = await bdo.updateBDO(savedUser.bdoUUID, hash, updatedBDO); 
  user.uuid.length.should.equal(36);

});

it('should create a signature for contractee, and add to contractor\'s bdo', async () => {
  const existingBDO = await bdo.getBDO(savedUser.bdoUUID, hash);
  keysToReturn = keys2;

  existingBDO.contracteeSignature = await sessionless.sign(JSON.stringify(bdo));
  keysToReturn = keys;

  const user = await bdo.updateBDO(savedUser.bdoUUID, hash, existingBDO);
  user.uuid.length.should.equal(36);
});

it('should have the contractee cast a spell at the cantractor', async () => {
  const spell = {
    timestamp: new Date().getTime() + '',
    spell: 'contract',
    casterUUID: savedUser.fountUUID,
    totalCost: 200,
    mp: true,
    ordinal: 400,
    gateways: []
  };

  const message = JSON.stringify({
    timestamp: spell.timestamp,
    spell: spell.spell,
    casterUUID: spell.casterUUID,
    totalCost: spell.totalCost,
    mp: spell.mp,
    ordinal: spell.ordinal,
  });

  spell.casterSignature = await sessionless.sign(message);

  const resp = await fetch('http://localhost:4444/magic/spell/contract', {
    method: 'post',
    body: JSON.stringify(spell),
    headers: {'Content-Type': 'application/json'}
  });
  const json = await resp.json();

console.log(json);
});

