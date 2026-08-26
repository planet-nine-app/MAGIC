# The Demo

This is [README-DEV.md's "the demo"](../../../README-DEV.md#the-demo) built
for real: throw your favorite streamer a health potion mid-boss-fight,
without any of the streaming platform's own systems needing to know or care
what a "health potion" is.

```
A[User's Client<Caster>] -->|Casts| B[TV<Target>]
B -->|Passes Request| C{Streaming Server<Gateway>}
C -->|Passes Request| D[Streamer's Client<Gateway>]
D -->|Passes Request| E[Game Client<Gateway>]
E -->|Passes Request| F{Server<Resolver>}
F -->|Effect| E --> D --> C --> B --> A
```

| Role | This repo | What it actually is here |
|---|---|---|
| A - User's Client (Caster) | `caster.js` (this directory) | A Node CLI - stands in for a phone/remote app |
| B - TV (Target) | `../../appletv/MagicTV/` | A real tvOS app - plays video, relays the cast |
| C - Streaming Server (Gateway) | `../streaming-server/` | Pure pass-through - per the README, this hop "doesn't need to *do* anything other than pass the request... along" |
| D - Streamer's Client (Gateway) | `../obs-gateway/` (reconfigured) | Would drive OBS in a real setup - see its own README |
| E - Game Client (Gateway) | `../../games/potion-game/` | A tiny stand-in RPG battle scene - where the potion actually lands |
| F - Server (Resolver) | fount | Verifies every signature, spends MP, and is the only party everyone else needs to trust |

**Status: verified end-to-end this session** - a real cast, signed by a
throwaway caster identity, made it through all six hops (five relays plus
the resolver) and back, with the visible effect landing correctly (the
player's HP bar in `potion-game` went up by exactly the configured heal
amount). B (the tvOS app) ran in the tvOS Simulator, not a physical Apple
TV - see `../../appletv/MagicTV/README.md`.

## Why each hop needs its own `spellbook.js`, and fount needs a different one again

Every gateway in this chain (`streaming-server`, `obs-gateway`,
`potion-game`, and B's Swift equivalent in `Config.swift`) needs to know two
things: its own `stopName`, and where the spell goes *next*. Each one's
local spellbook entry for `throwPotion` therefore has a **different**
`destinations` array - just itself plus whatever's immediately downstream:

- B (tvOS): hardcoded in `Config.swift` (`nextHopURL` -> C)
- C (`streaming-server`): `[{stopName:'streaming-server'}, {stopName:'obs-gateway'}]`
- D (`obs-gateway`): `[{stopName:'obs-gateway'}, {stopName:'game-client'}]`
- E (`potion-game`): `[{stopName:'game-client'}, {stopName:'fount'}]`

fount's **own** copy (`fount/src/server/node/spellbooks/spellbook.js`) must
stay single-hop - `[{stopName:'fount', ...}]` **only**. fount forwards the
resolved spell to every non-`fount` destination in *its own* spellbook
entry after resolving; if fount's copy also listed any of the above, it
would re-POST the resolved spell back into the chain a second time. See
`../obs-gateway/README.md` ("Why two different spellbook.js files") for the
fuller version of this explanation - it applies to every hop here, not just
D.

## Running it

All ports assume the defaults in each project's `config.js.example` /
`Config.swift`. Start in this order - each one self-registers with fount on
first boot, so fount has to be up first:

```bash
# 0. fount (F) - the resolver every hop registers with
cd fount/src/server/node && LOCALHOST=true node fount.js   # :3006

# Add the throwPotion entry to fount's spellbook if it isn't already there -
# see fount/src/server/node/spellbooks/spellbook.js, single-hop, same shape
# as the existing wandCast/obsCast entries.

# 1. potion-game (E)
cd MAGIC/src/games/potion-game
cp config.js.example config.js && npm install && node server.js   # :3300
# open http://localhost:3300 in a browser to watch the battle scene live

# 2. obs-gateway (D) - reconfigure it to sit mid-chain, not standalone
cd MAGIC/src/streaming/obs-gateway
cp config.js.example config.js && npm install
# edit config.js: SPELL_NAME: 'throwPotion',
#                 NEXT_DESTINATIONS: [{stopName:'game-client', stopURL:'http://localhost:3300/magic/spell/'}]
node server.js   # :3100

# 3. streaming-server (C)
cd MAGIC/src/streaming/streaming-server
cp config.js.example config.js && npm install && node server.js   # :3200

# 4. MagicTV (B) - see ../../appletv/MagicTV/README.md
cd MAGIC/src/appletv/MagicTV
xcodegen generate && xcodebuild -project MagicTV.xcodeproj -scheme MagicTV \
  -destination 'platform=tvOS Simulator,name=Apple TV' build
xcrun simctl boot "Apple TV"
xcrun simctl install booted <path-to-.app>   # printed by the build, under DerivedData
xcrun simctl launch booted com.planetnine.magic.tv   # :8787

# 5. Throw a potion (A)
cd MAGIC/src/streaming/demo
npm install
node caster.js   # defaults to localhost:8787 - pass host:port for a real device
```

A successful cast prints `HTTP 200 {"success":true,...}` from `caster.js`,
and (if `potion-game` is open in a browser) the HP bar visibly jumps.

## What's real and what's a stand-in

- **The MAGIC relay itself** (every signature, every hop, fount's
  verification and MP spend) is completely real - the same protocol code
  used by every other transport in this repo.
- **The video** is a placeholder public Apple sample stream by default -
  wiring up a real OBS -> HLS pipeline (OBS -> RTMP -> a media server like
  `node-media-server` producing HLS) is a separate infrastructure piece not
  built here.
- **The game** is not Final Fantasy 1 - it's a tiny original battle scene in
  the same visual spirit (a boss, a health bar, a thrown potion), built to
  have somewhere real for the "effect" to land without pulling in an actual
  game engine.
- **The caster** is a CLI, standing in for a phone/remote app - see
  `../../appletv/MagicTV/README.md` for why B (the tvOS app) was scoped to
  this repo's own transport work rather than also building a native
  companion app.

## Related

- The demo this implements: `../../../README-DEV.md#the-demo`
- B: `../../appletv/MagicTV/README.md`
- D: `../obs-gateway/README.md`
