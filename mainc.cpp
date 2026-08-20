


int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd == -1) {
    perror("Socket failed");
    return 1;
  }

// we create a socket
// AF_INET = ipv4
// SOCK_STREAM = TCP
// then we initialize the unique ID to server_fd and if it fails we return

  int opt = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));


// we make it reusable after restart so we dont have to wait a minute or so


  sockaddr_in address;
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(PORT);

// binds the socket to an address

  if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
    perror("Bind failed");
    return 1;
  }

// just checks if bind worked

  listen(server_fd, SOMAXCONN);
  set_nonblocking(server_fd);

// then we start accepting connections


 int epoll_fd = epoll_create1(0);
  struct epoll_event ev, events[MAX_EVENTS];
  ev.events = EPOLLIN;
  ev.data.fd = server_fd;
  epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev);

// creates a watchlist that takes all the active users
  
  
// then we have a loop 
  
  int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1)
  
// this basically notifies if anything happened on any server_fd so we dont 
// have to constantly check and nfds keeps track of how many times it happened


 for (int n = 0; n < nfds; ++n) {
// n is the number of clients that sent data and it goes thru it 1 by 1
      
      if (events[n].data.fd == server_fd) {
        // check if user is already connected   

        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);
        int client_socket = accept(server_fd, (struct sockaddr*)&client_addr, &addrlen);
        // creates a new socket for that user

        set_nonblocking(client_socket);

        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = client_socket;
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_socket, &ev);
        // added to the watchlist
        
        std::cout << "[Server] New connection accepted" << std::endl;

      }else {
        int client_fd = events[n].data.fd;
        char buffer[1024] = {0};
        ssize_t bytes_received = read(client_fd, buffer, sizeof(buffer));

        if (bytes_received > 0) {
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