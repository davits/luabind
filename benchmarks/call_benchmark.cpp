#include <benchmark/benchmark.h>
#include <luabind/bind.hpp>

#include <iostream>

int errorHandler(lua_State* L) {
    luaL_traceback(L, L, lua_tostring(L, -1), 0);
    std::cerr << "Error while running script: " << lua_tostring(L, -1) << std::endl;
    std::abort();
    return 1;
}

struct Test : luabind::Object {
public:
    static int luaFunction(lua_State* L) {
        auto t = luabind::value_mirror<Test*>::from_lua(L, -1);
        t->x = 0;
        return 0;
    }
    static void classFunction(Test* t) {
        t->x = 0;
    }

    void memberFunction() {
        x = 0;
    }

    int x;
};

int globalLuaFunction(lua_State*) {
    return 0;
}

class BenchmarkBase : public benchmark::Fixture {
protected:
    void SetUp(benchmark::State&) override {
        L = luaL_newstate();
        luaL_openlibs(L);

        lua_pushcfunction(L, errorHandler);
        errorHandlerIdx = lua_gettop(L);

        int x = 0;
        luabind::class_<Test>(L, "Test")
            .class_function("luaFunction", &Test::luaFunction)
            .class_function("classFunction", &Test::classFunction)
            .function("memberFunction", &Test::memberFunction)
            .function("statelessLambda", [](Test* t) { t->x = 0; })
            .function("lambda", [x](Test* t) {
                (void)x;
                t->x = 0;
            });

        luabind::function(L, "globLuaFunction", &globalLuaFunction);
    }

    void TearDown(benchmark::State&) override {
        lua_close(L);
        L = nullptr;
        errorHandlerIdx = 0;
    }

protected:
    lua_State* L = nullptr;
    int errorHandlerIdx = 0;
};

BENCHMARK_F(BenchmarkBase, GlobalEmptyLuaFunction)(benchmark::State& state) {
    int r = luaL_loadstring(L, "for i = 1,1000 do globLuaFunction() end");

    if (r != LUA_OK) {
        std::cout << "Error while loading script: " << lua_tostring(L, -1) << std::endl;
        return;
    }
    for (auto _ : state) {
        lua_pushvalue(L, -1);
        lua_pcall(L, 0, 0, errorHandlerIdx);
    }
    lua_pop(L, 1);
}

BENCHMARK_F(BenchmarkBase, LuaFunction)(benchmark::State& state) {
    int r = luaL_loadstring(L, "t = Test:new(); for i = 1,1000 do Test:luaFunction(t) end");
    if (r != LUA_OK) {
        std::cout << "Error while loading script: " << lua_tostring(L, -1) << std::endl;
        return;
    }
    for (auto _ : state) {
        lua_pushvalue(L, -1);
        lua_pcall(L, 0, 0, errorHandlerIdx);
    }
    lua_pop(L, 1);
}

BENCHMARK_F(BenchmarkBase, ClassFunction)(benchmark::State& state) {
    int r = luaL_loadstring(L, "t = Test:new(); for i = 1,1000 do Test:classFunction(t) end");
    if (r != LUA_OK) {
        std::cout << "Error while loading script: " << lua_tostring(L, -1) << std::endl;
        return;
    }
    for (auto _ : state) {
        lua_pushvalue(L, -1);
        lua_pcall(L, 0, 0, errorHandlerIdx);
    }
    lua_pop(L, 1);
}

BENCHMARK_F(BenchmarkBase, MemberFunction)(benchmark::State& state) {
    int r = luaL_loadstring(L, "t = Test:new(); for i = 1,1000 do t:memberFunction() end");
    if (r != LUA_OK) {
        std::cout << "Error while loading script: " << lua_tostring(L, -1) << std::endl;
        return;
    }
    for (auto _ : state) {
        lua_pushvalue(L, -1);
        lua_pcall(L, 0, 0, errorHandlerIdx);
    }
    lua_pop(L, 1);
}

BENCHMARK_F(BenchmarkBase, StatelessLambdaAsMember)(benchmark::State& state) {
    int r = luaL_loadstring(L, "t = Test:new(); for i = 1,1000 do t:statelessLambda() end");
    if (r != LUA_OK) {
        std::cout << "Error while loading script: " << lua_tostring(L, -1) << std::endl;
        return;
    }
    for (auto _ : state) {
        lua_pushvalue(L, -1);
        lua_pcall(L, 0, 0, errorHandlerIdx);
    }
    lua_pop(L, 1);
}

BENCHMARK_F(BenchmarkBase, LambdaAsMember)(benchmark::State& state) {
    int r = luaL_loadstring(L, "t = Test:new(); for i = 1,1000 do t:lambda() end");
    if (r != LUA_OK) {
        std::cout << "Error while loading script: " << lua_tostring(L, -1) << std::endl;
        return;
    }
    for (auto _ : state) {
        lua_pushvalue(L, -1);
        lua_pcall(L, 0, 0, errorHandlerIdx);
    }
    lua_pop(L, 1);
}

#if 1

BENCHMARK_MAIN();

#else
int main() {
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);

    // lua_pushcfunction(L, errorHandler);
    //  int errorHandlerIdx = lua_gettop(L);

    int x = 0;
    luabind::class_<Test>(L, "Test")
        .class_function("luaFunction", &Test::luaFunction)
        .class_function("classFunction", &Test::classFunction)
        .function("memberFunction", &Test::memberFunction)
        .function("statelessLambda", [](Test* t) { t->x = 0; })
        .function("lambda", [x](Test* t) {
            (void)x;
            t->x = 0;
        });

    luabind::function(L, "globLuaFunction", &globalLuaFunction);

    luaL_dostring(L, "t = Test:new(); for i = 1,1000000 do t:memberFunction() end");
}
#endif
