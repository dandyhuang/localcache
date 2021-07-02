// Copyright [year] <Copyright Owner>
#ifndef LOCAL_CACHE_SHM_H_
#define LOCAL_CACHE_SHM_H_

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#define InfoLog printf
#define ErrorLog printf
#define SendWarning printf

// 交换头部,每个实体都有自己的交换头部,用于判断当前哪块数据可读,另一块是否正在被写
struct SwapBuff {
  uint64_t nWriteLock;  // 写锁
  uint8_t nReadIndex;   // 读哪一块data
  uint32_t nDataSize;   // 每块数据大小
  char szBuffer[0];     // 数据buffer的开始地址,包含2块数据
};

// 共享内存的头部
struct ShmHeader {
  uint64_t init;         // 标示该块共享内存是否已经初始化完毕
  uint32_t nHeaderSize;  // 头部的长度,主要为了向后兼容
  uint32_t nEntitySize;  // 一个实体的全部长度 = 交换头部长度 + 2块 实体结构体的长度
};

template <class T>
class LocalCacheSHM {
 public:
  LocalCacheSHM() { m_pShmHeader = NULL; }
  ~LocalCacheSHM() {}

 public:
  void Init(int iShmKey) {
    InfoLog("%s|初始化共享内存, iShmKey=%d,", __FUNCTION__, iShmKey);

    // 一个实体的全部长度 = 交换头部 + 2块 实体结构体的长度
    uint32_t nEntityTotalSize = sizeof(SwapBuff) + sizeof(T) * 2;

    // 整片共享内存的长度 = 共享内存头部 + 一个实体的全部长度
    uint32_t nToalShmSize = sizeof(ShmHeader) + nEntityTotalSize;

    bool bNewShm = false;
    char* pShmHeader = GetShm(iShmKey, nToalShmSize, bNewShm);
    if (NULL == pShmHeader) {
      SendWarning("共享内存Cache:  初始化失败, 若次数较多请及时处理");
      throw("系统出了一点小问题，请稍后再试");
    }

    m_pShmHeader = reinterpret_cast<ShmHeader*>(pShmHeader);

    // 这段逻辑只有在清空共享内存的时候,或者某一台机器首次被创建的时候调用
    // 如果是新创建的,需要初识化,初始化的进程将init位设置为1.其他进程等待初识化完成
    // 谁创建,谁初始化,目的是在扩容的时候由新创建的进程自己去申请创建更大的共享内存
    // 避免由新的buff更大的进程(们)创建了共享内存而由老的buff比较小的进程(们)去完成初始化的动作
    if (bNewShm) {
      InfoLog("%s|开始共享内存初始化", __FUNCTION__);
      m_pShmHeader->nHeaderSize = sizeof(ShmHeader);
      m_pShmHeader->nEntitySize = nEntityTotalSize;

      SwapBuff* pEntity = GetEntity();
      pEntity->nDataSize = sizeof(T);
      pEntity->nReadIndex = 0;
      pEntity->nWriteLock = 0;

      m_pShmHeader->init = 1;  // 一台机器只有可能有一个进程将这个置为1
    } else {
      // 如果有两个进程都开始创建共享内存,一个进程创建完成共享内存后,必须要等它将m_pShmHeader的数据结构初始化完成
      // 即 m_pShmHeader->init = 1之后,另一个进程才能开始读写该块共享内存的操作
      InfoLog("%s|等待共享内存初识化", __FUNCTION__);
      Wait(m_pShmHeader->init, 1);
    }

    InfoLog("%s|共享内存已初始化:headerSize=%d, entitySize=%d, nDataSize=%d", __FUNCTION__,
            m_pShmHeader->nHeaderSize, m_pShmHeader->nEntitySize, GetEntity()->nDataSize);
  }

  // 得到实体头部交换buff的首地址
  SwapBuff* GetEntity() {
    char* pEntity = (reinterpret_cast<char*>(m_pShmHeader)) + m_pShmHeader->nHeaderSize;
    if (pEntity == NULL) {
      SendWarning("共享内存：获取swapping buffer指针为空, 请及时确认Cache数据");
      throw("系统出了一点小问题，请稍后再试");
    }
    return reinterpret_cast<SwapBuff*>(pEntity);
  }

  // 读取 可读部分的数据
  const T* GetReadData() {
    SwapBuff* pEntity = GetEntity();
    T* pCacheData = (T*)(pEntity->szBuffer + pEntity->nDataSize * pEntity->nReadIndex);
    if (pCacheData == NULL) {
      SendWarning("共享内存：读取数据， buffer指针为空，请及时确认Cache数据");
      throw("系统出了一点小问题，请稍后再试");
    }
    return pCacheData;
  }

