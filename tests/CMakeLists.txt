include(GoogleTest)

add_executable(base_class_binding base_class_binding.cpp lua_test.hpp)
target_link_libraries(base_class_binding luabind gtest_main gmock)
gtest_discover_tests(base_class_binding DISCOVERY_MODE PRE_TEST EXTRA_ARGS "--gtest_color=always")

add_executable(binding binding.cpp lua_test.hpp)
target_link_libraries(binding luabind gtest_main gmock)
gtest_discover_tests(binding DISCOVERY_MODE PRE_TEST EXTRA_ARGS "--gtest_color=always")

add_executable(binding_function_types binding_function_types.cpp lua_test.hpp)
target_link_libraries(binding_function_types luabind gtest_main gmock)
gtest_discover_tests(binding_function_types DISCOVERY_MODE PRE_TEST EXTRA_ARGS "--gtest_color=always")

add_executable(const_binding const_binding.cpp lua_test.hpp)
target_link_libraries(const_binding luabind gtest_main gmock)
gtest_discover_tests(const_binding DISCOVERY_MODE PRE_TEST EXTRA_ARGS "--gtest_color=always")

add_executable(explicit_delete explicit_delete.cpp lua_test.hpp)
target_link_libraries(explicit_delete luabind gtest_main gmock)
gtest_discover_tests(explicit_delete DISCOVERY_MODE PRE_TEST EXTRA_ARGS "--gtest_color=always")

add_executable(custom_functions custom_functions.cpp lua_test.hpp)
target_link_libraries(custom_functions luabind gtest_main gmock)
gtest_discover_tests(custom_functions DISCOVERY_MODE PRE_TEST EXTRA_ARGS "--gtest_color=always")

add_executable(errors errors.cpp lua_test.hpp)
target_link_libraries(errors luabind gtest_main gmock)
gtest_discover_tests(errors DISCOVERY_MODE PRE_TEST EXTRA_ARGS "--gtest_color=always")


