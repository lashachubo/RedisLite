#include "commands.h"
#include <sys/socket.h>
#include <sstream>
#include <chrono>
#include <functional>

std::unordered_map<std::string, Entry> g_database;
std::unordered_map<int, std::string> client_buffers;

std::vector<std::string> split_command(std::string cmd) {
  std::stringstream ss(cmd);
  std::string word;
  std::vector<std::string> parts;
  while (ss >> word) {
    parts.push_back(word);
  }
  return parts;
}

using Handler = std::function<void(int, const std::vector<std::string>&, long long)>;

void cmd_set(int client_fd, const std::vector<std::string>& args, long long now) {
  ARGS_CHECK(3);
  if (args.size() == 4 && args[3] == "EX") { send(client_fd, "-ERR no EX time given\r\n\n", 24, 0); return; }
  Entry e;
  e.value = args[2];
  if (args.size() >= 5 && args[3] == "EX") {
    if (std::stoll(args[4]) <= 0) { send(client_fd, "-ERR EX time must be more than 0\r\n\n", 35, 0); return; }
    long long ex = std::stoll(args[4]);
    e.expires_at = now + (ex * 1000);
  }
  g_database[args[1]] = std::move(e);
  send(client_fd, "+OK\r\n\n", 6, 0);
}

void cmd_get(int client_fd, const std::vector<std::string>& args, long long now) {
  ARGS_CHECK(2);
  auto it = g_database.find(args[1]);
  if (it != g_database.end()) {
    Entry& e = it->second;
    if (e.expires_at != 0 && now > e.expires_at) {
      g_database.erase(it);
      send(client_fd, "$-1\r\n\n", 6, 0);
    } else {
      std::string response = "$" + std::to_string(e.value.length()) + "\r\n" + e.value + "\r\n\n";
      send(client_fd, response.c_str(), response.length(), 0);
    }
  } else {
    send(client_fd, "$-1\r\n\n", 6, 0);
  }
}

void cmd_del(int client_fd, const std::vector<std::string>& args) {
  ARGS_CHECK(2);
  size_t deleted_count = g_database.erase(args[1]);
  std::string response = ":" + std::to_string(deleted_count) + "\r\n\n";
  send(client_fd, response.c_str(), response.length(), 0);
}

void cmd_lpush(int client_fd, const std::vector<std::string>& args) {
  ARGS_CHECK(3);
  auto& entry = g_database[args[1]];
  entry.is_list = true;
  for (size_t i = 2; i < args.size(); i++) {
    entry.list.push_front(args[i]);
  }
  std::string response = ":" + std::to_string(entry.list.size()) + "\r\n\n";
  send(client_fd, response.c_str(), response.length(), 0);
}

void cmd_rpush(int client_fd, const std::vector<std::string>& args) {
  ARGS_CHECK(3);
  auto& entry = g_database[args[1]];
  entry.is_list = true;
  for (size_t i = 2; i < args.size(); i++) {
    entry.list.push_back(args[i]);
  }
  std::string response = ":" + std::to_string(entry.list.size()) + "\r\n\n";
  send(client_fd, response.c_str(), response.length(), 0);
}

void cmd_lrange(int client_fd, const std::vector<std::string>& args) {
  ARGS_CHECK(4);
  auto it = g_database.find(args[1]);
  if (it != g_database.end()) {
    if (!it->second.is_list) { send(client_fd, "-WRONGTYPE that key does not hold a list\r\n\n", 43, 0); return; }
    auto& l = it->second.list;
    int n = (int)l.size();
    int start = std::stoi(args[2]);
    int end = std::stoi(args[3]);
    if (start < 0) start = n + start;
    if (end < 0) end = n + end;
    if (start < 0) start = 0;
    if (end >= n) end = n - 1;
    int count = (start > end) ? 0 : end - start + 1;
    std::string response = "*" + std::to_string(count) + "\r\n";
    for (int i = start; i <= end; i++) {
      response += "$" + std::to_string(l[i].length()) + "\r\n" + l[i] + "\r\n\n";
    }
    send(client_fd, response.c_str(), response.length(), 0);
  } else {
    send(client_fd, "*0\r\n", 4, 0);
  }
}

void cmd_ltrim(int client_fd, const std::vector<std::string>& args) {
  ARGS_CHECK(4);
  KEY_CHECK(args[1]);
  LIST_CHECK(args[1]);
  auto it = g_database.find(args[1]);
  Entry& e = it->second;
  auto& l = e.list;
  int n = l.size();
  int start = std::stoi(args[2]);
  int end = std::stoi(args[3]);
  if (start < 0) start = n + start;
  if (end < 0) end = n + end;
  if (start < 0) start = 0;
  if (start >= n) {
    l.clear();
  } else {
    if (end >= n) end = n - 1;
    if (start > end) {
      l.clear();
    } else {
      std::deque<std::string> trimmed(l.begin() + start, l.begin() + end + 1);
      l = std::move(trimmed);
    }
  }
  send(client_fd, "+OK\r\n\n", 6, 0);
}

