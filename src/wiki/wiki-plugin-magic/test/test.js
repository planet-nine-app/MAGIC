import bdo from 'bdo-js';
import fount from 'fount-js';
import { should } from 'chai';
import sessionless from 'sessionless-node';
should();

const savedUser = {};
const savedUser2 = {};
let keys;
let keys2 = {};
let keysToReturn;
const hash = 'firstHash';
const anotherHash = 'secondHash';

bdo.baseURL = `http://localhost:3003/`;
fount.baseURL = `http://localhost:3006/`;

it('should register the transferer with bdo and fount', async () => {
  const newBDO = {};
  const bdoUUID = await bdo.createUser(hash, newBDO, (k) => { keys = k; keysToReturn = k; }, () => { return keysToReturn; });
  const fountUser = await fount.createUser((_) => {}, () => { return keysToReturn; });

console.log('bdoUUID', bdoUUID, 'fountUser', fountUser);

  savedUser.bdoUUID = bdoUUID;
  savedUser.fountUUID = fountUser.uuid;

  bdoUUID.length.should.equal(savedUser.fountUUID.length);
});

it('should assign a galactic nineum to the transferer', async () => {
  const user = await fount.grantGalacticNineum(savedUser.fountUUID, '28880014');
console.log('galactic user', user);
  user.experiencePool.should.equal(0);  
});

it('should have the transferer post a bdo with how to initiate the transfer', async () => {
  const updatedBDO = {
    type: 'connecting',
    id: '12341234',
    text: 'Party Time'
  };

  const user = await bdo.updateBDO(savedUser.bdoUUID, hash, updatedBDO); 
  user.uuid.length.should.equal(36);
});

it('should register the transfee with bdo and fount', async () => {
  keysToReturn = undefined;
  const newBDO = {};
  const bdoUUID = await bdo.createUser(hash, newBDO, (k) => { keys2 = k; keysToReturn = k; }, () => { return keysToReturn; });
  const fountUser = await fount.createUser((_) => {}, () => { return keysToReturn; });

  savedUser2.bdoUUID = bdoUUID;
  savedUser2.fountUUID = fountUser.uuid;

  bdoUUID.length.should.equal(savedUser2.fountUUID.length);
});

it('should have the transferee register a bdo for the transferer', async () => {
  const updatedBDO = {
    bdoUUID: savedUser2.bdoUUID,
    pub: true,
    pubKey: keys2.pubKey
  };

  const user = await bdo.updateBDO(savedUser2.bdoUUID, hash, updatedBDO);
  user.uuid.length.should.equal(36);
});

it('should have the transferer get a bdo for the trnsferee', async () => {
  keysToReturn = keys;
  const bdo = await bdo.getBDO(savedUser.bdoUUID, HASH, keys2.pubKey);
  bdo.bdo.bdoUUID.should.equal(savedUser2.bdoUUID);
});

it('should have the transferer grant an inital nineum to the transferee', async () => {
  keysToReturn = keys;
  const flavor = 'c1c1c1c1c1c1';
  const user = await fount.grantNineum(savedUser.fountUUID, savedUser2.fountUUID, flavor);
console.log('nineum user', user);
  user.experiencePool.should.equal(0);
});

it('should have the transferer grant a second nineum to the transferee', async () => {
  const flavor = 'd1d1d1d1d1d1';
  const user = await fount.grantNineum(savedUser.fountUUID, savedUser2.fountUUID, flavor);
console.log('nineum user', user);
  user.experiencePool.should.equal(0);
});

it('should have the transferer grant an admin nineum to the transferee', async () => {
  const user = await fount.grantAdminNineum(savedUser.fountUUID, savedUser2.fountUUID);
  user.experiencePool.should.equal(0);
});
