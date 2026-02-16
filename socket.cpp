#include "iostream"
#include "cstring"
#include "sys/socket.h" // socket(), bind(), listen(), accept(), send(), setsockopt()
#include "netinet/in.h" // sockaddr_in, AF_INET, INADDR_ANY, htons()
#include "unistd.h" //read(), close()

int main(){
  // 1.create socket
  // AF_INET = IPv4, SOCK_STREAM = TCP
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);

  // 2. Define the address 
  sockaddr_in address;
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY; // listens to all networks
  address.sin_port = htons (6379);

  // 3. bind the socket - assigns the adress to the socket
  if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
      perror("bind failed");
      return -1;
  }
  
  // 4. Listen
  listen(server_fd, 3);
  std::cout << "Server is waiting\n";

  // 5. accept
  int addrlen = sizeof(address);
  int client_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
  std::cout << "client connected\n";

  // 6. read data with 5 second timeout
  struct timeval timeout;
  timeout.tv_sec = 300;
  timeout.tv_usec = 0;
  setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

  while(true){
    char buffer[1024] = {0};
    ssize_t bytes_recieved = read(client_socket, buffer, 1024);

    if(bytes_recieved > 0){
      std::cout << "\nClient said: " << buffer;

      const char* reply = "hello\r\n";
      send(client_socket, reply, strlen(reply), 0);
    } else {
      const char* reply = "timeout\r\n";
      send(client_socket, reply, strlen(reply), 0);
    }
  }

  // 7. end connection
  close(client_socket);
  close(server_fd);

  return 0;
}