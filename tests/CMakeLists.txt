include(CTest)

add_executable(base_class_binding base_class_binding.cpp lua_test.hpp)
target_link_libraries(base_class_binding luabind gtest_main)
add_test(NAME base_class_binding_test COMMAND base_class_binding)

add_executable(binding binding.cpp lua_test.hpp)
target_link_libraries(binding luabind gtest_main)
add_test(NAME binding_test COMMAND binding)

add_executable(const_binding const_binding.cpp lua_test.hpp)
target_link_libraries(const_binding luabind gtest_main)
add_test(NAME const_binding_test COMMAND const_binding)

add_executable(explicit_delete explicit_delete.cpp lua_test.hpp)
target_link_libraries(explicit_delete luabind gtest_main)
add_test(NAME explicit_delete_test COMMAND explicit_delete)

add_executable(custom_functions custom_functions.cpp lua_test.hpp)
target_link_libraries(custom_functions luabind gtest_main)
add_test(NAME custom_functions_test COMMAND custom_functions)

add_executable(errors errors.cpp lua_test.hpp)
target_link_libraries(errors luabind gtest_main)
add_test(NAME errors_test COMMAND errors)