void cmd_rename(int client_fd, const std::vector<std::string>& args) {
  ARGS_CHECK(3);
  KEY_CHECK(args[1]);
  auto it = g_database.find(args[1]);
  g_database[args[2]] = std::move(it->second);
  g_database.erase(it);
  send(client_fd, "+OK\r\n\n", 6, 0);
}

void cmd_incr(int client_fd, const std::vector<std::string>& args) {
  ARGS_CHECK(2);
  KEY_CHECK(args[1]);
  auto& val = g_database[args[1]].value;
  try {
    long long n = std::stoll(val) + 1;
    val = std::to_string(n);
    std::string res = ":" + val + "\r\n\n";
    send(client_fd, res.c_str(), res.length(), 0);
  } catch (...) {
    send(client_fd, "-ERR value is not an integer\r\n\n", 31, 0);
  }
}

void cmd_incrby(int client_fd, const std::vector<std::string>& args) {
  ARGS_CHECK(3);
  KEY_CHECK(args[1]);
  auto& val = g_database[args[1]].value;
  try {
    long long n = std::stoll(val) + std::stoll(args[2]);
    val = std::to_string(n);
    std::string res = ":" + val + "\r\n\n";
    send(client_fd, res.c_str(), res.length(), 0);
  } catch (...) {
    send(client_fd, "-ERR value is not an integer\r\n\n", 31, 0);
  }
}

void cmd_decr(int client_fd, const std::vector<std::string>& args) {
  ARGS_CHECK(2);
  KEY_CHECK(args[1]);
  auto& val = g_database[args[1]].value;
  try {
    long long n = std::stoll(val) - 1;
    val = std::to_string(n);
    std::string res = ":" + val + "\r\n\n";
    send(client_fd, res.c_str(), res.length(), 0);
  } catch (...) {
    send(client_fd, "-ERR value is not an integer\r\n\n", 31, 0);
  }
}

void cmd_decrby(int client_fd, const std::vector<std::string>& args) {
  ARGS_CHECK(3);
  KEY_CHECK(args[1]);
  auto& val = g_database[args[1]].value;
  try {
    long long n = std::stoll(val) - std::stoll(args[2]);
    val = std::to_string(n);
    std::string res = ":" + val + "\r\n\n";
    send(client_fd, res.c_str(), res.length(), 0);
  } catch (...) {
    send(client_fd, "-ERR value is not an integer\r\n\n", 31, 0);
  }
}

void cmd_append(int client_fd, const std::vector<std::string>& args) {
  ARGS_CHECK(3);
  auto& val = g_database[args[1]].value;
  std::string append_val;
  for (size_t i {2}; i < args.size(); i++) {
    if (i > 2) append_val += " ";
    append_val += args[i];
  }
  if (append_val.size() >= 2 && append_val.front() == '"' && append_val.back() == '"'){
    append_val = append_val.substr(1, append_val.size() - 2);
  }
  val += append_val;
  std::string res = ":" + std::to_string(val.length()) + "\r\n\n";
  send(client_fd, res.c_str(), res.length(), 0);
}

void cmd_mset(int client_fd, const std::vector<std::string>& args) {
  ARGS_CHECK(3);
  for(size_t i{1}; i < args.size(); i+=2){
    Entry e;
    e.value = args[i+1];
    g_database[args[i]] = std::move(e);
  }
  send(client_fd, "+OK\r\n\n", 6, 0);
}

void cmd_mget(int client_fd, const std::vector<std::string>& args, long long now) {
  if(args.size() < 2){ send(client_fd, "-ERR wrong number of arguments\r\n\n", 33, 0); return; }
  for(size_t i{1}; i < args.size(); i++){
    auto it = g_database.find(args[i]);
    if (it != g_database.end()) {
    Entry& e = it->second;
    if (e.expires_at != 0 && now > e.expires_at) {
      g_database.erase(it);
      send(client_fd, "$-1\r\n\n", 6, 0);
    } else {
      std::string response = "$" + std::to_string(e.value.length()) + "\r\n" + e.value + "\r\n\n";
      send(client_fd, response.c_str(), response.length(), 0);
    }
  } else {
    send(client_fd, "$-1\r\n\n", 6, 0);
  }
  }
}

