#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <map>
#include <cstring>
#include <iostream>
#include <utility>
#include <unistd.h>
#include "utils.h"
#include "door_plate.h"

#include "headers.h"

static const uint32_t kMaxDoorCnt = 1024 * 32;
static const char kMetaFileName[] = "META";
static const int kMaxRangeBufCount = kMaxDoorCnt;

static bool ItemKeyMatch(const Item &item, const std::string& target) {
  if (target.size() != item.key_size
      || memcmp(item.key, target.data(), item.key_size) != 0) {
    // Conflict
    return false;
  }
  return true;
}

static bool ItemTryPlace(const Item &item, const std::string& target) {
  if (item.in_use == 0) {
    return true;
  }
  return ItemKeyMatch(item, target);
}

DoorPlate::DoorPlate(const std::string& path)
  : dir_(path),
  fd_(-1),
  items_(NULL) {
  }

RetCode DoorPlate::Init() {
  bool new_create = false;
  const int map_size = kMaxDoorCnt * sizeof(Item);

  if (!FileExists(dir_)
      && 0 != mkdir(dir_.c_str(), 0755)) {
    return kIOError;
  }

  std::string path = dir_ + "/" + kMetaFileName;
  int fd = open(path.c_str(), O_RDWR, 0644);
  if (fd < 0 && errno == ENOENT) {
    // not exist, then create
    fd = open(path.c_str(), O_RDWR | O_CREAT, 0644);
    if (fd >= 0) {
      new_create = true;
      if (posix_fallocate(fd, 0, map_size) != 0) {
        std::cerr << "posix_fallocate failed: " << strerror(errno) << std::endl;
        close(fd);
        return kIOError;
      }
    }
  }
  if (fd < 0) {
    return kIOError;
  }
  fd_ = fd;

  void* ptr = mmap(NULL, map_size, PROT_READ | PROT_WRITE,
      MAP_SHARED, fd_, 0);
  if (ptr == MAP_FAILED) {
    std::cerr << "MAP_FAILED: " << strerror(errno) << std::endl;
    close(fd);
    return kIOError;
  }
  if (new_create) {
    memset(ptr, 0, map_size);
  }

  items_ = reinterpret_cast<Item*>(ptr);
  return kSucc;
}

DoorPlate::~DoorPlate() {
  if (fd_ > 0) {
    const int map_size = kMaxDoorCnt * sizeof(Item);
    munmap(items_, map_size);
    close(fd_);
  }
}

// Very easy hash table, which deal conflict only by try the next one
int DoorPlate::CalcIndex(const std::string& key) {
  uint32_t jcnt = 0;
  int index = StrHash(key.data(), key.size()) % kMaxDoorCnt;
  while (!ItemTryPlace(*(items_ + index), key)
      && ++jcnt < kMaxDoorCnt) {
    index = (index + 1) % kMaxDoorCnt;
  }

  if (jcnt == kMaxDoorCnt) {
    // full
    return -1;
  }
  return index;
}

RetCode DoorPlate::AddOrUpdate(const std::string& key, int value) {
  if (key.size() > kMaxKeyLen) {
    return kInvalidArgument;
  }

  int index = CalcIndex(key);
  if (index < 0) {
    return kFull;
  }

  Item* iptr = items_ + index;
  if (iptr->in_use == 0) {
    // new item
    memcpy(iptr->key, key.data(), key.size());
    iptr->key_size = key.size();
    iptr->in_use = 1;  // Place
  }
  iptr->value = value;
  return kSucc;
}

RetCode DoorPlate::Delete(const std::string& key) {
  if (key.size() > kMaxKeyLen) {
    return kInvalidArgument;
  }

  int index = CalcIndex(key);
  if (index < 0) {
    return kFull;
  }

  Item* iptr = items_ + index;
  iptr->in_use == 0;

  return kSucc;
}

RetCode DoorPlate::Read(const std::string& key, int& value) {
  int index = CalcIndex(key);
  if (index < 0
      || !ItemKeyMatch(*(items_ + index), key)) {
    return kNotFound;
  }

  value = (items_ + index)->value;
  return kSucc;
}

int DoorPlate::flushMem() {
  const int map_size = kMaxDoorCnt * sizeof(Item);
  return msync(items_, map_size, MS_SYNC);
}

void DoorPlate::PrintState() {
  const int map_size = kMaxDoorCnt * sizeof(Item);
  msync(items_, map_size, MS_SYNC);
  for (int i = 0; i < kMaxDoorCnt; ++i) {
    if (items_[i].in_use != 0) {
      printf("%s %d\n", items_[i].key, items_[i].value);
    }
  }
}