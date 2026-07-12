# Client executable
add_executable(exchange_client src/main.cpp)
target_link_libraries(exchange_client PRIVATE exchange_net exchange_core pthread)
