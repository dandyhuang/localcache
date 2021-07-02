/***********************************************************************
 * Copyright 2019, All rights reserved
 * File     :
 * Brief    :
 * Create on:
 * Author   :
 * Version  :
 * History  :
 ***********************************************************************/

#ifndef LOCAL_CACHE_TEST_H_
#define LOCAL_CACHE_TEST_H_
#include <string>

#include "local_cache_shm.h"

namespace LOCALCACHE {
#define SHM_CACHE_DATA_SIZE (1024 * 1024)

struct TestData {
  uint32_t expire_time;
  unsigned size;
  char data[SHM_CACHE_DATA_SIZE];

  TestData() {
    expire_time = 60;
    size = 0;
    memset(data, '\0', sizeof(data));
  }
};

class CTestDataCache {
 public:
  CTestDataCache();
  ~CTestDataCache() {}

 public:
  int Get(std::string* sValue);
  int Set(const std::string& sValue);

 private:
  void CheckInit();

 private:
  bool init_;
  LocalCacheSHM<TestData> local_cache_shm_;
};
}  // namespace LOCALCACHE

#endif  // LOCAL_CACHE_TEST_H_
