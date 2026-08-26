# potion-game - "E" (Game Client) in README-DEV.md's demo

A tiny stand-in RPG battle scene - not Final Fantasy 1, just its shape: a
boss fight with a player HP bar that visibly refills when a `throwPotion`
spell resolves through this gateway. This is the last hop before fount (the
resolver), and where the "effect" from README-DEV.md's demo actually
becomes visible.

See `../../streaming/demo/README.md` for the full 5-participant chain this
is one piece of, and the run order.

**Status**: verified as part of the full chain this session - a real cast
made HP jump by exactly `POTION_HEAL` in a live run, confirmed both via
`GET /state` and the SSE-driven browser view.

## Setup

```bash
cp config.js.example config.js
npm install
node server.js   # :3300 by default
```

Open `http://localhost:3300` in a browser to watch the battle scene live -
it starts at 35% HP (mid-fight) and jumps by `config.POTION_HEAL` (default
30, capped at `config.MAX_HP`) each time a `throwPotion` spell resolves
through this gateway.

## Related

- Full chain: `../../streaming/demo/README.md`
- Gateway library used here: `../../server/javascript/magic-adapter/`
