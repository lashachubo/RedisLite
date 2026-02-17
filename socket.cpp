#include <iostream>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <cstring>
#include <unordered_map>
#include <string>
#include <sstream>
#include <vector>

#define MAX_EVENTS 10
#define PORT 6379

// make a socket non-blocking
void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

std::unordered_map<std::string, std::string> g_database;

std::vector<std::string> split_command(std::string cmd) {
    std::stringstream ss(cmd);
    std::string word;
    std::vector<std::string> parts;
    while (ss >> word) {
        parts.push_back(word);
    }
    return parts;
}

int main() {

    // 1. create socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("Socket failed");
        return 1;
    }

    // prevents "Address already in use" errors
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 2. bind
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

    // 3. epoll
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
                // client sent data
                int client_fd = events[n].data.fd;
                char buffer[1024] = {0};
                ssize_t bytes_received = read(client_fd, buffer, sizeof(buffer));

                if (bytes_received > 0) {
                    // 1. Convert the buffer to a string
                  std::string raw_data(buffer);
                  std::vector<std::string> args = split_command(raw_data);

                  if (args.empty()) return 0;

                  std::string command = args[0]; // e.g., "SET" or "GET"

                  if (command == "SET" && args.size() >= 3) {
                      // args[1] is the Key, args[2] is the Value
                      g_database[args[1]] = args[2];
                      send(client_fd, "+OK\r\n", 5, 0);
                  } 
                  else if (command == "GET" && args.size() >= 2) {
                      // Look up the key in our map
                      if (g_database.count(args[1])) {
                          std::string val = g_database[args[1]];
                          std::string response = "$" + std::to_string(val.length()) + "\r\n" + val + "\r\n";
                          send(client_fd, response.c_str(), response.length(), 0);
                      } else {
                          send(client_fd, "$-1\r\n", 5, 0); // Redis way of saying "Not Found"
                      }
                  } 
                  else {
                      send(client_fd, "-ERR unknown command\r\n", 22, 0);
                  }
                } 
                else if (bytes_received == 0) {
                    std::cout << "[Server] Client disconnected" << std::endl;
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                    close(client_fd);
                }
            }
        }
    }

    close(server_fd);
    return 0;
}