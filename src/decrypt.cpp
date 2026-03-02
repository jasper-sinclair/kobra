#include "decrypt.h"
#include <cstring>
#include <vector>
#include "key.h"

std::vector<uint8_t> decrypt(
  const uint8_t* blob,
  size_t blob_size) {
  uint8_t key[32];
  derive_key(key);

  std::vector<uint8_t> decrypted(blob, blob + blob_size);

  xor_stream(decrypted.data(), decrypted.size(), key);

  return decrypted;
}
