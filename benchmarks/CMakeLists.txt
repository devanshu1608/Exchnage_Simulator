# ─── Benchmarks ───
add_executable(bench_order_book bench_order_book.cpp)
target_link_libraries(bench_order_book PRIVATE exchange_core)

add_executable(bench_spsc bench_spsc.cpp)
target_link_libraries(bench_spsc PRIVATE exchange_core)
target_link_libraries(bench_spsc PRIVATE pthread)
