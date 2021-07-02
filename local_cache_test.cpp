/***********************************************************************
 * Copyright 2019, All rights reserved
 * File     :
 * Brief    :
 * Create on:
 * Author   :
 * Version  :
 * History  :
 ***********************************************************************/

#include "local_cache_test.h"
namespace LOCALCACHE {
CTestDataCache::CTestDataCache() : init_(false) {}

void CTestDataCache::CheckInit() {
  if (!init_) {
    const std::string& shm_key = "1234";
    local_cache_shm_.Init(atoi(shm_key.c_str()));

    init_ = true;
  }
}

int CTestDataCache::Get(std::string* value) {
  CheckInit();

  value->clear();

  const TestData* p_test_data = local_cache_shm_.GetReadData();
  if (NULL == p_test_data) {
    printf("读取共享内存, ptr of read buffer is NULL \n");
    return -1;
  }
  //这里假设延迟很严重，写也开始操作这块内存，读写就有线程安全问题了
  value->append(p_test_data->data, p_test_data->data + p_test_data->size);
  return 0;
}

int CTestDataCache::Set(const std::string& value) {
  CheckInit();

  // 容量告警
  if (static_cast<float>(value.size()) / sizeof(TestData) >= 0.8) {
    // SendWarning("CTestDataCache-Set: 容量预告警，阈值[0.8]");
  }

  TestData* p_test_data = local_cache_shm_.TryLockWriteData();
  if (NULL == p_test_data) {
    ErrorLog("设置共享内存, ptr of write buffer is NULL");
    return -1;
  }

  if (value.size() > sizeof(p_test_data->data)) {
    std::string str_warning;
    str_warning = str_warning + " CTestDataCache-Set: 容量不足 [" + std::to_string(value.size()) +
                  "] > [" + std::to_string(sizeof(p_test_data->data)) + "]";

    // SendWarning(str_warning);
    return -2;
  }

  memcpy(p_test_data->data, value.c_str(), value.size());
  p_test_data->size = value.size();

  int expire_time = 3600;
  p_test_data->expire_time = time(NULL) + expire_time;

  local_cache_shm_.RelaseAndChangeWriteData();

  return 0;
}
}  // namespace LOCALCACHE

int main() {
  LOCALCACHE::CTestDataCache test;
  return 0;
}
