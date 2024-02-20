#pragma once

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <luabind/bind.hpp>

#include <source_location>

class LuaTest : public testing::Test {
protected:
    LuaTest() {
        L = luaL_newstate();
        luaL_openlibs(L);
        lua_pushcfunction(L, errorHandler);
        _errorHandlerIndex = lua_gettop(L);
    }

    ~LuaTest() override {
        if (L != nullptr) {
            lua_close(L);
        }
    }

public:
    int run(const char* script) {
        int r = (luaL_loadstring(L, script) || lua_pcall(L, 0, LUA_MULTRET, _errorHandlerIndex));
        if (r != LUA_OK) {
            const char* error = lua_tostring(L, -1);
            std::cerr << "Error: " << error << std::endl;
        }
        return r;
    }

    template <typename T>
    T runWithResult(const char* script) {
        const int top = lua_gettop(L);
        int r = (luaL_loadstring(L, script) || lua_pcall(L, 0, LUA_MULTRET, _errorHandlerIndex));
        if (r != LUA_OK) {
            const char* error = lua_tostring(L, -1);
            std::cerr << "Error: " << error << std::endl;
            EXPECT_EQ(r, LUA_OK);
            static T t {};
            return t;
        }
        EXPECT_EQ(lua_gettop(L), top + 1);
        T t = luabind::value_mirror<T>::from_lua(L, -1);
        lua_pop(L, -1);
        return t;
    }

    void runExpectingErrorImpl(const std::string_view script,
                               const testing::Matcher<const std::string_view&>& matcher,
                               const std::string_view file,
                               int line) {
        int lr = luaL_loadbufferx(L, script.data(), script.size(), "luabind::test", "t");
        if (lr != LUA_OK) {
            const auto errorMessage = luabind::value_mirror<std::string_view>::from_lua(L, -1);
            std::cerr << "Error at: " << file << ':' << line << '\n' << errorMessage << std::endl;
            EXPECT_THAT(errorMessage, matcher);
        }
        int should_not_be_zero = lua_pcall(L, 0, 0, _errorHandlerIndex);
        EXPECT_NE(should_not_be_zero, LUA_OK);
        if (should_not_be_zero != LUA_OK) {
            const auto errorMessage = luabind::value_mirror<std::string_view>::from_lua(L, -1);
            std::cerr << "Error at: " << file << ':' << line << '\n' << errorMessage << std::endl;
            EXPECT_THAT(errorMessage, matcher);
        }
    }

private:
    static int errorHandler(lua_State* L) {
        luaL_traceback(L, L, "", 1);
        auto message = luabind::value_mirror<std::string>::from_lua(L, -2);
        auto traceback = luabind::value_mirror<std::string_view>::from_lua(L, -1);
        message += traceback;
        lua_pop(L, 2);
        luabind::value_mirror<std::string>::to_lua(L, message);
        return 1;
    }

protected:
    lua_State* L = nullptr;
    int _errorHandlerIndex = 0;
};

#define runExpectingError(script, matcher) runExpectingErrorImpl(script, matcher, __FILE__, __LINE__)
