#ifndef ESPNOW_PROTOCOL_H
#define ESPNOW_PROTOCOL_H

#include <stdint.h>
#include "sessionless.h"

// Wire protocol between the wand (caster) and the target (gateway) over
// ESP-NOW. ESP-NOW's payload cap is 250 bytes, so these are packed structs,
// not JSON - JSON is only used on the target's WiFi hop to fount.
//
// This file must stay byte-identical between espnow-wand and espnow-target
// (they're separate PlatformIO projects, so it's duplicated rather than
// shared - if you change one, change both).

enum MsgType : uint8_t {
  MSG_REGISTER_REQUEST  = 1, // wand -> target
  MSG_REGISTER_RESPONSE = 2, // target -> wand
  MSG_CAST_SPELL         = 3, // wand -> target
  MSG_CAST_RESULT        = 4, // target -> wand
  MSG_TIME_SYNC          = 5  // target -> wand, broadcast periodically
};

struct __attribute__((packed)) RegisterRequest {
  uint8_t msgType; // = MSG_REGISTER_REQUEST
  uint8_t pubKey[PUBLIC_KEY_SIZE_BYTES];   // 33 bytes, compressed
  uint64_t timestamp;                       // ms, formatted as decimal string when signing
  uint8_t signature[SIGNATURE_SIZE_BYTES]; // 64 bytes, signs (timestamp + hex(pubKey))
};

struct __attribute__((packed)) RegisterResponse {
  uint8_t msgType; // = MSG_REGISTER_RESPONSE
  uint8_t success; // 1 or 0
  char casterUUID[37]; // uuid string + null terminator, empty if success == 0
};

struct __attribute__((packed)) CastSpell {
  uint8_t msgType; // = MSG_CAST_SPELL
  char casterUUID[37];
  uint64_t timestamp;
  uint32_t ordinal;
  uint8_t casterSignature[SIGNATURE_SIZE_BYTES]; // 64 bytes
};

struct __attribute__((packed)) CastResult {
  uint8_t msgType; // = MSG_CAST_RESULT
  uint8_t success; // 1 or 0
};

// fount rejects any request whose signed timestamp is more than 5 minutes
// from its own wall-clock time (a global middleware, not just a resolve()
// check) - the wand has no WiFi/NTP of its own, so the target (which does
// sync via NTP) broadcasts real epoch time periodically for the wand to
// calibrate against. Without this, both registration and casting fail with
// fount's generic "no time like the present" rejection.
struct __attribute__((packed)) TimeSync {
  uint8_t msgType; // = MSG_TIME_SYNC
  uint64_t epochMs; // target's real epoch time (ms) at the moment of send
};

// Spell constants - baked into both firmwares at compile time rather than
// sent over the wire, since they're always the same for this demo. Must
// match the wandCast spellbook entry in fount/src/server/node/spellbooks/
// spellbook.js (cost: 50, mp: true) and sharon/tests/magic/wand-cast-hosted.js.
#define SPELL_NAME "wandCast"
#define SPELL_TOTAL_COST 50
#define SPELL_MP true
#define GATEWAY_MINIMUM_COST 0

#endif
