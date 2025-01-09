project(server)

add_subdirectory("logging/")
add_subdirectory("reactor/")
add_subdirectory("threadpool/")

add_library(${PROJECT_NAME} INTERFACE)
# target_include_directories(${PROJECT_NAME}
#   INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
#   $<INSTALL_INTERFACE:include>
# )
