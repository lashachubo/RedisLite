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

using Handler = std::function<void(int, const std::vector<std::string>&)>;

static long long now_ms() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();
}

void cmd_set(int client_fd, const std::vector<std::string>& args){
  ARGS_CHECK(3);
  if (args.size() == 4 && args[3] == "EX") { send(client_fd, "-ERR no EX time given\r\n\n", 24, 0); return; }
  Entry e;
  e.value = args[2];
  if (args.size() >= 5 && args[3] == "EX") {
    if (std::stoll(args[4]) <= 0) { send(client_fd, "-ERR EX time must be more than 0\r\n\n", 35, 0); return; }
    long long ex = std::stoll(args[4]);
    e.expires_at = now_ms() + (ex * 1000);
  }
  g_database[args[1]] = std::move(e);
  send(client_fd, "+OK\r\n\n", 6, 0);
}

void cmd_get(int client_fd, const std::vector<std::string>& args){
  ARGS_CHECK(2);
  auto it = g_database.find(args[1]);
  if (it != g_database.end()) {
    Entry& e = it->second;
    if (e.expires_at != 0 && now_ms() > e.expires_at) {
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

void cmd_del(int client_fd, const std::vector<std::string>& args){
  ARGS_CHECK(2);
  size_t deleted_count = g_database.erase(args[1]);
  std::string response = ":" + std::to_string(deleted_count) + "\r\n\n";
  send(client_fd, response.c_str(), response.length(), 0);
}

void cmd_lpush(int client_fd, const std::vector<std::string>& args){
  ARGS_CHECK(3);
  auto& entry = g_database[args[1]];
  entry.is_list = true;
  for (size_t i = 2; i < args.size(); i++) {
    entry.list.push_front(args[i]);
  }
  std::string response = ":" + std::to_string(entry.list.size()) + "\r\n\n";
  send(client_fd, response.c_str(), response.length(), 0);
}

void cmd_rpush(int client_fd, const std::vector<std::string>& args){
  ARGS_CHECK(3);
  auto& entry = g_database[args[1]];
  entry.is_list = true;
  for (size_t i = 2; i < args.size(); i++) {
    entry.list.push_back(args[i]);
  }
  std::string response = ":" + std::to_string(entry.list.size()) + "\r\n\n";
  send(client_fd, response.c_str(), response.length(), 0);
}

void cmd_lrange(int client_fd, const std::vector<std::string>& args){
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

void cmd_ltrim(int client_fd, const std::vector<std::string>& args){
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

void cmd_rename(int client_fd, const std::vector<std::string>& args){
  ARGS_CHECK(3);
  KEY_CHECK(args[1]);
  auto it = g_database.find(args[1]);
  g_database[args[2]] = std::move(it->second);
  g_database.erase(it);
  send(client_fd, "+OK\r\n\n", 6, 0);
}

void cmd_incr(int client_fd, const std::vector<std::string>& args){
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

void cmd_incrby(int client_fd, const std::vector<std::string>& args){
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

void cmd_decrby(int client_fd, const std::vector<std::string>& args){
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

void cmd_append(int client_fd, const std::vector<std::string>& args){
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

void cmd_mset(int client_fd, const std::vector<std::string>& args){
  ARGS_CHECK(3);
  for(size_t i{1}; i < args.size(); i+=2){
    Entry e;
    e.value = args[i+1];
    g_database[args[i]] = std::move(e);
  }
  send(client_fd, "+OK\r\n\n", 6, 0);
}

void cmd_mget(int client_fd, const std::vector<std::string>& args){
  ARGS_CHECK(2);
  for(size_t i{1}; i < args.size(); i++){
    auto it = g_database.find(args[i]);
    if (it != g_database.end()) {
    Entry& e = it->second;
    if (e.expires_at != 0 && now_ms() > e.expires_at) {
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

void cmd_expire(int client_fd, const std::vector<std::string>& args){
  ARGS_CHECK(3);
  KEY_CHECK(args[1]);
  KV_CHECK(args[1]);

  g_database[args[1]].expires_at = now_ms() + (std::stoll(args[2]) * 1000);
  send(client_fd, "+OK\r\n\n", 6, 0);
}

void cmd_llen(int client_fd, const std::vector<std::string>& args){
  ARGS_CHECK(2);
  KEY_CHECK(args[1]);
  LIST_CHECK(args[1]);

  std::string response = "$" + std::to_string(g_database[args[1]].list.size()) + "\r\n\n";
  send(client_fd, response.c_str(), response.length(), 0);
}

void cmd_strlen(int client_fd, const std::vector<std::string>& args){
  ARGS_CHECK(2);
  KEY_CHECK(args[1]);
  KV_CHECK(args[1]);

  std::string response = "$" + std::to_string(g_database[args[1]].value.size()) + "\r\n\n";
  send(client_fd, response.c_str(), response.length(), 0);
}

void cmd_lindex(int client_fd, const std::vector<std::string>& args){
  ARGS_CHECK(3);
  KEY_CHECK(args[1]);
  LIST_CHECK(args[1]);
  
  auto& list = g_database[args[1]].list;
  int index = std::stoi(args[2]);
  if (index < 0) index = (int)list.size() + index;
  if (index < 0 || index >= (int)list.size()) { send(client_fd, "$-1\r\n\n", 6, 0); return; }
  std::string response = "$" + std::to_string(list[index].length()) + "\r\n" + list[index] + "\r\n\n";
  send(client_fd, response.c_str(), response.length(), 0);
}

void cmd_lpop(int client_fd, const std::vector<std::string>& args){
  ARGS_CHECK(2);
  KEY_CHECK(args[1]);
  LIST_CHECK(args[1]);

  auto& list = g_database[args[1]].list;
  std::string val = list.front();
  list.pop_front();
  std::string response = "$" + std::to_string(val.length()) + "\r\n" + val + "\r\n\n";
  send(client_fd, response.c_str(), response.length(), 0);
}

void cmd_rpop(int client_fd, const std::vector<std::string>& args){
  ARGS_CHECK(2);
  KEY_CHECK(args[1]);
  LIST_CHECK(args[1]);

  auto& list = g_database[args[1]].list;
  std::string val = list.back();
  list.pop_back();
  std::string response = "$" + std::to_string(val.length()) + "\r\n" + val + "\r\n\n";
  send(client_fd, response.c_str(), response.length(), 0);
}

// hash commands

void cmd_hset(int client_fd, const std::vector<std::string>& args){
  ARGS_CHECK(4);
  auto& h = g_database[args[1]].hash;
  int added = 0;
  for (size_t i = 2; i < args.size(); i += 2) {
    if (!h.count(args[i])) added++;
    h[args[i]] = args[i + 1];
  }
  std::string response = ":" + std::to_string(added) + "\r\n\n";
  send(client_fd, response.c_str(), response.length(), 0);
}

void cmd_hget(int client_fd, const std::vector<std::string>& args){
  ARGS_CHECK(3);
  KEY_CHECK(args[1]);
  HASH_CHECK(args[1]);

  auto it = g_database.find(args[1]);
  for (size_t i = 2; i < args.size(); i++) {
    auto fit = it->second.hash.find(args[i]);
    if (fit == it->second.hash.end()) { send(client_fd, "$-1\r\n\n", 6, 0); continue; }
    std::string response = "$" + std::to_string(fit->second.length()) + "\r\n" + fit->second + "\r\n\n";
    send(client_fd, response.c_str(), response.length(), 0);
  }
}

void cmd_hgetall(int client_fd, const std::vector<std::string>& args){
  ARGS_CHECK(2);
  KEY_CHECK(args[1]);
  HASH_CHECK(args[1]);
  
  auto& h = g_database[args[1]].hash;
  std::string response = "*" + std::to_string(h.size() * 2) + "\r\n\n";
  for (auto& [field, val] : h) {
    response += "$" + std::to_string(field.length()) + "\r\n" + field + "\r\n\n";
    response += "$" + std::to_string(val.length()) + "\r\n" + val + "\r\n\n";
  }
  send(client_fd, response.c_str(), response.length(), 0);
}

void cmd_hdel(int client_fd, const std::vector<std::string>& args){
  ARGS_CHECK(2);
  KEY_CHECK(args[1]);
  HASH_CHECK(args[1]);

  g_database.erase(args[1]);
  send(client_fd, ":1\r\n\n", 5, 0);
}

void cmd_hfdel(int client_fd, const std::vector<std::string>& args){
  ARGS_CHECK(3);
  KEY_CHECK(args[1]);
  HASH_CHECK(args[1]);

  auto& it = g_database[args[1]];
  it.hash.erase(args[2]);

  send(client_fd, ":1\r\n\n", 5, 0);
}

void cmd_hexists(int client_fd, const std::vector<std::string>& args){
  ARGS_CHECK(3);
  KEY_CHECK(args[1]);
  HASH_CHECK(args[1]);

  auto& h = g_database[args[1]].hash;
  std::string response = ":" + std::to_string(h.count(args[2])) + "\r\n\n";
  send(client_fd, response.c_str(), response.length(), 0);
}

void cmd_hlen(int client_fd, const std::vector<std::string>& args){
  ARGS_CHECK(2);
  KEY_CHECK(args[1]);
  HASH_CHECK(args[1]);

  std::string response = ":" + std::to_string(g_database[args[1]].hash.size()) + "\r\n\n";
  send(client_fd, response.c_str(), response.length(), 0);
}

void cmd_hfields(int client_fd, const std::vector<std::string>& args){
  ARGS_CHECK(2);
  KEY_CHECK(args[1]);
  HASH_CHECK(args[1]);

  auto& h = g_database[args[1]].hash;
  for(auto& [field, value] : h){
    std::string response = "$" + std::to_string(field.length()) + "\r\n" + field + "\r\n\n";
    send(client_fd, response.c_str(), response.length(), 0);
  }
}

void cmd_hvals(int client_fd, const std::vector<std::string>& args){
  ARGS_CHECK(2);
  KEY_CHECK(args[1]);
  HASH_CHECK(args[1]);

  auto& h = g_database[args[1]].hash;
  for(auto& [field, value] : h){
    std::string response = "$" + std::to_string(value.length()) + "\r\n" + value + "\r\n\n";
    send(client_fd, response.c_str(), response.length(), 0);
  }
}

// Set commands

void cmd_sadd(int client_fd, const std::vector<std::string>& args){
  ARGS_CHECK(3);

  auto& set = g_database[args[1]].set;
  for(size_t i {2}; i < args.size(); i++){
    if(set.count(args[i])){
      send(client_fd, "-ERR already in set\r\n\n", 22, 0);
      return;
    }
    set.insert(args[i]);
    std::string response = "$" + std::to_string(args[i].length()) +"\r\n:1\r\n\n"; 
    send(client_fd, response.c_str(), response.length(), 0);
  }
}

// server info 

void cmd_info(int client_fd, const std::vector<std::string>& args){
  std::string info = "total_keys: " + std::to_string(g_database.size()) + "\n";
  info += "connected_clients: " + std::to_string(client_buffers.size()) + "\n";
  std::string response = "$" + std::to_string(info.length()) + "\r\n" + info + "\r\n";
  send(client_fd, response.c_str(), response.length(), 0);
}

void cmd_dbsize(int client_fd, const std::vector<std::string>& args){
  std::string response = ":" + std::to_string(g_database.size()) + "\r\n\n";
  send(client_fd, response.c_str(), response.length(), 0);
}

void cmd_ping(int client_fd, const std::vector<std::string>& args){
  send(client_fd, "+PONG\r\n\n", 8, 0);
}

void cmd_flushall(int client_fd, const std::vector<std::string>& args){
  g_database.clear();
  send(client_fd, "+OK\r\n\n", 6, 0);
}

void cmd_echo(int client_fd, const std::vector<std::string>& args){
  ARGS_CHECK(2);

  std::string response = "$";
  for(size_t i{1}; i < args.size(); i++){
    response += args[i];
    if(i < args.size()){ response += " "; }
  }
  response += "\r\n\n";
  send(client_fd, response.c_str(), response.length(), 0);
}

void cmd_type(int client_fd, const std::vector<std::string>& args){
  ARGS_CHECK(2);
  KEY_CHECK(args[1]);
  
  auto it = g_database.find(args[1]);
  std::string response = "+";
  if (it->second.is_list) response += "list";
  else if (!it->second.hash.empty()) response += "hash";
  else response += "string";

  response += "\r\n\n";
  send(client_fd, response.c_str(), response.length(), 0);
}

void cmd_help(int client_fd, const std::vector<std::string>& args);

std::unordered_map<std::string, Handler> commands = {
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
    {"LLEN",     cmd_llen},
    {"STRLEN",   cmd_strlen},
    {"DBSIZE",   cmd_dbsize},
    {"LINDEX",   cmd_lindex},
    {"LPOP",     cmd_lpop},
    {"RPOP",     cmd_rpop},
    {"ECHO",     cmd_echo},
    {"HELP",     cmd_help},
    {"TYPE",     cmd_type},
    {"HGETALL",  cmd_hgetall},
    {"HDEL",     cmd_hdel},
    {"HFDEL",    cmd_hfdel},
    {"HEXISTS",  cmd_hexists},
    {"HLEN",     cmd_hlen},
    {"HFIELDS",  cmd_hfields},
    {"HVALS",    cmd_hvals},
    {"SADD",     cmd_sadd}
  };

void process_and_reply(int client_fd, const std::vector<std::string>& args){
  if (args.empty()) return;
  auto it = commands.find(args[0]);
  if (it != commands.end()) {
    it->second(client_fd, args);
  } else {
    send(client_fd, "-ERR unknown command\r\n\n", 23, 0);
  }
}

void cmd_help(int client_fd, const std::vector<std::string>& args){
  std::string response = "For full explanation visit:\nhttps://github.com/lashachubo/RedisLite?tab=readme-ov-file#supported-commands\n";
  for(auto& [str, _] : commands){
    response += str + "\n";
  }
  response += "\r\n";
  send(client_fd, response.c_str(), response.length(), 0);
}
