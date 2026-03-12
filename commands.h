#pragma once
#include "string"
#include "vector"
#include "unordered_map"
#include "entry.h"
#include <sys/socket.h>

extern std::unordered_map<std::string, Entry> g_database;
extern std::unordered_map<int, std::string> client_buffers;

#define ARGS_CHECK(n) \
    if ((int)args.size() < (n)) { send(client_fd, "-ERR wrong number of arguments\r\n\n", 33, 0); return; }

#define KEY_CHECK(key) \
    if (!g_database.count(key)) { send(client_fd, "-ERR no such key\r\n\n", 19, 0); return; }

#define LIST_CHECK(key) \
    if (!g_database[key].is_list) { send(client_fd, "-WRONGTYPE that key does not hold a list\r\n\n", 43, 0); return; }

std::vector<std::string> split_command(std::string cmd);
void process_and_reply(int client_fd, const std::vector<std::string>& args);
