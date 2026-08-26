// Bundled into crypto.js and evaluated inside a JavaScriptCore JSContext by
// MagicSessionless.swift. NOT a copy of any existing vendored crypto.js in
// this ecosystem (both known copies - sessionless/src/swift/client-ios/.../
// crypto.js and The Advancement's Shared (Extension)/Resources/crypto.js -
// have real signing bugs: the first truncates every signed message to 32
// characters via a broken utf8ToBytes, the second doesn't zero-pad the r/s
// signature components to 32 bytes each before concatenating them, which
// intermittently produces a malformed signature whenever either component
// happens to have a leading zero byte). This is instead a from-scratch,
// minimal wrapper around the exact same `ethereum-cryptography` primitives
// sessionless-node itself uses (see fount/src/server/node/node_modules/
// sessionless-node/sessionless.js - generateKeys/sign/verifySignature here
// mirror that file's logic function-for-function), so a signature produced
// here verifies correctly against fount and interops with every other
// sessionless implementation in this ecosystem.
//
// generateKeys() takes a privateKey directly (rather than sessionless-node's
// async saveKeys/getKeys callback pair) since JSContext calls need to be
// synchronous - MagicSessionless.swift generates the random private key
// itself (via SecRandomCopyBytes) and passes it in.

import { secp256k1 } from 'ethereum-cryptography/secp256k1.js';
import { keccak256 } from 'ethereum-cryptography/keccak.js';
import { bytesToHex } from 'ethereum-cryptography/utils.js';

// NOT ethereum-cryptography's own utf8ToBytes - that one requires
// TextEncoder, which JavaScriptCore's JSContext doesn't provide (no WHATWG
// globals, just bare JS). MAGIC spell messages are always ASCII (decimal
// timestamps, spell names, UUIDs, "true"/"false"), so a plain charCodeAt
// encoding is byte-for-byte identical to real UTF-8 for every message this
// will ever actually sign - verified by cross-checking signatures against
// the real sessionless-node in this same session.
const utf8ToBytes = (str) => Uint8Array.from(Array.from(str).map((ch) => ch.charCodeAt(0)));

const generateKeys = (privateKeyHex) => {
  const publicKey = bytesToHex(secp256k1.getPublicKey(privateKeyHex));
  return { privateKey: privateKeyHex, publicKey };
};

const sign = (message, privateKeyHex) => {
  const messageHash = keccak256(utf8ToBytes(message));
  const signature = secp256k1.sign(messageHash, privateKeyHex);
  return signature.toCompactHex(); // zero-padded r + s, 128 hex chars - matches sessionless-node exactly
};

const verifySignature = (sig, message, pubKeyHex) => {
  const messageHash = keccak256(utf8ToBytes(message));

  let rHex = sig.substring(0, 64);
  let sHex = sig.substring(64);
  if (rHex.length % 2) rHex = '0' + rHex;
  if (sHex.length % 2) sHex = '0' + sHex;

  const signature = { r: BigInt('0x' + rHex), s: BigInt('0x' + sHex) };
  return secp256k1.verify(signature, messageHash, pubKeyHex);
};

globalThis.sessionless = { generateKeys, sign, verifySignature };
