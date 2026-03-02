#pragma once
#include <cstdint>
#include <vector>

std::vector<uint8_t> decrypt_blob(
  const uint8_t* blob,
  size_t blob_size);

#ifdef PROTECTED_NNUE
uint8_t* load_protected_nnue(
  size_t* out_size);
#endif