void cmd_exists(int client_fd, const std::vector<std::string>& args){
  ARGS_CHECK(2);
  for(size_t i{1}; i < args.size(); i++){
    if(g_database.count(args[i])){ send(client_fd, "$1\r\n\n", 5, 0); continue; }
    send(client_fd, "$-1\r\n\n", 6, 0);
  }
}

void cmd_persist(int client_fd, const std::vector<std::string>& args){
  ARGS_CHECK(2);
  KEY_CHECK(args[1]);
  auto& entery = g_database[args[1]];
  entery.expires_at = {0};
  send(client_fd, "+OK\r\n\n", 6, 0);
}

void cmd_expire(int client_fd, const std::vector<std::string>& args, long long now){
  ARGS_CHECK(3);
  KEY_CHECK(args[1]);
  if(g_database[args[1]].is_list) { send(client_fd, "-WRONGTYPE this key holds a list\r\n\n", 35, 0); return;}
  g_database[args[1]].expires_at = now + (std::stoll(args[2]) * 1000);
  send(client_fd, "+OK\r\n\n", 6, 0);
}

void cmd_llen(int client_fd, const std::vector<std::string>& args){
  ARGS_CHECK(2);
  KEY_CHECK(args[1]);
  LIST_CHECK(args[1]);

  std::string response = "$" + std::to_string(g_database[args[1]].list.size()) + "\r\n\n";
  send(client_fd, response.c_str(), response.length(), 0);
}

// hash commands

void cmd_hset(int client_fd, const std::vector<std::string>& args) {
  ARGS_CHECK(4);
  auto& h = g_database[args[1]].hash;
  int added = 0;
  for (size_t i = 2; i < args.size(); i += 2) {
    if (!h.count(args[i])) added++;
    h[args[i]] = args[i + 1];
  }
  std::string res = ":" + std::to_string(added) + "\r\n\n";
  send(client_fd, res.c_str(), res.length(), 0);
}

void cmd_hget(int client_fd, const std::vector<std::string>& args) {
  ARGS_CHECK(3);
  auto it = g_database.find(args[1]);
  if(it == g_database.end()) { send(client_fd, "$-1\r\n\n", 6, 0); return; }
  auto fit = it->second.hash.find(args[2]);
  if (fit == it->second.hash.end()) { send(client_fd, "$-1\r\n\n", 6, 0); return; }
  std::string response = "$" + std::to_string(fit->second.length()) + "\r\n" + fit->second + "\r\n\n";
  send(client_fd, response.c_str(), response.length(), 0);
}

// server info 

void cmd_info(int client_fd, const std::vector<std::string>& args) {
  std::string info = "total_keys: " + std::to_string(g_database.size()) + "\n";
  info += "connected_clients: " + std::to_string(client_buffers.size()) + "\n";
  std::string res = "$" + std::to_string(info.length()) + "\r\n" + info + "\r\n";
  send(client_fd, res.c_str(), res.length(), 0);
}

void cmd_ping(int client_fd, const std::vector<std::string>& args) {
  send(client_fd, "+PONG\r\n", 7, 0);
}

void cmd_flushall(int client_fd, const std::vector<std::string>& args) {
  g_database.clear();
  send(client_fd, "+OK\r\n", 5, 0);
}

void process_and_reply(int client_fd, const std::vector<std::string>& args) {
  if (args.empty()) return;

  long long now = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();

  static std::unordered_map<std::string, Handler> commands = {
    {"SET",      cmd_set},
    {"GET",      cmd_get},
    {"DEL",      cmd_del},
    {"LPUSH",    cmd_lpush},
    {"RPUSH",    cmd_rpush},
    {"LRANGE",   cmd_lrange},
    {"LTRIM",    cmd_ltrim},
    {"RENAME",   cmd_rename},
    {"INFO",     cmd_info},
    {"PING",     cmd_ping},
    {"FLUSHALL", cmd_flushall},
    {"INCR",     cmd_incr},
    {"INCRBY",   cmd_incrby},    
    {"DECR",     cmd_decr},
    {"DECRBY",   cmd_decrby},
    {"APPEND",   cmd_append},
    {"HSET",     cmd_hset},
    {"HGET",     cmd_hget},
    {"MSET",     cmd_mset},
    {"MGET",     cmd_mget},
    {"EXISTS",   cmd_exists},
    {"PERSIST",  cmd_persist},
    {"EXPIRE",   cmd_expire},
    {"LLEN",     cmd_llen}
  };

  auto it = commands.find(args[0]);
  if (it != commands.end()) {
    it->second(client_fd, args, now);
  } else {
    send(client_fd, "-ERR unknown command\r\n\n", 23, 0);
  }
}
