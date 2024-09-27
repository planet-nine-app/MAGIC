export default (fountUser, spell, spellbook, stopName, signer, extraForGateway) => {

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
