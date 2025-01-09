## About

I/O multiplexing is a technique used in computer programming to handle multiple input/output operations simultaneously. It allows a program to monitor multiple file descriptors (such as sockets, files, or pipes) to see if any of them are ready for reading, writing, or other I/O operations. This is particularly useful in network programming, where a server might need to handle multiple client connections at the same time.

There are several methods for I/O multiplexing, including:

*select()*: This system call allows a program to monitor multiple file descriptors to see if any of them are ready for I/O operations.

*poll()*: Similar to select(), but can handle a larger number of file descriptors.

*epoll()*: An efficient method available on Linux that scales better with a large number of file descriptors.

I/O multiplexing helps improve the efficiency and responsiveness of programs, especially in scenarios where multiple I/O operations need to be managed concurrently.

These methods are essential for building high-performance network servers and applications that need to handle multiple I/O operations concurrently.

### select

Usage: select() is used to monitor multiple file descriptors to see if any of them are ready for I/O operations (read, write, or error).

How it works: You pass three sets of file descriptors (read, write, and error) to select(), along with a timeout value. select() will block until at least one of the file descriptors is ready or the timeout expires.

Limitations: It has a limit on the maximum number of file descriptors it can handle (typically 1024). It also requires scanning all file descriptors, which can be inefficient for a large number of descriptors.

Example:

```c++
#include <sys/select.h>
#include <unistd.h>
#include <stdio.h>

int main() {
    fd_set readfds;
    struct timeval tv;
    int retval;

    // Watch stdin (fd 0) to see when it has input.
    FD_ZERO(&readfds);
    FD_SET(0, &readfds);

    // Wait up to five seconds.
    tv.tv_sec = 5;
    tv.tv_usec = 0;

    retval = select(1, &readfds, NULL, NULL, &tv);

    if (retval == -1)
        perror("select()");
    else if (retval)
        printf("Data is available now.\n");
    else
        printf("No data within five seconds.\n");

    return 0;
}
```

### poll

Usage: poll() is similar to select() but can handle a larger number of file descriptors.

How it works: You pass an array of pollfd structures, each representing a file descriptor and the events you are interested in (e.g., read, write). poll() will block until one or more file descriptors are ready or the timeout expires.

Advantages: It does not have the same file descriptor limit as select(). It also provides a more flexible and scalable interface.

Example:

```c++
#include <poll.h>
#include <unistd.h>
#include <stdio.h>

int main() {
    struct pollfd fds[1];
    int ret;

    // Watch stdin (fd 0) to see when it has input.
    fds[0].fd = 0;
    fds[0].events = POLLIN;

    // Wait up to five seconds.
    ret = poll(fds, 1, 5000);

    if (ret == -1)
        perror("poll()");
    else if (ret == 0)
        printf("No data within five seconds.\n");
    else
        printf("Data is available now.\n");

    return 0;
}

```

### epoll

Usage: epoll() is an efficient I/O multiplexing method available on Linux, designed to handle a large number of file descriptors.

How it works: You create an epoll instance and add file descriptors to it using epoll_ctl(). You then use epoll_wait() to wait for events on these file descriptors.

Advantages: It is highly scalable and efficient, as it uses an event-driven mechanism. It only returns the file descriptors that are ready, avoiding the need to scan all descriptors. It also supports edge-triggered and level-triggered modes, providing more control over event handling.

Example:

```c++
#include <sys/epoll.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_EVENTS 10

int main() {
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }

    struct epoll_event event;
    struct epoll_event events[MAX_EVENTS];

    event.events = EPOLLIN;
    event.data.fd = 0; // stdin

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, 0, &event) == -1) {
        perror("epoll_ctl");
        exit(EXIT_FAILURE);
    }

    int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, 5000);
    if (nfds == -1) {
        perror("epoll_wait");
        exit(EXIT_FAILURE);
    }

    for (int n = 0; n < nfds; ++n) {
        if (events[n].data.fd == 0) {
            printf("Data is available now.\n");
        }
    }

    close(epoll_fd);
    return 0;
}

```
