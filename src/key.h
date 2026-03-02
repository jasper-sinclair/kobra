#pragma once
#include <cstdint>
void derive_key(uint8_t key[32]);
void xor_stream(uint8_t* data, size_t size, const uint8_t key[32]);