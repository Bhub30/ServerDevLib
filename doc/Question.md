## Question

1. why client close connection while server still receive EPOLLIN event fromt epoll_wait

When a client closes a connection, the server might still receive an EPOLLIN event from epoll_wait because the socket is still readable. This happens because the server needs to read the EOF (end-of-file) from the socket to detect that the client has closed the connection. When the client closes the connection, the server's read call will return 0, indicating that the connection has been closed.

Here's a brief explanation of the process:

- Client Closes Connection: The client sends a FIN (finish) packet to the server to indicate that it wants to close the connection.

- Server Receives FIN: The server receives the FIN packet and the socket becomes readable, triggering an EPOLLIN event.

- Server Reads EOF: The server calls read on the socket. The read call returns 0, indicating that the client has closed the connection.

To handle this properly, you should check the return value of read in your server code. If read returns 0, you can close the socket and clean up the resources. Here's an example:

```c++
#include <sys/epoll.h>
#include <unistd.h>
#include <vector>
#include <iostream>

int main() {
    int epoll_fd = epoll_create1(0);
    std::vector<struct epoll_event> events(10);

    while (true) {
        int nfds = epoll_wait(epoll_fd, events.data(), events.size(), -1);
        for (int i = 0; i < nfds; ++i) {
            if (events[i].events & EPOLLIN) {
                char buffer[1024];
                ssize_t bytesRead = read(events[i].data.fd, buffer, sizeof(buffer));
                if (bytesRead == 0) {
                    // Client closed the connection
                    close(events[i].data.fd);
                    std::cout << "Client closed connection" << std::endl;
                } else if (bytesRead > 0) {
                    // Process the data
                } else {
                    // Handle read error
                }
            }
        }
    }

    return 0;
}
```

This code checks if read returns 0 and closes the socket if the client has closed the connection.

2. What does it mean when write() returns 0

When the write() syscall returns 0, it typically means that no data was written to the file descriptor. This can happen for a few reasons:

- Non-blocking Mode: If the file descriptor is set to non-blocking mode and the output buffer is full, write() may return 0 to indicate that no data was written. In this case, you may need to try writing again later.

- Closed Connection: If you're writing to a socket and the connection has been closed by the peer, write() may return 0. This indicates that the connection is no longer available for writing.

- File System Full: If you're writing to a file and the file system is full, write() may return 0. This indicates that there is no space left to write the data.

In general, a return value of 0 from write() is unusual and should be handled carefully in your code to ensure that you properly manage the situation.
