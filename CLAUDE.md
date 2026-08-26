# MAGIC - Planet Nine's Multi-Device Consensus Protocol

## Overview

MAGIC (Multi-device Asynchronous Generic Input/output Consensus) is the
protocol layer that lets a **caster** authorize a request that passes
through arbitrary **gateways** to a **resolver**, with the resolver being
the only party that needs to be trusted by everyone else. It's built on
[sessionless](https://www.github.com/planet-nine-app/sessionless) - no
shared secrets anywhere in the pipeline, just asymmetric signatures.
See `README-DEV.md` for the full protocol writeup (caster/gateway/resolver
JSON shapes, the `mp` field, etc.) - this file is implementation notes for
this repo specifically.

**Location**: `/MAGIC/`

## Repo Structure

```
MAGIC/
├── README.md / README-DEV.md / README-UX.md   # protocol spec, three audiences
├── src/
│   ├── embedded/                               # hardware implementations
│   │   ├── ble-sessionless/                    # MAGIC over BLE - see below
│   │   ├── lora-sessionless/                   # MAGIC over LoRa (sender)
│   │   ├── lora-gateway-nesso/                 # LoRa gateway variant
│   │   ├── lora-gateway-umpros3/               # LoRa gateway on UM Pro S3
│   │   ├── espnow-wand/                        # ESP-NOW wand (caster) - see below
│   │   └── espnow-target/                      # ESP-NOW target (gateway) - see below
│   ├── macos/BLEBridge/                        # generic BLE central for ble-sessionless - see below
│   ├── appletv/MagicTV/                        # tvOS app - "B" in The Demo - see below
│   ├── games/potion-game/                      # tiny stand-in RPG - "E" in The Demo - see below
│   ├── streaming/
│   │   ├── obs-gateway/                        # MAGIC gateway that drives OBS Studio - "D" in The Demo - see below
│   │   ├── streaming-server/                   # pure pass-through gateway - "C" in The Demo
│   │   └── demo/                               # caster.js ("A") + The Demo's own README tying all 5 roles together
│   ├── desktop/                                # per-language MAGIC caster CLIs/demos (js, rust, swift)
│   └── server/                                 # per-language MAGIC gateway/resolver libs (js, rust, ts)
│       └── javascript/magic-adapter/           # reusable Express gateway helper (magic-gateway-js) - used by every Node hop above
└── test/
```

Each `lora-*`/`ble-*` project is a standalone PlatformIO project with its
own vendored `lib/sessionless/` (secp256k1 + keccak) and `lib/
secp256k1-embedded/` (the actual elliptic curve math, a git-submodule-style
vendor of `bitcoin-core/secp256k1`).

## Physical MAGIC: ESP-NOW Wand + Target

**Status: working end-to-end** (registration and full `wandCast` spell
casting, confirmed on real Unexpected Maker TinyS3[D] + Pro S3[D]
hardware, hosted-gateway backend). Full technical detail, wire protocol,
setup steps, and a blow-by-blow debugging journey live in
`src/embedded/espnow-wand/README.md` - read that before touching either
project, most of what's unusual in the code exists because of a specific
lesson learned there.

**The one-sentence version of what it is**: a wand (caster, TinyS3, ESP-NOW
only, never touches WiFi) casts the `wandCast` spell by broadcasting a
signed request to a target (gateway, Pro S3, WiFi-connected) over ESP-NOW;
the target adds its own gateway signature and relays the full MAGIC
payload to fount's hosted resolver at `https://allyabase-gateway.netlify.app`
over HTTPS; the target reacts locally (NeoPixel) to the resolution.

**The one thing worth knowing before diving into the code**: the biggest
bug of the whole build was hardware, not software - one physical board's
onboard antenna was defective, and every wand→target ESP-NOW transmission
silently failed for that reason alone, despite the driver reporting local
send success the entire time (no link-layer ACK exists for broadcast
frames, so "sent OK" only ever meant "handed to the radio," never
"actually received by anyone"). Fixed by switching that board's RF path to
its external antenna connector via GPIO38, which worked even completely
unterminated. See the README's "Debugging journey" section for the full
chain of bugs found on the way there (a hand-patched vendored crypto
library that skipped its own math, fount's global timestamp-freshness
middleware, an HTTP method mismatch, an invalid status code breaking
Netlify's Lambda response encoding, and PlatformIO port auto-detection
picking the wrong board).

**Backend changes that shipped alongside this**, in the wider allyabase
ecosystem (not in this repo, listed here since this embedded work is what
motivated them):
- `wandCast` spell added to `fount/src/server/node/spellbooks/
  spellbook.js` (both the canonical `fount` repo and the `allyabase/
  deployment/fount` copy actually bundled into the live Netlify gateway).
- fount's `resolve()` handler's invalid `res.status(900)` (not a real HTTP
  status code, breaks under Netlify's Lambda response encoding) fixed to
  `400`, in `allyabase/deployment/fount` (`netlify-packaging` branch), and
  redeployed to `https://allyabase-gateway.netlify.app`.
- `sharon/tests/magic/wand-cast-hosted.js` - a laptop-only Node script that
  exercises the exact same signed-message contract the firmware implements,
  useful for validating the backend independent of any hardware being
  flashed or even powered on.

## BLE Transport: `ble-sessionless` (self-contained target) + `macos/BLEBridge`

**Status: builds clean (firmware `pio run`, BLEBridge `swift build`), not
flashed to real hardware in this session** (no board attached). Structurally
this reuses `espnow-target`'s already-hardware-proven WiFi/NTP/fount code
almost verbatim, and casts the exact same deployed `wandCast` spell, so only
the BLE-specific parts are genuinely unverified - see
`src/embedded/ble-sessionless/README.md` ("What to check on first boot").

**The one-sentence version**: unlike the ESP-NOW wand/target pair (two
boards, two identities), this is **one** Pro S3 board that owns **one**
Sessionless identity and plays caster and gateway itself - it advertises a
BLE GATT peripheral (`MAGIC-ProS3`, same UUIDs an older crypto-free stub
already used) that casts a real signed `wandCast` spell whenever ANY
connected BLE central writes anything to its characteristic, or its BOOT
button is pressed. No MAGIC/sessionless logic needs to exist on the
laptop/phone/Apple TV side of that connection - that's the point of this
transport over building native casting code separately for each of those
platforms. `src/macos/BLEBridge/` is a matching generic BLE central
(macOS/CoreBluetooth) - it used to fake a LoRa-gateway payload just to
reuse `lora-server`'s display; now it triggers real casts and logs real
signed cast-result JSON.

## Streaming Transport: `src/streaming/obs-gateway`

**Status: the MAGIC relay itself is verified working end-to-end against a
local fount instance**, both standalone and as part of The Demo below. The
`obs-websocket-js` calls that actually drive OBS are unverified - no OBS
Studio installed in this environment; they're skipped gracefully (logged,
non-fatal) when OBS isn't reachable, which is exactly the path that was
exercised.

**The one-sentence version**: a caster casts a spell (`obsCast` standalone,
or `throwPotion` as "D" in The Demo below) at this gateway; it uses
`magic-gateway-js` (`src/server/javascript/magic-adapter/`) to add its own
gateway signature and forward onward; once resolved, it triggers a
configured OBS action (scene switch / source flash / hotkey, via
obs-websocket protocol v5) keyed by the spell's optional `effect` field, and
relays the response back the way it came. `config.js`'s `SPELL_NAME` /
`NEXT_DESTINATIONS` make this gateway reusable at any position in a chain,
not just as the last hop before fount - see its own README.

## The Demo: `src/appletv/MagicTV` + `src/streaming/*` + `src/games/potion-game`

**Status: verified end-to-end this session.** README-DEV.md's ["throw a
health potion at a streamer" demo](README-DEV.md#the-demo) - the full
5-participant chain, not just the OBS piece above - built and actually run:
a signed cast made it through all 5 hops (caster -> TV -> streaming server
-> streamer's client -> game client) and resolved against fount, with the
visible effect landing correctly (a browser-viewable HP bar in
`potion-game` jumped by exactly the configured heal amount). Full role
mapping, run order, and what's real vs. a placeholder (the video stream
defaults to a public Apple sample HLS URL; there's no actual game engine,
just a small stand-in battle scene) live in `src/streaming/demo/README.md`
- start there.

**The one genuinely novel piece**: `src/appletv/MagicTV/` is a real tvOS
app (built/booted via `xcodebuild`/`xcrun simctl` from the CLI, no Xcode
GUI in this environment) that plays the video and runs a local MAGIC
gateway. Before writing its Sessionless signing, two *other*
JavaScriptCore-based Swift Sessionless implementations already existed
elsewhere in this ecosystem - both were checked and both have real signing
bugs (one truncates every signed message to 32 characters, the other
doesn't zero-pad signature components, ~1-in-128 malformed). Neither was
reused; `src/appletv/MagicTV/crypto-bundle/entry.js` is a from-scratch,
minimal replacement built on the exact primitives `sessionless-node` itself
uses, cross-verified in both directions against the real `sessionless-node`
before being trusted. See `src/appletv/MagicTV/README.md` for the specifics
- worth reading before building on top of *any* Sessionless-over-JSContext
code in this wider ecosystem, not just this app.

**Backend changes made alongside this** (local-only, not deployed - flagged
here rather than silently made): `obsCast` and `throwPotion` entries were
added to the **local** `fount/src/server/node/spellbooks/spellbook.js` copy
(each single-hop, `destinations: [{stopName:'fount',...}]` only - see
obs-gateway's README, "Why two different spellbook.js files," for why each
must NOT also list its own gateway's stopName, or fount would forward the
resolved spell back into a loop). These edits are uncommitted in the
`fount` repo and were not made to `allyabase/deployment/fount` or deployed
anywhere - review/commit/revert them there independently of this repo.

## Last Updated
August 2026. Physical MAGIC (ESP-NOW wand + target) confirmed working
end-to-end on real hardware. BLE transport (`ble-sessionless` +
`macos/BLEBridge`) built, unverified on real hardware. README-DEV.md's full
"throw a health potion at a streamer" demo (`appletv/MagicTV` +
`streaming/*` + `games/potion-game`) built and verified end-to-end locally,
including a real tvOS app running in the Simulator - see "The Demo" above.
