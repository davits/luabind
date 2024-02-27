[![Unit tests](https://github.com/davits/luabind/actions/workflows/unit_tests.yml/badge.svg?branch=master&event=push)](https://github.com/davits/luabind/actions/workflows/unit_tests.yml)
[![codecov](https://codecov.io/gh/davits/luabind/graph/badge.svg?token=WOZ5J1HU3F)](https://codecov.io/gh/davits/luabind)

Luabind is a small header only library aiming to provide easy means to expose C++ types in Lua.

It uses variadic template magic to generate code for you, which handles function arguments transition from Lua to C++, function call, and transition of result back to Lua.

## Example
```cpp
class Account : public luabind::Object {
public:
    Account(int balance);
public:
    void credit(int amount);
    void debit(int amount);

public: // public for the sake of the example
    int balance;
};

class SpecialAccount : public Account {
public:
    SpecialAccount(int balance, int overdraft);

    int getOverdraft() const;

private:
    int overdraft;
};
```

Binding to lua
```cpp
lua_State* L = luaL_newstate();
luaL_openlibs(L);
luabind::class_<Account>(L, "Account")
    .constructor<int>("new")
    .function("credit", &Account::credit)
    .function("debit", &Account::debit)
    .property_readonly("balance", &Account::balance);

luabind::class_<SpecialAccount, Account>(L, "SpecialAccount")
    .constructor<int, int>("new")
    .construct_shared<int, int>("create")
    .property_readonly("overdraft", &SpecialAccount::getOverdraft)
    .property("availableBalance", [](SpecialAccount& self) { return self.balance + self.getOverdraft(); });

luaL_dostring(L, R"--(
    a = Account:new(10)
    a:credit(3)
    a:debit(2)
    s = SpecialAccount:new(50, 10) " Memory is allocated on lua stack.
    s:credit(20)
    s = SpecialAccount:create(50, 10) " Represented by C++ shared_ptr on lua stack.
    s:credit(20)
    print(s.balance)
    print(s.overdraft)
    print(s.availableBalance)
)--");
```
## How to install, configure, build and run

Ordinary prcedure when developing and testing `luabind` library.

```sh
git clone https://github.com/davits/luabind.git

git submodule update --recursive --init

cmake --preset luabind

cmake --build --preset luabind

ctest --preset unit_tests
```

## Integrate to CMake project

Simple example to use `luabind` as a submodule to a project which already has Lua integrated.
``` cmake
# Given that "lua" is the cmake name of Lua library in the top project
# And it has lua's path added to include path, such as:
# target_include_directories(lua SYSTEM INTERFACE ${LUA_SOURCE_DIR})
set(LUABIND_LUA_LIB_NAME lua)
add_subdirectory(luabind)
```

Here are some control variables, which will help to fine tune integration:

| CMake variable | Description |
| ------ | ------ |
| LUABIND_LUA_LIB_NAME (STRING) | cmake name of the Lua library to use (default: luabind_lua) |
| LUABIND_LUA_CPP (BOOL) | option indicating whether Lua headers should be included as C++ code. (default: OFF) |
| LUABIND_UNIT_TESTS (BOOL) | option to enable luabind tests (default: OFF) |
| LUABIND_BENCHMARKS (BOOL) | option to enable luabind benchmarks (default: OFF) |
