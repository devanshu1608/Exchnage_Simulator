cmake_minimum_required(VERSION 3.20)
project(ExchangeSimulator
    VERSION 1.0.0
    LANGUAGES CXX
)

# ─── C++20 Standard ───
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add basic compiler flags directly instead of using complex modules
add_compile_options(-Wall -Wextra -O3 -pthread)

# ─── Core (header-only) ───
add_library(exchange_core INTERFACE)
target_include_directories(exchange_core INTERFACE
    ${CMAKE_SOURCE_DIR}/core/include
    ${CMAKE_SOURCE_DIR}/containers/include
)

# ─── Net library (compiled) ───
add_library(exchange_net
    net/src/tcp_socket.cpp
    net/src/tcp_server.cpp
    net/src/tcp_client.cpp
    net/src/epoll_mgr.cpp
)
target_include_directories(exchange_net PUBLIC
    ${CMAKE_SOURCE_DIR}/net/include
)
target_link_libraries(exchange_net PUBLIC exchange_core)

# ─── Matching library (compiled) ───
add_library(exchange_matching
    matching/src/order_book.cpp
    matching/src/matching_engine.cpp
)
target_include_directories(exchange_matching PUBLIC
    ${CMAKE_SOURCE_DIR}/matching/include
)
target_link_libraries(exchange_matching PUBLIC exchange_core)

# ─── Exchange server executable ───
file(GLOB_RECURSE SERVER_SOURCES "src/*.cpp")

add_executable(exchange_server
    ${SERVER_SOURCES}
)
target_include_directories(exchange_server PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(exchange_server PRIVATE
    exchange_net exchange_matching exchange_core pthread
)


# ─── Benchmarks ───
if(EXISTS "${CMAKE_SOURCE_DIR}/benchmarks/CMakeLists.txt")
    add_subdirectory(benchmarks)
endif()

# ─── Client ───
if(EXISTS "${CMAKE_SOURCE_DIR}/client/CMakeLists.txt")
    add_subdirectory(client)
endif()
