#pragma once
#include <string>
#include <deque>
#include <unordered_map>
#include <unordered_set>

struct Entry {
  std::string value; // GET/SET
  std::deque<std::string> list; // LPUSH/LRANGE
  std::unordered_map<std::string, std::string> hash; // HSET/HGET
  std::unordered_set<std::string> set;
  bool is_list = false;
  long long expires_at {0};
};