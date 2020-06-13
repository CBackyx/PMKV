#ifndef ENGINE_EXAMPLE_DOOR_PLATE_H_
#define ENGINE_EXAMPLE_DOOR_PLATE_H_
#include <string.h>
#include <stdint.h>
#include <map>
#include <string>
#include "engine.h"
#include "data_store.h"

static const uint32_t kMaxKeyLen = 32;

struct Item {
  Item() : key_size(0), in_use(0) {
  }
  int value;
  int key_size;
  char key[kMaxKeyLen];
  uint8_t in_use;
};

// Hash index for key
class DoorPlate  {
 public:
    explicit DoorPlate(const std::string& path);
    ~DoorPlate();

    RetCode Init();

    RetCode AddOrUpdate(const std::string& key, int value);

    RetCode Delete(const std::string& key);

    RetCode Read(const std::string& key, int& value);

    int flushMem();

 private:
    std::string dir_;
    int fd_;
    Item *items_;

    int CalcIndex(const std::string& key);
};


#endif  // ENGINE_EXAMPLE_DOOR_PLATE_H_
