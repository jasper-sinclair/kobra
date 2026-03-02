#pragma once
#ifndef EMBED_NNUE
#include <fcntl.h>
#include <sys/stat.h>
#include "crypto_key.h"

#ifdef _WIN64
#include <Windows.h>
using fd = HANDLE;
#define FD_ERR INVALID_HANDLE_VALUE
using map_t = HANDLE;
#else
#include <sys/mman.h>
#include <unistd.h>
using fd = int;
#define FD_ERR -1
using map_t = size_t;
#endif

inline fd open_file(
  const char* name){
  #ifndef _WIN64
  return open(name,O_RDONLY);
  #else
  return CreateFile(name, GENERIC_READ, FILE_SHARE_READ,
    nullptr, OPEN_EXISTING,
    FILE_FLAG_RANDOM_ACCESS,nullptr);
  #endif
}

inline void close_file(
  fd file){
  #ifndef _WIN64
  close(file);
  #else
  CloseHandle(file);
  #endif
}

inline size_t file_size(
  fd file){
  #ifndef _WIN64
  struct stat statbuf;
  fstat(file,&statbuf);
  return statbuf.st_size;
  #else
  DWORD size_high;
  const DWORD size_low = GetFileSize(file,&size_high);
  return (static_cast<uint64_t>(size_high) << 32) | size_low;
  #endif
}

inline const void* map_file(
  fd file,
  map_t* map){
  #ifndef _WIN64
  *map = file_size(file);
  void* data = mmap(nullptr,*map,PROT_READ,MAP_SHARED,file,0);
  return data == MAP_FAILED?nullptr:data;
  #else
  DWORD size_high;
  const DWORD size_low = GetFileSize(file,&size_high);
  *map = CreateFileMapping(file,nullptr, PAGE_READONLY,
    size_high,size_low,nullptr);
  if (*map == nullptr) return nullptr;
  return MapViewOfFile(*map, FILE_MAP_READ,0,0,0);
  #endif
}

inline void unmap_file(
  const void* data,
  map_t map){
  if (! data) return;
  #ifndef _WIN64
  munmap((void*)data,map);
  #else
  UnmapViewOfFile(data);
  CloseHandle(map);
  #endif
}
#else
#include "decrypt.h"
#include <vector>
extern "C"{
#include "../../shared/crypto/aes.h"
#include "../../shared/crypto/sha256.h"
}
#ifndef INCBIN_PREFIX
#define INCBIN_PREFIX g_
#endif
#ifndef INCBIN_STYLE
#define INCBIN_STYLE INCBIN_STYLE_SNAKE
#endif
#include "incbin.h"
INCBIN(embedded_nnue,"network.bin");
extern const unsigned char g_embedded_nnue_data[];
extern const unsigned int g_embedded_nnue_size;
#endif
