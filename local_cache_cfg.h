// Copyright [year] <Copyright Owner>
#ifndef LOCAL_CACHE_CFG_H_
#define LOCAL_CACHE_CFG_H_

#include <time.h>

#include <map>

template <class Tkey, class Tvalue>
class CacheCfg {
 public:
  CacheCfg();
  CacheCfg(time_t timeout);

 public:
  void SetTimeOut(time_t t);

  int get(const Tkey& key, Tvalue& value) const;
  int get(const Tkey& key, time_t end_time, Tvalue& value) const;

  void set(const Tkey& key, const Tvalue& value);
  void set(const Tkey& key, const Tvalue& value, time_t end_time);

 private:
  typedef struct _Tcontent {
    Tvalue value;
    time_t expire;
  } Tcontent;

  std::map<Tkey, Tcontent> content;
  time_t timeout;
};

template <class Tkey, class Tvalue>
CacheCfg<Tkey, Tvalue>::CacheCfg(time_t iTimeout) : timeout(iTimeout){};

template <class Tkey, class Tvalue>
CacheCfg<Tkey, Tvalue>::CacheCfg() : timeout(3600){};

template <class Tkey, class Tvalue>
void CacheCfg<Tkey, Tvalue>::SetTimeOut(time_t t) {
  timeout = t;
};

template <class Tkey, class Tvalue>
void CacheCfg<Tkey, Tvalue>::set(const Tkey& key, const Tvalue& value) {
  Tcontent con;
  con.value = value;
  con.expire = (time(NULL) + timeout);
  content[key] = con;
  return;
};

template <class Tkey, class Tvalue>
void CacheCfg<Tkey, Tvalue>::set(const Tkey& key, const Tvalue& value, time_t end_time) {
  Tcontent con;
  con.value = value;
  con.expire = end_time;
  content[key] = con;
  return;
};

template <class Tkey, class Tvalue>
int CacheCfg<Tkey, Tvalue>::get(const Tkey& key, Tvalue& value) const {
  typename map<Tkey, Tcontent>::const_iterator cit = content.find(key);
  if (cit != content.end()) {
    const Tcontent& con = cit->second;
    value = con.value;
    if (con.expire > time(NULL)) {
      return 0;
    } else {
      return 2;
    }
  } else {
    return 1;
  }
};

template <class Tkey, class Tvalue>
int CacheCfg<Tkey, Tvalue>::get(const Tkey& key, time_t end_time, Tvalue& value) const {
  typename map<Tkey, Tcontent>::const_iterator cit = content.find(key);
  if (cit != content.end()) {
    const Tcontent& con = cit->second;
    value = con.value;
    if (con.expire > end_time) {
      return 0;
    } else {
      return 2;
    }
  } else {
    return 1;
  }
}

#endif  // LOCAL_CACHE_CFG_H_
