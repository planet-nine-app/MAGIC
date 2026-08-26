# MagicTV - "B" (TV / Target) in README-DEV.md's demo

A real tvOS app: plays a video stream (standing in for "watching a streamer
on your TV") and, at the same time, runs a local MAGIC gateway that relays
a cast from a nearby device toward the rest of the chain -
`../../streaming/demo/README.md` covers the full 5-participant flow this is
one piece of.

```
A[User's Client<Caster>] -> B[TV<Target>] -> C{Streaming Server} -> D[Streamer's Client] -> E[Game Client] -> F{Resolver}
                              ^^^^^^^^^^^^
                              this app
```

**Status: verified end-to-end in the tvOS Simulator this session** - not on
a real Apple TV device (none available here). Built and booted via
`xcodebuild`/`xcrun simctl` from the CLI (no Xcode GUI in this environment),
registered a real Sessionless identity with a locally-running fount (real
`PUT /user/create` round trip, confirmed in fount's own logs), and correctly
relayed a signed `throwPotion` cast through every other hop in the demo
chain and back - see `../../streaming/demo/README.md` for the exact
commands to reproduce this. The Simulator shares the host Mac's network
stack directly, which is what made this level of verification possible
without a physical device; a real Apple TV would need its LAN IP wired into
`caster.js` instead of `localhost`, and hasn't been tested.

## Why a tvOS app needed its own crypto, and why it isn't reusing what already existed

Before writing this, two other Sessionless-over-JavaScriptCore
implementations already existed elsewhere in this ecosystem
(`sessionless/src/swift/client-ios/Sessionless/` - the canonical CocoaPod -
and `The Advancement/Shared (Extension)/Sessionless.swift`). Both were
checked, and both have real signing bugs:

- The CocoaPod's bundled `crypto.js` truncates every signed message to 32
  characters (`stri.slice(0, 32)` inside its `utf8ToBytes`) - every real
  MAGIC message (`timestamp+spell+casterUUID+totalCost+mp+ordinal`, always
  well over 32 characters) would silently sign the wrong bytes.
- The Advancement's `crypto.js` doesn't zero-pad the `r`/`s` signature
  components before concatenating them (`sig.r.toString(16) +
  sig.s.toString(16)`) - correct most of the time, but produces a malformed,
  unverifiable signature whenever either component happens to have a
  leading zero byte (roughly 1 in 128 signatures).

`crypto-bundle/entry.js` is a from-scratch, minimal replacement - the exact
same `ethereum-cryptography` primitives `sessionless-node` itself uses
(`secp256k1.sign(...).toCompactHex()`, matching zero-padding included), just
repackaged for JSContext's synchronous calling convention and lack of
`TextEncoder`. It's cross-verified in both directions against the real
`sessionless-node` (a signature made here verifies there and vice versa) -
see that file's own comments, or re-run the check yourself:

```bash
cd crypto-bundle && npm install
npx esbuild entry.js --bundle --format=iife --platform=browser --outfile=../Resources/crypto.js
```

(The bundled `Resources/crypto.js` is committed - only re-run the above if
you change `entry.js`.)

## Setup

Requires [XcodeGen](https://github.com/yonaskolb/XcodeGen) (`brew install
xcodegen`) - `MagicTV.xcodeproj` is generated from `project.yml`, not
committed.

```bash
xcodegen generate
open MagicTV.xcodeproj    # or build/run from the CLI, see below
```

Edit `MagicTV/Config.swift` directly (no `.example` copy-step convention for
Xcode projects) - `fountBase`, `nextHopURL`, `listenPort`, `hlsURL` all have
real working defaults matching `../../streaming/demo/README.md`'s setup.

### CLI build/run (what was actually used to verify this)

```bash
xcodebuild -project MagicTV.xcodeproj -scheme MagicTV \
  -destination 'platform=tvOS Simulator,name=Apple TV' build

xcrun simctl boot "Apple TV"   # or reuse an already-booted one
xcrun simctl install booted <path-to-MagicTV.app-in-DerivedData>
xcrun simctl launch --console-pty booted com.planetnine.magic.tv
```

Once running, `curl -X POST http://localhost:8787/magic/spell/throwPotion` from
the host Mac reaches it directly (Simulator-only - see "Status" above).

## What this app actually does

1. On launch: generates (or loads from Keychain) its own Sessionless
   keypair, self-registers with fount (`Config.fountBase`), and starts
   listening on `Config.listenPort` for incoming casts.
2. Plays `Config.hlsURL` full-screen via `AVPlayer`/`VideoPlayer` - defaults
   to a public Apple sample stream so there's something real to watch out of
   the box. Point it at wherever your actual OBS output ends up (OBS -> RTMP
   -> a media server producing HLS - that relay isn't built here, see
   `../../streaming/README.md`).
3. On receiving a cast at `/magic/spell/throwPotion` (see `GatewayServer.swift`):
   builds and signs its own gateway entry (`{timestamp, uuid, minimumCost:
   0, ordinal}`, message format must match fount's `resolve()`
   reconstruction exactly), appends it to `spell.gateways`, and POSTs the
   updated spell to `Config.nextHopURL` (C, the streaming server) -
   synchronously, mirroring the blocking style the rest of this demo chain
   uses.
4. Relays whatever comes back (fount's eventual resolution, having passed
   through every other hop) as the HTTP response to whoever cast at this
   app, and flashes a brief on-screen icon over the video if it succeeded.

## Local network permission

tvOS prompts for local network access on first launch
(`NSLocalNetworkUsageDescription` in `project.yml`) - accept it, or the
gateway listener won't be reachable. `NSAppTransportSecurity.
NSAllowsLocalNetworking` is also set, since this demo's other hops are all
plain HTTP on localhost - tighten this before using real, non-local
endpoints.

## Related

- Full chain + setup order: `../../streaming/demo/README.md`
- MAGIC protocol / the demo this implements: `../../../README-DEV.md`
