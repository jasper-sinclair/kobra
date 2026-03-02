#include <cstring>
#include <vector>

extern "C" {
#include "../../crypto/aes.h"
}

#include "decrypt.h"
#include "key.h"

std::vector<uint8_t> decrypt(
  const uint8_t* blob,
  const size_t blob_size) {
  uint8_t key[32];
  derive_key(key);
  const uint8_t* iv=blob;
  const uint8_t* ciphertext=blob + 16;
  const size_t size=blob_size - 16;

  std::vector decrypted(
    ciphertext,
    ciphertext + size);

  AES_ctx ctx;
  AES_init_ctx_iv(&ctx, key, iv);
  AES_CTR_xcrypt_buffer(
    &ctx,
    decrypted.data(),
    static_cast<uint32_t>(size));

  memset(key, 0, sizeof(key));

  return decrypted;
}
