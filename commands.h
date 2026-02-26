#pragma once
#include "string"
#include "vector"
#include "unordered_map"
#include "entry.h"

extern std::unordered_map<std::string, Entry> g_database;
extern std::unordered_map<int, std::string> client_buffers;

std::vector<std::string> split_command(std::string cmd);
void process_and_reply(int client_fd, const std::vector<std::string>& args);
