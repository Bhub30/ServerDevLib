## Structure

A reactive application consists of several moving parts and will rely on some support mechanisms:

### Handle

An identifier and interface to a specific request, with IO and data. This will often take the form of a socket, file descriptor, or similar mechanism, which should be provided by most modern operating systems.

### Demultiplexer

An event notifier that can efficiently monitor the status of a handle, then notify other subsystems of a relevant status change (typically an IO handle becoming "ready to read"). Traditionally this role was filled by the select() system call, but more contemporary examples include epoll, kqueue, and IOCP.

### Dispatcher
The actual event loop of the reactive application, this component maintains the registry of valid event handlers, then invokes the appropriate handler when an event is raised.

### Event Handler

Also known as a request handler, this is the specific logic for processing one type of service request. The reactor pattern suggests registering these dynamically with the dispatcher as callbacks for greater flexibility. By default, a reactor does not use multi-threading but invokes a request handler within the same thread as the dispatcher.

### Event Handler Interface
An abstract interface class, representing the general properties and methods of an event handler. Each specific handler must implement this interface while the dispatcher will operate on the event handlers through this interface.
