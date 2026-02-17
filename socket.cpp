#include <iostream>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <cstring>

#define MAX_EVENTS 10
#define PORT 6379

// make a socket non-blocking
void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
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
                    std::cout << "[Client] " << buffer << std::flush;
                    
                    const char* response = "[message recieved]\r\n";
                    send(client_fd, response, strlen(response), 0);
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