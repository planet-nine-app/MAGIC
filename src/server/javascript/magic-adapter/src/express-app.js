import adapter from './magic.js';

export default (app, fountUser, spellbook, stopName, signer, extra, onSuccess) => {
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
