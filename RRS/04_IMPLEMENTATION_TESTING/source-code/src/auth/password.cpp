#include "password.h"

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>

#include <cstdlib>
#include <vector>

namespace rrs {
namespace {

const int kSaltBytes = 16;
const int kHashBytes = 32;
const int kIterations = 100000;

std::string to_hex(const unsigned char* data, size_t len) {
    static const char* digits = "0123456789abcdef";
    std::string out;
    out.reserve(len * 2);
    for (size_t i = 0; i < len; ++i) {
        out.push_back(digits[data[i] >> 4]);
        out.push_back(digits[data[i] & 0x0F]);
    }
    return out;
}

bool from_hex(const std::string& hex, std::vector<unsigned char>& out) {
    if (hex.empty() || hex.size() % 2 != 0) return false;
    out.clear();
    for (size_t i = 0; i < hex.size(); i += 2) {
        char pair[3] = {hex[i], hex[i + 1], '\0'};
        char* end = nullptr;
        long v = std::strtol(pair, &end, 16);
        if (*end != '\0') return false;
        out.push_back(static_cast<unsigned char>(v));
    }
    return true;
}

bool derive(const std::string& password, const unsigned char* salt, size_t salt_len,
            int iterations, unsigned char* out, size_t out_len) {
    return PKCS5_PBKDF2_HMAC(password.c_str(), static_cast<int>(password.size()),
                             salt, static_cast<int>(salt_len), iterations,
                             EVP_sha256(), static_cast<int>(out_len), out) == 1;
}

}  // namespace

bool password_hash(const std::string& password, std::string& encoded_out) {
    unsigned char salt[kSaltBytes];
    unsigned char hash[kHashBytes];
    if (RAND_bytes(salt, kSaltBytes) != 1) return false;
    if (!derive(password, salt, kSaltBytes, kIterations, hash, kHashBytes)) return false;
    encoded_out = "pbkdf2$" + std::to_string(kIterations) + "$" +
                  to_hex(salt, kSaltBytes) + "$" + to_hex(hash, kHashBytes);
    return true;
}

bool password_verify(const std::string& password, const std::string& encoded) {
    std::vector<std::string> parts;
    size_t start = 0;
    while (true) {
        size_t pos = encoded.find('$', start);
        if (pos == std::string::npos) { parts.push_back(encoded.substr(start)); break; }
        parts.push_back(encoded.substr(start, pos - start));
        start = pos + 1;
    }
    if (parts.size() != 4 || parts[0] != "pbkdf2") return false;

    int iterations = std::atoi(parts[1].c_str());
    if (iterations <= 0) return false;

    std::vector<unsigned char> salt, expected;
    if (!from_hex(parts[2], salt) || !from_hex(parts[3], expected)) return false;
    if (expected.size() != static_cast<size_t>(kHashBytes)) return false;

    unsigned char actual[kHashBytes];
    if (!derive(password, salt.data(), salt.size(), iterations, actual, kHashBytes)) return false;

    return CRYPTO_memcmp(actual, expected.data(), kHashBytes) == 0;
}

}  // namespace rrs
