# streaming-server - "C" in README-DEV.md's demo

A pure pass-through MAGIC gateway - stands in for a streaming platform's
backend (Twitch, or "some unnamed streaming platform" per the README). Per
the README's own text: "the streaming clients and server don't need to *do*
anything other than pass the request and the effect along between the
caster and resolver" - so unlike `../obs-gateway/` (D), this hop has no side
effect of its own.

See `../demo/README.md` for the full 5-participant chain this is one piece
of, and the run order.

**Status**: verified as part of the full chain this session - see
`../demo/README.md`.

## Setup

```bash
cp config.js.example config.js
npm install
node server.js   # :3200 by default
```

## Related

- Full chain: `../demo/README.md`
- Gateway library used here: `../../server/javascript/magic-adapter/`
