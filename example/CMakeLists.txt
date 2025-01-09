include_directories("${CMAKE_SOURCE_DIR}/include")

add_executable(
  client
  "client.cpp"
)
add_executable(
  TcpServerExample
  "TcpServerExample.cpp"
)
add_executable(
  ReactorExample
  "ReactorExample.cpp"
)
add_executable(
  ThreadPoolExample
  "ThreadPoolExample.cpp"
)
add_executable(
  LogExample
  "LoggerExample.cpp"
)

link_directories(${CMAKE_BINARY_DIR})
target_link_libraries(TcpServerExample  PRIVATE server)
target_link_libraries(ReactorExample PRIVATE server)
target_link_libraries(ThreadPoolExample PRIVATE server threadpool)
target_link_libraries(LogExample PRIVATE logging)
