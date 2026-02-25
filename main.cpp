#include "iostream"
#include "vector"
#include "sys/socket.h"
#include "netinet/in.h"
#include "unistd.h"
#include "fcntl.h"
#include "sys/epoll.h"
#include "cstring"
#include "unordered_map"
#include "string"
#include "sstream"
#include "chrono"
#include "fstream"
#include "deque"

#define MAX_EVENTS 10
#define PORT 6379

struct Entry {
  std::string value;  //  GET/SET
  std::deque<std::string> list; // LPUSH/LRANGE
  bool is_list = false;
  long long expires_at;
};

// make a socket nonblocking
void set_nonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

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


void process_and_reply(int client_fd, const std::vector<std::string>& args) {
  if (args.empty()) return;

  long long now = std::chrono::duration_cast<std::chrono::milliseconds>(
  std::chrono::system_clock::now().time_since_epoch()).count();

  std::string command = args[0];

  if (command == "SET" && args.size() >= 3) {
    Entry e;
    e.value = args[2];
    e.expires_at = 0;

    if (args.size() >= 5 && args[3] == "EX") {
      e.expires_at = now + (std::stoll(args[4]) * 1000);
    }

    g_database[args[1]] = e;
    send(client_fd, "+OK\r\n\n", 6, 0);
    
  } else if (command == "GET" && args.size() >= 2) {
    if (g_database.count(args[1])) {
      Entry& e = g_database[args[1]]; 
      
      if (e.expires_at != 0 && now > e.expires_at) {
        g_database.erase(args[1]);
        send(client_fd, "$-1\r\n\n", 5, 0);
      } else {
        std::string response = "$" + std::to_string(e.value.length()) + "\r\n" + e.value + "\r\n\n";
        send(client_fd, response.c_str(), response.length(), 0);
      }
    } else {
      send(client_fd, "$-1\r\n\n", 5, 0);
    }

  } else if (command == "DEL" && args.size() >= 2) {
    size_t deleted_count = g_database.erase(args[1]);
    std::string response = ":" + std::to_string(deleted_count) + "\r\n\n";
    send(client_fd, response.c_str(), response.length(), 0);

  } else if (command == "LPUSH" && args.size() >= 3) {
    g_database[args[1]].list.insert(g_database[args[1]].list.begin(), args[2]);
    g_database[args[1]].is_list = true;
    
    std::string response = ":" + std::to_string(g_database[args[1]].list.size()) + "\r\n\n";
    send(client_fd, response.c_str(), response.length(), 0);

  } else if (command == "RPUSH" && args.size() >= 3) {
    g_database[args[1]].is_list = true;
    for (size_t i = 2; i < args.size(); i++) {
      g_database[args[1]].list.push_back(args[i]);
    }
    std::string res = ":" + std::to_string(g_database[args[1]].list.size()) + "\r\n\n";
    send(client_fd, res.c_str(), res.length(), 0);

  } else if (command == "LRANGE" && args.size() >= 4) {
    if (g_database.count(args[1])) {
      auto& l = g_database[args[1]].list;
      int start = std::stoi(args[2]);
      int end = std::stoi(args[3]);
      
      // bounds checking
      if (start < 0) start = 0;
      if (end >= l.size()) end = l.size() - 1;

      std::string response = "*" + std::to_string(end - start + 1) + "\r\n";
      for (int i = start; i <= end; i++) {
        response += "$" + std::to_string(l[i].length()) + "\r\n" + l[i] + "\r\n\n";
      }
      send(client_fd, response.c_str(), response.length(), 0);
    } else {
      send(client_fd, "*0\r\n", 4, 0);
    }
    
  } else if (command == "LTRIM" && args.size() >= 4) {
    if (g_database.count(args[1])) {
      Entry& e = g_database[args[1]];

      if (!e.is_list) {
        std::string err = "-WRONGTYPE that key does not hold a list\r\n\n";
        send(client_fd, err.c_str(), err.length(), 0);
      }

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
          l = trimmed;
        }
      }
      send(client_fd, "+OK\r\n\n", 5, 0);
    } else {
      send(client_fd, "-ERR no such key\r\n\n", 5, 0);
    }

  } else if (command == "RENAME" && args.size() >= 3) {
    if (g_database.count(args[1])) {
      g_database[args[2]] = g_database[args[1]];
      g_database.erase(args[1]);
      send(client_fd, "+OK\r\n\n", 5, 0);
    } else {
      send(client_fd, "-ERR no such key\r\n\n", 18, 0);
    }

  } else if (command == "INFO") {
    std::string info = "total_keys: " + std::to_string(g_database.size()) + "\n";
    info += "connected_clients: " + std::to_string(client_buffers.size()) + "\n";
    
    std::string res = "$" + std::to_string(info.length()) + "\r\n" + info + "\r\n";
    send(client_fd, res.c_str(), res.length(), 0);

  } else if (command == "PING") {
    send(client_fd, "+PONG\r\n", 7, 0);

  } else if (command == "FLUSHALL") {
    g_database.clear();
    send(client_fd, "+OK\r\n", 5, 0);
  
  } else {
    send(client_fd, "-ERR unknown command\r\n", 22, 0);
  }
}

int main() {

  // create socket
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd == -1) {
    perror("Socket failed");
    return 1;
  }

  // prevents "Address already in use" errors
  int opt = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  // bind
  sockaddr_in address;
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(PORT);

  if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
    perror("Bind failed");
    return 1;
  }

  listen(server_fd, SOMAXCONN);
  set_nonblocking(server_fd);

  std::cout << "Redis Clone listening on port " << PORT << "..." << std::endl;

  int epoll_fd = epoll_create1(0);
  struct epoll_event ev, events[MAX_EVENTS];

  // allow multiple connections
  ev.events = EPOLLIN; 
  ev.data.fd = server_fd;
  epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev);

  while (true) {
    int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);

    for (int n = 0; n < nfds; ++n) {
      if (events[n].data.fd == server_fd) {
        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);
        int client_socket = accept(server_fd, (struct sockaddr*)&client_addr, &addrlen);
              
        set_nonblocking(client_socket);
              
        // add new client to watchlist
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = client_socket;
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_socket, &ev);
              
        std::cout << "[Server] New connection accepted" << std::endl;

      } else {
        int client_fd = events[n].data.fd;
        char buffer[1024] = {0};
        ssize_t bytes_received = read(client_fd, buffer, sizeof(buffer));

        if (bytes_received > 0) {
          // add bytes to clients buffer
          client_buffers[client_fd].append(buffer, bytes_received);

          // check if full line
          size_t pos;
          while ((pos = client_buffers[client_fd].find('\n')) != std::string::npos) {
            std::string full_command = client_buffers[client_fd].substr(0, pos);
            if (!full_command.empty() && full_command.back() == '\r') {
              full_command.pop_back();
            }

            std::vector<std::string> args = split_command(full_command);
            process_and_reply(client_fd, args);
            client_buffers[client_fd].erase(0, pos + 1);
          }
        } 
        else if (bytes_received <= 0) {
          client_buffers.erase(client_fd);
          close(client_fd);
          std::cout << "[Server] Client disconnected" << std::endl;
        }
      }
    }
  }

  close(server_fd);
  return 0;
}