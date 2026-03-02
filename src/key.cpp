#include "key.h"
#include <cstdint>
#include <cstring>

void derive_key(uint8_t key[32]) {
  static constexpr uint8_t seed[32]={
      0x97, 0x21, 0xd3, 0x31,
      0xab, 0x39, 0xee, 0x9d,
      0x5a, 0x6f, 0x2b, 0x61,
      0xfd, 0x50, 0xa1, 0x9d,
      0x5e, 0xc0, 0x7f, 0xd7,
      0x17, 0x13, 0x1e, 0x42,
      0x25, 0xca, 0xab, 0xa3,
      0xf0, 0x65, 0x2c, 0x66
  };
  memcpy(key, seed, 32);
}

void xor_stream(uint8_t* data, size_t size, const uint8_t key[32]) {
  uint64_t s[4];
  std::memcpy(s, key, 32);
  uint64_t state=s[0] ^ s[1] ^ s[2] ^ s[3];
  for (size_t i=0; i < size; ++i) {
    state^=state << 13;
    state^=state >> 7;
    state^=state << 17;
    data[i]^=static_cast<uint8_t>(state);
  }
}