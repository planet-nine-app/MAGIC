# ESP-NOW Wand + Target - Physical MAGIC over ESP-NOW

**Status: working end-to-end.** Registration (wand → target → fount) and
spell casting (`wandCast`, full round trip to the hosted allyabase gateway)
both confirmed on real hardware. See "Debugging journey" below before
touching this code - nearly everything that looks unusual in here exists
because of a specific, hard-won lesson.

Two Unexpected Maker boards casting a real MAGIC spell: a **wand** (TinyS3)
that only ever speaks ESP-NOW, and a **target** (Pro S3) that relays
everything to [fount](https://github.com/planet-nine-app/fount) over WiFi
and reacts locally to the result.

Sibling project: `../espnow-target/` (this README covers both - they're a
matched pair for one demo, not independent examples).

## Overview

```
┌────────────────┐   ESP-NOW    ┌─────────────────┐   HTTPS    ┌────────┐
│  Wand (caster)  │◄───────────►│ Target (gateway) │◄──────────►│ fount  │
│  no WiFi at all │  broadcast   │  WiFi + HTTPS    │  wandCast  │        │
└────────────────┘              └─────────────────┘            └────────┘
      NeoPixel: blue = sent           NeoPixel: green = listening, purple = spell received (5s)
```

The wand is deliberately WiFi-free, even for its own one-time registration
with fount - the target relays that too. A wand should only ever speak into
the air. This also means the demo shows off the MAGIC gateway-relay pattern
twice: once for registration, once for the actual spell cast.

The `wandCast` spell (cost 50, `mp: true`, no destinations besides fount
itself) is registered in `fount/src/server/node/spellbooks/spellbook.js` and
deployed to `https://allyabase-gateway.netlify.app`. The exact wire contract
was validated end-to-end from a laptop before any firmware was written - see
`sharon/tests/magic/wand-cast-hosted.js`, which is the ground-truth reference
for the signed-message construction both firmwares implement.

## Hardware

- Wand: [Unexpected Maker TinyS3](https://unexpectedmaker.com/shop.html#!/TinyS3-D/p/759468759) (ESP32-S3, `board = um_tinys3`)
- Target: [Unexpected Maker Pro S3](https://unexpectedmaker.com/shop.html#!/ProS3-D/p/759221737) (ESP32-S3, `board = um_pros3`)
- USB-C cables
- BOOT button (built-in, GPIO0) is the wand's cast trigger - no extra wiring needed

### Onboard NeoPixel

**GPIO18** (data) with a **GPIO17** power-enable pin that must be driven
HIGH - confirmed directly from the official board definitions shipped with
the ESP32 Arduino core (`variants/um_tinys3/pins_arduino.h` and
`variants/um_pros3/pins_arduino.h`, both `RGB_DATA=18` / `RGB_PWR=17`),
not just secondhand docs. Identical wiring on both boards.

### Wand's RF switch - the actual root cause of every wand→target failure

TinyS3[D] has both an onboard chip antenna and a u.FL connector for an
external one, switchable at runtime via GPIO38 (`LOW` = onboard/default,
`HIGH` = external). **This specific TinyS3 unit's onboard antenna was
defective or badly underperforming** - the wand's `esp_now_send()` always
reported local `SUCCESS`, but nothing it transmitted ever reached the
target, across dozens of tests and every software variable we could think
of (channel matching, WiFi power-save, AP/STA mode, peer channel
declarations, a from-scratch minimal test sketch). Switching the RF switch
to the external path - **even with nothing physically plugged into the
u.FL connector** - fixed reception completely and immediately. The wand's
`initEspNow()` sets this unconditionally:

```cpp
#define RF_SWITCH 38
pinMode(RF_SWITCH, OUTPUT);
digitalWrite(RF_SWITCH, HIGH); // external antenna path
```

If you swap in a different TinyS3 unit and it has a healthy onboard
antenna, this is harmless to leave in (an unterminated external path still
radiates something, evidently better than this unit's onboard trace did) -
but if you want to go back to pure-onboard, set it `LOW` instead. Physically
attaching an external antenna to the u.FL connector should only improve on
this further. Board-specific pin per the `unexpectedmaker/UM SeriesD
Helper` library (`RF_SWITCH` is 38 on TinyS3[D], 11 on Pro S3[D] - Pro S3
has the same switchable-antenna feature, unused here since its onboard
antenna was never in question).

## Wire protocol

### ESP-NOW hop (wand <-> target)

Packed C structs, not JSON - ESP-NOW's payload cap is 250 bytes. Broadcast
peer (`FF:FF:FF:FF:FF:FF`) on both sides, no MAC pairing/config needed; the
largest struct (`CastSpell`) is 114 bytes.

| msgType | Struct | Direction | Purpose |
|---|---|---|---|
| 1 | `RegisterRequest` | wand → target | wand's pubKey + self-signed registration request |
| 2 | `RegisterResponse` | target → wand | assigned `casterUUID` (or failure) |
| 3 | `CastSpell` | wand → target | signed spell cast |
| 4 | `CastResult` | target → wand | success/failure, so the wand's own NeoPixel can mirror the target's |

See `src/espnow_protocol.h` (identical in both projects - it's duplicated,
not shared, since they're separate PlatformIO builds; if you change one,
change both) for exact field layouts and the spell constants (`wandCast`,
cost 50, `mp: true`) baked into both firmwares at compile time.

### HTTPS hop (target -> fount)

Standard MAGIC spell payload - see `sharon/tests/magic/wand-cast-hosted.js`
for the reference implementation this mirrors field-for-field. The target
builds the full `{timestamp, spell, casterUUID, totalCost, mp, ordinal,
casterSignature, gateways: [...]}` body itself; the wand only ever transmits
its own signature and the values that went into it, never the JSON.

## Setup

1. **Target first.** Copy `espnow-target/src/config.h.example` to
   `espnow-target/src/config.h` and fill in your WiFi credentials:
   ```cpp
   #define WIFI_SSID "..."
   #define WIFI_PASSWORD "..."
   #define GATEWAY_BASE "https://allyabase-gateway.netlify.app/fount"
   ```
   `config.h` is gitignored.

2. Flash the target:
   ```bash
   cd espnow-target
   pio run --target upload
   pio device monitor
   ```
   Expected output: WiFi connects, then `Syncing time via NTP...` followed by
   `NTP synced. Epoch: ...`, then a `PUT /user/create -> HTTP 200` line with
   an assigned `targetUUID`, then `ESP-NOW ready (broadcast)` and `Ready.
   Listening for the wand...`. NeoPixel goes solid green once WiFi + NTP +
   registration all succeed (dim red until then). fount rejects any request
   whose signed timestamp is more than 5 minutes from its own clock - NTP is
   what makes the timestamp real instead of "milliseconds since this board
   booted".

3. Flash the wand (no config file needed - it has no WiFi):
   ```bash
   cd ../espnow-wand
   pio run --target upload
   pio device monitor
   ```
   On first boot it prints `RF switch set to EXTERNAL antenna.` (see "Wand's
   RF switch" above - this unit's onboard antenna doesn't work reliably),
   then `Waiting for time sync from target before
   registering...` and does nothing else until it hears a `MSG_TIME_SYNC`
   broadcast from the target (every 5s, once the target's NTP sync
   succeeds) - the wand has no WiFi/NTP of its own, so it calibrates its
   own clock off the target's broadcasts to produce timestamps fount will
   accept. Both boards are hardcoded to `WIFI_CHANNEL 11` (see
   Troubleshooting if your network isn't on channel 11). Once synced
   (`Time synced from target...`), it repeatedly
   broadcasts a registration request every 3s until the target relays it to
   fount and sends back a `casterUUID`. Expect a double green flash and
   `Registered! casterUUID: ...` in Serial Monitor.

4. **Press BOOT on the wand.** Expect: wand flashes blue (sent) → target
   turns solid purple immediately on receipt, holds for 5 seconds while its
   Serial Monitor logs `POST /resolve/wandCast -> HTTP 200` and
   `{"success":true,...}`, then returns to solid green → wand also flashes
   green twice (via the `CastResult` relay back). The 5-second purple hold
   is a fixed duration regardless of success/failure - a starting point, not
   yet differentiating the two by color.

## Debugging journey

This firmware was written and largely validated *before* any hardware was
in hand - the backend contract was proven with a laptop-only Node script
first (`sharon/tests/magic/wand-cast-hosted.js`), then the firmware was
authored against that known-good contract. Getting it working on real
boards surfaced a chain of real, non-obvious bugs, roughly in the order
they were found:

1. **`secp256k1_ec_pubkey_create()` was hand-patched with stray `return 1;`
   statements** in the vendored `lib/secp256k1-embedded/secp256k1/src/
   secp256k1.c` (both projects' copies) that skipped the actual elliptic
   curve math entirely, silently leaving the pubkey struct all-zero while
   still reporting success. Manifested as an `illegal argument:
   !secp256k1_fe_is_zero(&ge->x)` abort during key generation. Fixed by
   removing the stray returns - real upstream `bitcoin-core/secp256k1`
   logic, once actually allowed to run, works fine.
2. **fount has a global timestamp-freshness middleware** (`allowedTimeDifference
   = 300000` in `fount.js`) that rejects any request - not just spell
   resolution, registration too - whose signed `timestamp` is more than 5
   minutes from the server's real clock. Both firmwares originally signed
   with `millis()` (time since boot), nowhere near a real epoch. Fixed by
   having the target sync real time via NTP and broadcast it to the wand
   over ESP-NOW (`MSG_TIME_SYNC`) every 5s, since the wand has no WiFi of
   its own to get real time any other way.
3. **The target's registration relay called `http.POST()` against a route
   fount only registers as `app.put()`** - Express's default response to
   an unmatched method is an HTML 404 (`Cannot POST /user/create`), which
   fails to parse as JSON and reads identically to a real network failure.
   Confirmed by comparing `curl -X POST` vs `-X PUT` against the live
   gateway directly. Fixed by using `http.PUT()` for that one call.
4. **fount's `resolve()` used `res.status(900)`** for every spell failure -
   900 isn't a valid HTTP status code, and while Node/Express tolerate it
   silently, Netlify's Lambda response encoding chokes on it (`invalid
   status code returned from lambda: 900`), turning every failed cast into
   an opaque 502 instead of parseable JSON. Fixed in `allyabase/deployment/
   fount` (the `netlify-packaging` branch actually bundled by the live
   gateway) and redeployed.
5. **PlatformIO's port auto-detection picks the wrong board** when both the
   wand and target are connected simultaneously - confirmed directly (a
   target upload grabbed the wand's port). Fixed by pinning `upload_port`/
   `monitor_port` explicitly in both `platformio.ini` files rather than
   relying on auto-detection.
6. **The wand's onboard antenna was defective** - see "Wand's RF switch"
   above. This was the last and biggest one: every software variable
   (channel matching confirmed multiple independent ways, WiFi power-save,
   AP/STA mode, peer channel declarations, a from-scratch minimal test
   sketch with zero shared code) was ruled out before landing on hardware.
   The tell in hindsight: the wand's `esp_now_send()` always reported local
   `SUCCESS` (that only proves the frame was handed to the radio, not that
   it was usefully radiated - there's no link-layer ACK for broadcast
   frames), and the wand also struggled to associate with a real WiFi AP in
   isolated testing, a much higher-power operation than raw ESP-NOW.

A dead end worth knowing about so it isn't retried: the wand originally
scanned channels 1-13 at runtime to find the target automatically, since at
the time channel mismatch looked like a plausible cause. That scanner
introduced a real, hard-to-fully-close race between its hop timer (running
in `loop()`) and the ESP-NOW receive callback (running on a different
FreeRTOS task) - fixed with an authoritative re-pin, then abandoned
entirely once direct measurement showed the target's channel was
consistently 11 the whole time, making the scan pure unnecessary
complexity. Both boards now hardcode `WIFI_CHANNEL 11`.

## Troubleshooting

- **Wand never registers**: confirm the target is already flashed, powered,
  and shows `Ready. Listening for the wand...` before powering the wand -
  the wand's retry loop is patient (every 3s) but won't succeed against a
  target that isn't listening yet.
- **Target stuck on `Syncing time via NTP...`**: it needs real internet
  egress (UDP/123), not just local network connectivity - same requirement
  as reaching `allyabase-gateway.netlify.app`. It retries every 5s in
  `loop()`, so it'll recover on its own once egress is available.
- **Wand stuck on "Waiting for time sync from target..."**: the target must
  reach `NTP synced.` before it starts broadcasting `MSG_TIME_SYNC` - check
  the target's Serial Monitor for NTP trouble first. If NTP is fine and it's
  still stuck, check `WIFI_CHANNEL` in `espnow-wand/src/main.cpp` (see the
  note below) against what the target's Serial Monitor prints as `WiFi
  channel: N` right after it connects - they must match exactly, and the
  wand won't discover a mismatch on its own.
- **`WIFI_CHANNEL` is hardcoded to 11**: both boards need to be on the same
  ESP-NOW channel, and only the target can tell you what that actually is
  (it's whatever channel your WiFi AP happens to be using - check its
  Serial Monitor for `WiFi channel: N` right after it connects). An earlier
  version of this firmware had the wand scan channels 1-13 at runtime to
  find the target automatically, but that added a real, hard-to-fully-close
  race between the scan-hop timer and the ESP-NOW receive callback (they
  run on different FreeRTOS tasks) - and was solving a problem that wasn't
  actually occurring, since the target's channel was consistently 11 across
  every direct measurement during testing. If your WiFi network's channel
  ever changes, update `WIFI_CHANNEL` in `espnow-wand/src/main.cpp` to
  match and reflash, rather than resurrecting the scanner.
- **`{"error":"no time like the present"}` from fount**: a signed timestamp
  was more than 5 minutes from fount's clock - almost always means either
  the target's NTP sync hasn't actually landed yet (check its Serial output)
  or the wand cast before ever receiving a `MSG_TIME_SYNC` broadcast.
- **Target shows `HTTP 400 {"success":false}` on cast**: the caster/gateway
  signature didn't verify, or MP was insufficient. Re-flash the wand to force
  fresh keys + re-registration (NVS persists across resets but not across a
  full chip erase / `pio run --target erase`).
- **NeoPixel never lights**: double-check `RGB_LED_POWER_PIN` (17) is
  actually being driven HIGH before `pixel.begin()` - it's a known trap on
  these boards that the data pin alone does nothing without it.
- **Registration relay times out repeatedly**: check target's WiFi actually
  has internet egress to `allyabase-gateway.netlify.app`, not just local
  network connectivity.
- **Upload/monitor grabs the wrong board, or fails with "port busy"**: both
  `platformio.ini` files pin `upload_port`/`monitor_port` explicitly
  (confirmed necessary - auto-detection picks the wrong board when both
  are plugged in at once). Run `pio device list` and update the pinned
  port if it's stopped matching (USB port paths renumber on replug/reboot).
  "Port busy" usually just means a `pio device monitor` session is still
  open on that port in another terminal - close it first.
- **Want to test the backend independent of hardware**: run
  `node sharon/tests/magic/wand-cast-hosted.js` from the repo root - it
  exercises the identical fount contract from a laptop.

## Related

- MAGIC protocol: `../../../README-DEV.md`
- Reference embedded examples: `../lora-sessionless/`, `../lora-gateway-umpros3/`, `../ble-sessionless/`
- Sessionless C++ (vendored into `lib/sessionless/` in both projects): `../../../../sessionless/src/cpp/`
- fount spellbook: `../../../../fount/src/server/node/spellbooks/spellbook.js`
