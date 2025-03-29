'use strict';

var adapter = (fountUser, spell, spellbook, stopName, signer, extraForGateway) => {

  const nextDestinationForSpell = (spell) => {
    const spellName = spell.spell;
    const spellEntry = spellbook[spellName];
    const currentIndex = spellEntry.destinations.indexOf(spellEntry.destinations.find(($) => $.stopName === stopName));
    const nextDestination = spellEntry.destinations[currentIndex + 1].stopURL + spellName;
  
    return nextDestination;
  };

  const gatewayForSpell = async (spell) => { 
    const gateway = {
      timestamp: new Date().getTime() + '',
      uuid: fountUser.uuid,
      minimumCost: 20,
      ordinal: fountUser.ordinal
    };

    const message = gateway.timestamp + gateway.uuid + gateway.minimumCost + gateway.ordinal;

    gateway.signature = await signer.sign(message);
    if(extraForGateway) {
      gateway.extra = extraForGateway;
    }

    if(!spell.gateways) {
      spell.gateways = [];
    }

    spell.gateways.push(gateway);
    return spell;
  };

  const forwardSpell = async (spell, destination) => {
    return await fetch(destination, {
      method: 'post',
      body: JSON.stringify(spell),
      headers: {'Content-Type': 'application/json'}
    });
  };

  const putItAllTogether = async (spellToModify) => {
    const nextDestination = await nextDestinationForSpell(spellToModify);
    const updatedSpell = await gatewayForSpell(spellToModify);

    return await forwardSpell(updatedSpell, nextDestination);
  };

  return putItAllTogether;
};

var expressApp = (app, fountUser, spellbook, stopName, signer, extra, onSuccess) => {
  const noop = () => {};
  const extraForGateway = extra || noop;

  app.post('/magic/spell/:spellName', async (req, res) => {
    try {
      const spellName = req.params.spellName;
      const spell = req.body;
    
      const MAGIC = adapter(fountUser, spell, spellbook, stopName, signer, extraForGateway(spellName));
      
      const spellResponse = await MAGIC(spell);
      const spellResponseBody = await spellResponse.json();
      return onSuccess(req, res, spellResponseBody);
    } catch(err) {
      res.status(404);
      res.send({error: 'not found'});
    }
  });
};

var gateway = {
  expressApp
};

module.exports = gateway;
