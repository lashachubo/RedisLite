#pragma once
#include "string"
#include "deque"

struct Entry {
  std::string value;        // GET/SET
  std::deque<std::string> list; // LPUSH/LRANGE
  bool is_list = false;
  long long expires_at;
};