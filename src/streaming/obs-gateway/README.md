# OBS Gateway - MAGIC pathway into streaming software

Wires OBS Studio into a MAGIC spell as a real gateway: a caster (a viewer's
client, a chat bot, whatever) casts a spell at this service, this service
adds its own gateway signature and forwards onward, and once the spell
resolves, drives an OBS action (switch scene, flash a source, trigger a
hotkey) via [obs-websocket](https://github.com/obsproject/obs-websocket)
(protocol v5, built into OBS Studio 28+).

This plays "D" (Streamer's Client) in the full version of README-DEV.md's
"throw a health potion at a streamer" demo, now actually built end-to-end -
see [`../demo/README.md`](../demo/README.md) for the complete 5-participant
chain (caster -> TV -> streaming server -> **this** -> game client ->
resolver) and how to run it. This gateway also still works standalone
(forwarding straight to fount, no chain) via `config.js`'s `SPELL_NAME` /
`NEXT_DESTINATIONS` - see "Setup" below for both modes.

**Status: not yet run against real OBS in this session** (no OBS Studio
installed in this environment). The MAGIC relay itself - caster ->
this gateway -> fount -> back through this gateway - is real, uses the
same signed-message contract already proven by
`sharon/tests/magic/wand-cast-hosted.js` and the ESP-NOW wand/target demo,
and can be (and should be, before trusting this) exercised end-to-end
against a locally-run fount using `test/cast.js` even without OBS running -
see "Testing without OBS" below. Only the actual `obs-websocket-js` calls
in `src/effects.js` are unverified against a live OBS instance.

## Why `magic-gateway-js`

[`../../server/javascript/magic-adapter/`](../../server/javascript/magic-adapter/)
already implements exactly this shape of gateway - forward a spell, add your
own signature, and do something real once it resolves (its own README uses
"a raspberry pi wired up to a discoball and strobe light" as the example).
This just plugs OBS in as that "something real," via `gateway.expressApp()`.

## Architecture

```
Caster --POST /magic/spell/obsCast--> obs-gateway --POST /resolve/obsCast--> fount
                                            ^                                   |
                                            |________________ resolved (or not) |
                                            |
                                       triggers OBS action (obs-websocket)
                                            |
                                    response relayed back to caster
```

## Why two different `spellbook.js` files

Both this gateway (`src/spellbook.js`) and fount (its own
`spellbooks/spellbook.js`) need an `obsCast` entry, but they need
**different** `destinations` arrays, and getting this backwards causes a
real bug (an infinite forwarding loop):

- **This gateway's copy** needs a two-hop array,
  `[{stopName:'obs-gateway',...}, {stopName:'fount',...}]`, because
  `magic-gateway-js` uses it to find "what comes after me" and forward
  there. Without the `obs-gateway` entry, it wouldn't know its own position
  in the chain.
- **fount's copy** must stay a single-hop array, `[{stopName:'fount',...}]`
  *only*. fount's `resolve()` handler forwards the resolved spell to every
  destination in *its own* spellbook entry other than itself
  (`destinations.filter(d => d.stopName !== 'fount')`) - if fount's copy
  also listed `obs-gateway`, fount would POST the resolved spell straight
  back to this gateway's `/magic/spell/obsCast` a second time after
  resolving it, which would try to gateway-relay it again.

This mirrors exactly how `wandCast`'s entry in fount's spellbook
(`destinations: [{stopName:'fount', stopURL: ...}]`, single-hop, no further
forwarding) already works for the ESP-NOW demo - the "effect" there is
delivered by the direct HTTP response chain instead, same as it is here.

## Setup

1. **A fount instance to resolve against.** For local development:
   ```bash
   cd ../../../../fount/src/server/node
   LOCALHOST=true node fount.js   # listens on :3006
   ```
   (Talking to the hosted `allyabase-gateway.netlify.app` deployment instead
   would require adding an `obsCast` entry there too - a production
   deployment change, out of scope for this README; ask before doing that.)

2. **Add the `obsCast` entry to fount's spellbook** (see "Why two different
   spellbook.js files" above for exactly what it needs to look like) -
   `fount/src/server/node/spellbooks/spellbook.js`:
   ```javascript
   obsCast: {
     cost: 50,
     destinations: [
       { stopName: 'fount', stopURL: process.env.LOCALHOST ? 'http://localhost:3006/resolve/' : `https://${SUBDOMAIN}.fount.allyabase.com/resolve/` }
     ],
     resolver: 'fount',
     mp: true
   }
   ```

3. ```bash
   npm install
   cp config.js.example config.js
   ```
   Fill in `OBS_URL`/`OBS_PASSWORD` (Tools > obs-websocket Settings in OBS)
   and the `EFFECTS` map to match real scene/source names in your OBS setup
   - the defaults (`Scene` / `MAGIC-Overlay`) are placeholders.

4. ```bash
   npm start
   ```
   Expect: this gateway generates (or loads) its own Sessionless keypair,
   registers with fount (`PUT /user/create -> HTTP 200`), attempts to
   connect to OBS (warns and continues if it can't - see "Status" above),
   then listens on `config.PORT` (default 3100).

**To run this as "D" in the full chain instead** (see
[`../demo/README.md`](../demo/README.md)): after step 3, edit `config.js`'s
`SPELL_NAME` to `'throwPotion'` and `NEXT_DESTINATIONS` to point at the game
client (`[{stopName:'game-client', stopURL:'http://localhost:3300/magic/spell/'}]`)
instead of fount directly - `src/spellbook.js` builds this gateway's local
spellbook entry from those two config values, so no code changes are
needed, just config. fount needs a matching `throwPotion` entry (single-hop,
same shape as `obsCast` above) instead of, or alongside, the `obsCast` one.

## Testing without OBS

```bash
npm run cast          # casts effect "potion"
node test/cast.js hype   # or any effect name from config.js EFFECTS
```

This registers a fresh throwaway caster identity with fount and casts
`obsCast` directly at this gateway - exercises the full signed relay
(caster -> gateway -> fount -> gateway) even with OBS unreachable, since
`src/effects.js` treats "OBS not connected" as a non-fatal, logged skip
rather than a spell failure. `result.obsGateway.triggered` in the response
tells you whether the effect actually fired.

## Spell shape

`obsCast` follows the standard MAGIC caster payload (see
[`../../../README-DEV.md`](../../../README-DEV.md)) plus one optional field:

```json
{
  "timestamp": "...", "spell": "obsCast", "casterUUID": "...",
  "totalCost": 50, "mp": true, "ordinal": ...,
  "casterSignature": "...", "gateways": [],
  "effect": "potion"
}
```

`effect` selects which entry in `config.js`'s `EFFECTS` map fires - add your
own (`scene`, `sourceFlash`, or `hotkey` types, see `config.js.example`) to
match whatever you've built in OBS.

## Related

- MAGIC protocol: `../../../README-DEV.md`
- Gateway library used here: `../../server/javascript/magic-adapter/`
- Ground-truth signed-message contract: `sharon/tests/magic/wand-cast-hosted.js`, `../../embedded/espnow-wand/README.md`
