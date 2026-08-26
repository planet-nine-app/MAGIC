// This gateway's own copy of the spellbook entries it cares about - used
// only to compute where to forward a spell next (magic-gateway-js's
// nextDestinationForSpell looks up spellbook[spell].destinations, finds
// this gateway's own stopName, and forwards to whatever comes after it).
//
// Deliberately a TWO-hop destinations array (obs-gateway -> fount), unlike
// fount's own local spellbook.js entry for the same spell, which must stay
// a single-hop array (itself only). If fount's copy also listed
// 'obs-gateway', fount would forward the resolved spell right back here a
// second time after resolving it (its resolve() handler POSTs to every
// non-'fount' destination in ITS OWN spellbook copy) - see README.md
// "Why two different spellbook.js files" for the full explanation.
// stopURL needs to be a plain string (magic-gateway-js concatenates it
// directly with the spell name), so this is a factory rather than a static
// object - config isn't known until server.js loads it.
//
// config.SPELL_NAME/NEXT_DESTINATIONS make this gateway reusable at any
// position in a chain, not just as the last hop before fount: standalone
// (default) it's obsCast -> fount directly; in ../demo/ it's reconfigured as
// throwPotion's "D" (Streamer's Client) hop, forwarding to "E" (the game
// client) instead - see ../README.md and ../demo/README.md.
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
