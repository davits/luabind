include (GoogleTest)

add_executable(call_benchmark call_benchmark.cpp)
target_link_libraries(call_benchmark luabind benchmark::benchmark)
