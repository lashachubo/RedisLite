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
  if (args.size() < 3) { send(client_fd, "-ERR wrong number of arguments\r\n\n", 33, 0); return; }
  Entry e;
  e.value = args[2];
  e.expires_at = 0;
  if (args.size() >= 5 && args[3] == "EX") {
    e.expires_at = now + (std::stoll(args[4]) * 1000);
  }
  std::move(e);
  send(client_fd, "+OK\r\n\n", 6, 0);
}

void cmd_get(int client_fd, const std::vector<std::string>& args, long long now) {
  if (args.size() < 2) { send(client_fd, "-ERR wrong number of arguments\r\n\n", 33, 0); return; }
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

void cmd_del(int client_fd, const std::vector<std::string>& args, long long now) {
  if (args.size() < 2) { send(client_fd, "-ERR wrong number of arguments\r\n\n", 33, 0); return; }
  size_t deleted_count = g_database.erase(args[1]);
  std::string response = ":" + std::to_string(deleted_count) + "\r\n\n";
  send(client_fd, response.c_str(), response.length(), 0);
}

void cmd_lpush(int client_fd, const std::vector<std::string>& args, long long now) {
  if (args.size() < 3) { send(client_fd, "-ERR wrong number of arguments\r\n\n", 33, 0); return; }
  auto& entry = g_database[args[1]];
  entry.is_list = true;
  for (size_t i = 2; i < args.size(); i++) {
    entry.list.push_front(args[i]);
  }
  std::string response = ":" + std::to_string(entry.list.size()) + "\r\n\n";
  send(client_fd, response.c_str(), response.length(), 0);
}

void cmd_rpush(int client_fd, const std::vector<std::string>& args, long long now) {
  if (args.size() < 3) { send(client_fd, "-ERR wrong number of arguments\r\n", 32, 0); return; }
  auto& entry = g_database[args[1]];
  entry.is_list = true;
  for (size_t i = 2; i < args.size(); i++) {
    entry.list.push_back(args[i]);
  }
  std::string response = ":" + std::to_string(entry.list.size()) + "\r\n\n";
  send(client_fd, response.c_str(), response.length(), 0);
}

void cmd_lrange(int client_fd, const std::vector<std::string>& args, long long now) {
  if (args.size() < 4) { send(client_fd, "-ERR wrong number of arguments\r\n\n", 33, 0); return; }
  auto it = g_database.find(args[1]);
  if (it != g_database.end()) {
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

void cmd_ltrim(int client_fd, const std::vector<std::string>& args, long long now) {
  if (args.size() < 4) { send(client_fd, "-ERR wrong number of arguments\r\n\n", 33, 0); return; }
  auto it = g_database.find(args[1]);
  if (it == g_database.end()) { send(client_fd, "-ERR no such key\r\n\n", 19, 0); return; }
  Entry& e = it->second;
  if (!e.is_list) { send(client_fd, "-WRONGTYPE that key does not hold a list\r\n\n", 43, 0); return; }
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

void cmd_rename(int client_fd, const std::vector<std::string>& args, long long now) {
  if (args.size() < 3) { send(client_fd, "-ERR wrong number of arguments\r\n\n", 33, 0); return; }
  auto it = g_database.find(args[1]);
  if (it == g_database.end()) { send(client_fd, "-ERR no such key\r\n\n", 19, 0); return; }
  g_database[args[2]] = std::move(it->second);
  g_database.erase(it);
  send(client_fd, "+OK\r\n\n", 6, 0);
}

void cmd_info(int client_fd, const std::vector<std::string>& args, long long now) {
  std::string info = "total_keys: " + std::to_string(g_database.size()) + "\n";
  info += "connected_clients: " + std::to_string(client_buffers.size()) + "\n";
  std::string res = "$" + std::to_string(info.length()) + "\r\n" + info + "\r\n";
  send(client_fd, res.c_str(), res.length(), 0);
}

void cmd_ping(int client_fd, const std::vector<std::string>& args, long long now) {
  send(client_fd, "+PONG\r\n", 7, 0);
}

void cmd_flushall(int client_fd, const std::vector<std::string>& args, long long now) {
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
  };

  auto it = commands.find(args[0]);
  if (it != commands.end()) {
    it->second(client_fd, args, now);
  } else {
    send(client_fd, "-ERR unknown command\r\n\n", 23, 0);
  }
}
