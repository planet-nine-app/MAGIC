# BLE Sessionless Target - Physical MAGIC over BLE

**Status: builds clean (`pio run`), not yet flashed to real hardware in this
session** - no board was attached while this was written. It's a direct
structural port of `../espnow-target/`'s proven WiFi/NTP/fount-registration/
casting code, reusing the exact same deployed `wandCast` spell, so the parts
most likely to bite (secp256k1 stack sizing, fount's timestamp-freshness
window, the `PUT` vs `POST` route on `/user/create`) are already known-good.
What's new and unverified on real hardware is the BLE side. See
"What to check on first boot" below before trusting it.

## Why this exists

`../espnow-wand/` + `../espnow-target/` solve physical MAGIC for a pair of
dedicated ESP32 boards talking ESP-NOW to each other. That doesn't reach a
laptop, phone, or Apple TV - none of those are going to run custom ESP-NOW
firmware, and none of them should need MAGIC/sessionless signing logic of
their own just to trigger a spell near a piece of hardware.

BLE (specifically: a GATT peripheral any generic BLE central can write to)
is the transport that reaches all three. So this board takes a different
shape than the wand/target split: it's **one** board that owns **one**
Sessionless identity and plays caster and gateway itself. WiFi/HTTPS to
fount happens entirely on this board; the BLE hop only ever carries a
trigger (any byte, written by anything) and a plaintext JSON result
notification back out - no signing, no key material, no MAGIC-shaped
payload ever needs to exist on the client side of that hop. That's the
concrete meaning of "laptop/desktop/phone/Apple TV can all cast a spell
through this without any of them needing their own MAGIC identity."

It casts the same `wandCast` spell (cost 50, `mp: true`) already registered
in fount's spellbook and deployed to `https://allyabase-gateway.netlify.app`
- see `../espnow-wand/README.md` for how that spell and that deployment were
validated. No spellbook or backend changes are needed to bring this
transport up.

## What was already here

This directory used to hold a bare demo stub: touch a capacitive pad, BLE-
notify a plaintext `{"msg":"foo",...}` string, no signing, no fount, no spell
- paired with `../../macos/BLEBridge/`, a matching macOS BLE central that
forwarded whatever it received into `lora-server`'s `/lora` route by faking
a LoRa-gateway-shaped payload (hardcoded `signatureValid: true`) just to
reuse that project's display. That pairing is why the device name
(`MAGIC-ProS3`) and both GATT UUIDs below are unchanged - `BLEBridge` finds
this board without needing to change anything on its side of discovery.
`BLEBridge` has been updated (see its own directory) to talk the real
protocol below instead of faking one.

## Wire protocol

### BLE hop (any central <-> this board)

One GATT service, one characteristic, matching `src/macos/BLEBridge/`:

| | |
|---|---|
| Device name | `MAGIC-ProS3` |
| Service UUID | `4fafc201-1fb5-459e-8fcc-c5c9c331914b` |
| Characteristic UUID | `beb5483e-36e1-4688-b7f5-ea07361b26a8` |
| Properties | READ, WRITE, NOTIFY, INDICATE |

**Trigger a cast**: write anything to the characteristic - one byte is
enough, the content is never inspected. This is deliberate: it means a
generic BLE tool (nRF Connect, LightBlue, a one-line CoreBluetooth/Web
Bluetooth snippet) can trigger a real signed MAGIC spell cast with zero
MAGIC-specific code. The onboard BOOT button (GPIO0) triggers the exact same
path locally, same as the ESP-NOW wand's button.

**Cast result**: after casting, this board notifies connected centrals with:

```json
{"success": true, "spell": "wandCast", "deviceUUID": "..."}
```

### HTTPS hop (this board -> fount)

Standard MAGIC spell payload, self-cast (this board is its own caster, so
`gateways` is sent as an empty array - fount's `resolve()` already treats an
empty/absent `gateways` array as "no gateway hops to verify," so this needs
no backend changes):

