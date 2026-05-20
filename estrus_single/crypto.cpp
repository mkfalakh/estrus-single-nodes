#include "crypto.h"
#include "mbedtls/sha256.h"
#include "mbedtls/md.h"

String sha256(String input) {
  uint8_t hash[32];
  mbedtls_sha256_context ctx;

  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0); // 🔥 FIX
  mbedtls_sha256_update(&ctx, (const uint8_t*)input.c_str(), input.length()); // 🔥 FIX
  mbedtls_sha256_finish(&ctx, hash); // 🔥 FIX
  mbedtls_sha256_free(&ctx);

  String output = "";
  char buf[3];

  for (int i = 0; i < 32; i++) {
    sprintf(buf, "%02x", hash[i]);
    output += buf;
  }

  return output;
}

String hashPassword(String password, String salt) {
  return sha256(password + salt);
}