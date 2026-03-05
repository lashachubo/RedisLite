#pragma once
#include <string>
#include <deque>
#include <unordered_map>

struct Entry {
  std::string value;        // GET/SET
  std::deque<std::string> list; // LPUSH/LRANGE
  std::unordered_map<std::string, std::string> hash; // HSET/HGET
  bool is_list = false;
  long long expires_at {0};
};