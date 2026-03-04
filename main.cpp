#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <cstring>
#include <string>
#include "commands.h"

#define MAX_EVENTS 1024
#define PORT 6379

// Parses one command from buf (RESP or inline), fills args, erases consumed bytes.
// Returns false if buf doesn't yet contain a complete command.
bool parse_resp(std::string& buf, std::vector<std::string>& args) {
  if (buf.empty()) return false;
  if (buf[0] != '*') {
    // inline fallback (telnet / plain text)
    size_t pos = buf.find('\n');
    if (pos == std::string::npos) return false;
    std::string line = buf.substr(0, pos);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    buf.erase(0, pos + 1);
    args = split_command(line);
    return !args.empty();
  }
  size_t pos = buf.find("\r\n");
  if (pos == std::string::npos) return false;
  int count;
  try { count = std::stoi(buf.substr(1, pos - 1)); } catch (...) { return false; }
  size_t offset = pos + 2;
  std::vector<std::string> result;
  for (int i = 0; i < count; i++) {
    if (offset >= buf.size() || buf[offset] != '$') return false;
    size_t end = buf.find("\r\n", offset);
    if (end == std::string::npos) return false;
    int len;
    try { len = std::stoi(buf.substr(offset + 1, end - offset - 1)); } catch (...) { return false; }
    offset = end + 2;
    if (offset + (size_t)len + 2 > buf.size()) return false;
    result.push_back(buf.substr(offset, len));
    offset += len + 2;
  }
  buf.erase(0, offset);
  args = result;
  return true;
}

void set_nonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);
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

          std::vector<std::string> args;
          while (parse_resp(client_buffers[client_fd], args)) {
            process_and_reply(client_fd, args);
            args.clear();
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