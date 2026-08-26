// This gateway's own copy of the spellbook - see ../../obs-gateway/README.md
// ("Why two different spellbook.js files") for why every hop in a chain
// needs its own copy with a destinations array starting at itself, and why
// fount's own copy must stay single-hop.
export default (config) => ({
  [config.SPELL_NAME]: {
    cost: config.SPELL_COST,
    mp: true,
    resolver: 'fount',
    destinations: [
      { stopName: config.STOP_NAME, stopURL: `http://localhost:${config.PORT}/magic/spell/` },
      ...config.NEXT_DESTINATIONS
    ]
  }
});
