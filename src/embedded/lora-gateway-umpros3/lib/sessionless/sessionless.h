#ifndef SESSIONLESS_H
#define SESSIONLESS_H

#include <cstddef>
#include <cstring>
#include <stdint.h>
#include <string>

#define SHA256_SIZE_BYTES 32
#define PRIVATE_KEY_SIZE_BYTES SHA256_SIZE_BYTES
#define PUBLIC_KEY_SIZE_BYTES 33
#define SIGNATURE_SIZE_BYTES 64

struct Keys
{
    uint8_t publicKey[PUBLIC_KEY_SIZE_BYTES];
    uint8_t privateKey[PRIVATE_KEY_SIZE_BYTES];
};

namespace sessionless
{
    bool generateKeys(Keys &keys);

    bool sign(std::string message,
              const unsigned char *privateKey,
              unsigned char *signature);

    bool verifySignature(const unsigned char *signature,
                         const unsigned char *publicKey,
                         std::string message);
};

// C-style wrapper functions for easier usage
inline bool generateKeys(uint8_t *privateKey, uint8_t *publicKey) {
    Keys keys;
    if (sessionless::generateKeys(keys)) {
        memcpy(privateKey, keys.privateKey, PRIVATE_KEY_SIZE_BYTES);
        memcpy(publicKey, keys.publicKey, PUBLIC_KEY_SIZE_BYTES);
        return true;
    }
    return false;
}

inline bool sign(const uint8_t *message, size_t messageLen,
                 const uint8_t *privateKey, uint8_t *signature) {
    std::string msg((const char*)message, messageLen);
    return sessionless::sign(msg, privateKey, signature);
}

inline bool verify(const uint8_t *message, size_t messageLen,
                   const uint8_t *signature, const uint8_t *publicKey) {
    std::string msg((const char*)message, messageLen);
    return sessionless::verifySignature(signature, publicKey, msg);
}

#endif