```json
{
  "timestamp": "...", "spell": "wandCast", "casterUUID": "<this board's fount uuid>",
  "totalCost": 50, "mp": true, "ordinal": ...,
  "casterSignature": "...", "gateways": []
}
```

## NeoPixel states

| Color | Meaning |
|---|---|
| Purple (solid, at boot) | Booting |
| Dim red | WiFi/NTP not ready yet |
| Blue | Advertising, no BLE central connected |
| Green | BLE central connected, ready to cast |
| Purple (solid, 5s hold) | Casting - fixed-duration hold regardless of success/failure, same reasoning as `espnow-target` |
| Red flash x3 | Cast triggered but dropped (not registered yet, or WiFi down) |

Pin confirmation: **GPIO18 data / GPIO17 power-enable**, taken directly from
`espnow-target`'s already-verified board definition lookup for `um_pros3`
(`variants/um_pros3/pins_arduino.h`: `RGB_DATA=18`, `RGB_PWR=17`). The
original stub used **GPIO48**, a generic ESP32-S3-devkit NeoPixel pin that
was never actually confirmed against this specific board - fixed here.

## Setup

1. Copy `src/config.h.example` to `src/config.h` and fill in WiFi
   credentials (config.h is gitignored). `GATEWAY_BASE` and the spell
   constants are already correct and don't need editing unless you're
   pointing at a different fount deployment.
2. ```bash
   pio run --target upload
   pio device monitor
   ```
   Expected: WiFi connects, `Syncing time via NTP...` then `NTP synced.`,
   then `PUT /user/create -> HTTP 200` with an assigned `deviceUUID`, then
   `BLE initialized!` / `Ready. Press BOOT or write to the characteristic to
   cast.`. NeoPixel goes blue (advertising) once WiFi + NTP + registration
   all succeed, dim red until then.
3. Connect from `../../macos/BLEBridge/` (or any BLE central) - NeoPixel
   goes green once connected.
4. Trigger a cast: press BOOT, or write any byte to the characteristic from
   whatever's connected. NeoPixel goes solid purple for 5s while
   `POST /resolve/wandCast -> HTTP 200` and the JSON response are logged to
   Serial Monitor, then returns to blue/green, and connected centrals get a
   notify with the real result.

## What to check on first boot (unverified pieces)

Everything under "FOUNT HTTP" and "WIFI" in `main.cpp` is a near-verbatim
port of `espnow-target`'s already-hardware-proven code, so it should behave
identically. What's genuinely new here and worth watching closely the first
time a board is flashed:

- **BLE peripheral role while WiFi STA is also active.** `espnow-target`'s
  README documents real, hard-won ESP-NOW+WiFi coexistence issues (modem
  sleep interfering with reception, channel pinning). BLE and WiFi share the
  same 2.4GHz radio on the ESP32-S3 in a similar way - if BLE notifications
  or connections seem flaky while WiFi is active, that coexistence is the
  first thing to suspect, same category of problem as the ESP-NOW case even
  though the specific fix will likely differ (BLE isn't ESP-NOW).
- **`BLECharacteristicCallbacks::onWrite` running off the Arduino loop
  task.** It only sets a `volatile bool` flag here (deliberately, mirroring
  how `espnow-target` hands ESP-NOW receive-callback work off to `loop()`
  rather than doing HTTP calls inside a callback) - but this hasn't been
  exercised on real hardware to confirm there's no race worth caring about
  under BLE's actual callback/task model specifically.
- **`notify()` while a cast's blocking HTTPS round-trip is in flight.** The
  characteristic is only notified once, after `castSpell()` returns - a
  central shouldn't see anything until the cast fully resolves, but this
  ordering hasn't been confirmed on a real BLE stack.

## Related

- MAGIC protocol: `../../../README-DEV.md`
- Sibling client: `../../macos/BLEBridge/`
- Reference hardware pattern this was ported from: `../espnow-wand/README.md`, `../espnow-target/`
- Sessionless C++ (vendored into `lib/sessionless/`): `../../../../sessionless/src/cpp/`
