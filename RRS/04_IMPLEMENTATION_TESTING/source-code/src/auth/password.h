/*
 * password.h - salted password hashing (SEC-001).
 * PBKDF2-HMAC-SHA256 via OpenSSL, random 16-byte salt per password.
 * Encoded form: pbkdf2$<iterations>$<salt hex>$<hash hex>
 */
#ifndef RRS_PASSWORD_H
#define RRS_PASSWORD_H

#include <string>

namespace rrs {

/* Hash a password with a fresh random salt. Returns false on crypto failure. */
bool password_hash(const std::string& password, std::string& encoded_out);

/* Check a password against an encoded hash (constant-time compare).
 * Returns false for a wrong password OR a malformed encoded string. */
bool password_verify(const std::string& password, const std::string& encoded);

}  // namespace rrs

#endif