  T* TryLockWriteData() {
    time_t nCurrentTime = time(NULL);
    SwapBuff* pEntity = GetEntity();
    if (!TryLock(pEntity->nWriteLock, 0, nCurrentTime)) {
      InfoLog("%s|加写锁失败", __FUNCTION__);

      time_t nLastLockTime = pEntity->nWriteLock;
      const std::string& strReleaseTime = "100";
      int nReleaseTime = atoi(strReleaseTime.c_str());
      if (nReleaseTime < 600) {  // 10分钟,设置太小会导致误解锁,所以不能设置太小
        nReleaseTime = 600;
      }

      if (nCurrentTime > nLastLockTime && nCurrentTime - nLastLockTime > nReleaseTime) {
        if (TryLock(pEntity->nWriteLock, nLastLockTime, nCurrentTime)) {
          InfoLog(
              "%s|检测到写锁过期,配置为:%d秒,当前:%lu秒,重新抢写锁成功,lock_time=%lu, "
              "curr_time=%lu",
              __FUNCTION__, nReleaseTime, nCurrentTime - nLastLockTime, nLastLockTime,
              nCurrentTime);

          SendWarning("共享内存：检测到写锁过期未及时释放，强制解除占用成功");
        } else {
          InfoLog(
              "%s|检测到写锁过期,重新抢写锁失败,配置为:%d秒,当前:%lu秒,lock_time=%lu, "
              "curr_time=%lu",
              __FUNCTION__, nReleaseTime, nCurrentTime - nLastLockTime, nLastLockTime,
              nCurrentTime);

          SendWarning("共享内存：检测到写锁过期未及时释放，强制解除占用失败");
          return NULL;
        }
      } else {
        InfoLog("%s|写锁未过期,配置为:%d秒,当前:%lu秒,lock_time=%lu, curr_time=%lu", __FUNCTION__,
                nReleaseTime, nCurrentTime - nLastLockTime, nLastLockTime, nCurrentTime);
        return NULL;
      }
    }

    uint8_t nWriteIndex = GetWriteIndex(pEntity->nReadIndex);
    T* pCacheData = (T*)(pEntity->szBuffer + pEntity->nDataSize * nWriteIndex);

    InfoLog("%s|加写锁成功,nWriteIndex=%d", __FUNCTION__, nWriteIndex);

    return pCacheData;
  }

  // 释放并交换buffer
  void RelaseAndChangeWriteData() {
    SwapBuff* pEntity = GetEntity();

    uint8_t nWriteIndex = GetWriteIndex(pEntity->nReadIndex);
    pEntity->nReadIndex = nWriteIndex;

    InfoLog("%s|释放写锁并交换读buff,新的读索引为:%d", __FUNCTION__, pEntity->nReadIndex);

    UnLock(pEntity->nWriteLock);
  }

  // 只释放写锁,不交换buffer
  void RelaseWriteData() {
    SwapBuff* pEntity = GetEntity();
    InfoLog("%s|释放写锁", __FUNCTION__);
    UnLock(pEntity->nWriteLock);
  }

 private:
  bool TryLock(uint64_t& ucLock, uint64_t ucOldValue, uint64_t ucNewValue) {
    return __sync_bool_compare_and_swap(&ucLock, ucOldValue,
                                        ucNewValue);  // 在相等并写入的情况下返回true.
  }

  void UnLock(uint64_t& ucLock) { __sync_lock_release(&ucLock); }

  void Wait(uint64_t& lock, uint64_t value) {
    while (!__sync_bool_compare_and_swap(&lock, value, value)) {
    }
  }

  int GetWriteIndex(int nReadIndex) {
    if (nReadIndex == 0) {
      return 1;
    } else {
      return 0;
    }
  }

  char* GetShm(int iKey, uint64_t nLen, bool& bNewShm) {
    int iShmID = -1;
    void* pvShm = NULL;
    int iFlag = 0666;

    // 最多重试3次
    for (int i = 0; i < 3; i++) {
      // 先获取已存在的共享内存
      iShmID = shmget(iKey, nLen, (iFlag));
      if (iShmID != -1)  // 成功
      {
        bNewShm = false;
        InfoLog("%s|获取到已存在的共享内存成功,iShmID=%d", __FUNCTION__, iShmID);
        break;
      }

      if (EINVAL == errno) {
        InfoLog("%s|共享内存扩容,尝试删除原共享内存后继续创建", __FUNCTION__);
        SendWarning("共享内存：共享内存扩容，尝试删除后重新创建");

        // 可能是数据结构扩字段导致申请共享内存大于原共享内存导致报参数错误,尝试删除原共享内存
        int iDelShmId = shmget(iKey, 0, iFlag);
        if (iDelShmId != -1) {
          int iDelRet =
              shmctl(iDelShmId, IPC_RMID, 0);  // 当存在竞争时,可能A删除成功,B删除失败,没关系,继续
          InfoLog("%s|尝试删除原共享内存result=%d", __FUNCTION__, iDelRet);
        }
      } else if (ENOENT != errno) {
        ErrorLog("%s|获取共享内存失败,错误:%s", __FUNCTION__, strerror(errno));
        break;
      }

      // 获取失败后,创建新的共享内存
      iShmID = shmget(iKey, nLen, (iFlag | IPC_CREAT | IPC_EXCL));
      if (iShmID != -1) {
        bNewShm = true;
        InfoLog("%s|创建新的共享内存成功,iShmID=%d", __FUNCTION__, iShmID);
        break;
      }

      // 如果共享内存已存在,重试,其他错误,退出.
      if (EEXIST == errno) {
        InfoLog("%s|创建新的共享内存失败,共享内存已存在,尝试重新获取共享内存", __FUNCTION__);
      } else {
        ErrorLog("%s|创建新的共享内存失败,错误:%s", __FUNCTION__, strerror(errno));
        break;
      }
    }

    if (iShmID == -1) {
      ErrorLog("%s|获取或者创建共享内存失败", __FUNCTION__);
      return NULL;
    }

    //绑定到共享内存
    int iAccessFlag = 0;

    if ((pvShm = shmat(iShmID, NULL, iAccessFlag)) == (void*)-1) {
      ErrorLog("%s|shmat失败", __FUNCTION__);
      return NULL;
    }

    return reinterpret_cast<char*>(pvShm);
  }

 private:
  ShmHeader* m_pShmHeader;
};

#endif  // LOCAL_CACHE_SHM_H_
