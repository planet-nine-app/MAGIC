import sessionless from 'sessionless-node';
import superagent from 'superagent';
import config from '../../config/local.js';

const magic = async (bodyFromRegistration, passThrough) => {
  console.log('passThrough is: ' + passThrough);
  const { uuid, welcomeText, color } = bodyFromRegistration;

  const colorURL = config.colors[color].serverURL;

  const spell = {
    timestamp: new Date().getTime(),
    spell: 'demo cadabra!',
    casterUUID: uuid,
    totalCost: 10, 
    mp: true,
    ordinal: 1,
  };

  spell.casterSignature = await sessionless.sign(JSON.stringify(spell));

  spell.gateways = [];

console.log(spell);

  try {
console.log('sending to: ' + colorURL);
    const res = await superagent.post('http://localhost:3001/' + (passThrough ? 'pass-through' : 'spell'))
      .send({ spell })
      .set('Content-Type', 'application/json')
      .set('Accept', 'application/json');
   
    return res.body;
  } catch(err) {
console.log(err);
    return new Error(err);
  }
};

export default magic;
