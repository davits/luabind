#include "lua_test.hpp"

struct SomeClass : luabind::Object {
    SomeClass() = default;
    SomeClass(int x)
        : _x(x) {}

    void memberFunction(int x) {
        _x = x;
    }
    static int luaFunction(lua_State* L) {
        auto* self = luabind::value_mirror<SomeClass*>::from_lua(L, 1);
        self->_x = luabind::value_mirror<int>::from_lua(L, 2);
        return 0;
    }

    static SomeClass create(int x) {
        return SomeClass {x};
    }

    int _x = 0;
};

class BindFunctions : public LuaTest {
protected:
    void SetUp() override {
        const int top = lua_gettop(L);

        luabind::class_<SomeClass>(L, "SomeClass")
            .function("memberFunction", &SomeClass::memberFunction)
            .function("lambdaFunction", [](SomeClass& self, int x) { self._x = x; })
            .function("capturingLambdaFunction", [x = 5](SomeClass& self) { self._x = x; })
            .function("luaFunction", &SomeClass::luaFunction)
            .function("lambdaLuaFunction",
                      [](lua_State* L) -> int {
                          auto* self = luabind::value_mirror<SomeClass*>::from_lua(L, 1);
                          self->_x = luabind::value_mirror<int>::from_lua(L, 2);
                          return 0;
                      })
            .function("capturingLambdaLuaFunction",
                      [x = 7](lua_State* L) -> int {
                          auto* self = luabind::value_mirror<SomeClass*>::from_lua(L, 1);
                          self->_x = x;
                          return 0;
                      })
            .class_function("create", &SomeClass::create)
            .class_function("createWithLambda", [](int x) { return SomeClass {x}; })
            .class_function("createWithCapturingLambda", [x = 11]() { return SomeClass {x}; })
            .class_function("createWithLuaLambda",
                            [](lua_State* L) -> int {
                                auto x = luabind::value_mirror<int>::from_lua(L, 2);
                                return luabind::value_mirror<SomeClass>::to_lua(L, SomeClass {x});
                            })
            .class_function("createWithCapturingLuaLambda", [x = 13](lua_State* L) -> int {
                return luabind::value_mirror<SomeClass>::to_lua(L, SomeClass {x});
            });

        EXPECT_EQ(lua_gettop(L), top);
    }

    void TearDown() override {}
};

TEST_F(BindFunctions, MemberFunctionBinding) {
    SomeClass o;
    o = runWithResult<SomeClass>(R"--(
        local t = SomeClass:new()
        t:memberFunction(1)
        return t
    )--");
    EXPECT_EQ(o._x, 1);

    o = runWithResult<SomeClass>(R"--(
        local t = SomeClass:new()
        t:lambdaFunction(2)
        return t
    )--");
    EXPECT_EQ(o._x, 2);

    o = runWithResult<SomeClass>(R"--(
        local t = SomeClass:new()
        t:capturingLambdaFunction()
        return t
    )--");
    EXPECT_EQ(o._x, 5);

    o = runWithResult<SomeClass>(R"--(
        local t = SomeClass:new()
        t:luaFunction(3)
        return t
    )--");
    EXPECT_EQ(o._x, 3);

    o = runWithResult<SomeClass>(R"--(
        local t = SomeClass:new()
        t:lambdaLuaFunction(4)
        return t
    )--");
    EXPECT_EQ(o._x, 4);
}

TEST_F(BindFunctions, ClassFunctionBinding) {
    SomeClass o;
    o = runWithResult<SomeClass>(R"--(
        local t = SomeClass:create(1)
        return t
    )--");
    EXPECT_EQ(o._x, 1);

    o = runWithResult<SomeClass>(R"--(
        local t = SomeClass:createWithLambda(2)
        return t
    )--");
    EXPECT_EQ(o._x, 2);

    o = runWithResult<SomeClass>(R"--(
        local t = SomeClass:createWithCapturingLambda()
        return t
    )--");
    EXPECT_EQ(o._x, 11);

    o = runWithResult<SomeClass>(R"--(
        local t = SomeClass:createWithLuaLambda(3)
        return t
    )--");
    EXPECT_EQ(o._x, 3);

    o = runWithResult<SomeClass>(R"--(
        local t = SomeClass:createWithCapturingLuaLambda()
        return t
    )--");
    EXPECT_EQ(o._x, 13);
}
