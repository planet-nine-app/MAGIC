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
