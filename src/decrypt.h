#pragma once
#include <cstdint>
#include <vector>

std::vector<uint8_t> decrypt(
  const uint8_t* blob,
  size_t blob_size);
