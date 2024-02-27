#include "lua_test.hpp"

#include <memory>

class String : public luabind::Object {
public:
    std::string str;

public:
    String(const std::string_view s)
        : str(s) {}

    static String createString(const std::string_view s) {
        return String(s);
    }

    void set(const std::string& s) {
        str = s;
    }

    const std::string& get() {
        return str;
    }

    void copyFrom(const std::shared_ptr<String> from) {
        str = from->str;
    }

    void copy(const String& from) {
        str = from.str;
    }

    void customError(bool thr) {
        if (thr) {
            throw std::runtime_error("Custom std::error");
        } else {
            throw "unknown";
        }
    }
};

class Numeric : public luabind::Object {
public:
    void boolean(bool) {}

    void integral(int) {}

    void floating(float) {}
};

Numeric toNumeric(const String&) {
    return Numeric {};
}

class StrLuaTest : public LuaTest {
protected:
    void SetUp() override {
        const int top = lua_gettop(L);
        luabind::class_<String>(L, "String")
            .constructor<const std::string_view>("new")
            .construct_shared<const std::string_view>("create")
            .class_function("createString", &String::createString)
            .function("set", &String::set)
            .function("get", &String::get)
            .property("str", &String::str)
            .function("copy", &String::copy)
            .function("copyFrom", &String::copyFrom)
            .function("customError", &String::customError);

        luabind::class_<Numeric>(L, "Numeric")
            .construct_shared<>("create")
            .function("boolean", &Numeric::boolean)
            .function("integral", &Numeric::integral)
            .function("floating", &Numeric::floating);

        luabind::function(L, "toNumeric", &toNumeric);

        EXPECT_EQ(lua_gettop(L), top);
    }
};

TEST_F(StrLuaTest, MirrorErrors) {
    // clang-format off
    runExpectingError(R"--(
        u = Numeric:new()
        a = String:new(u)
    )--",
        testing::StartsWith("Argument at 2 has invalid type. Expecting 'string', but got 'userdata'."));

    runExpectingError(
        R"--(
        a = String:new('aaa')
        b = String:create('bbb')
        a:copyFrom(b) -- ok
        a:copyFrom('abcd') -- should result in error
    )--",
        testing::StartsWith(
            "Argument at 2 has invalid type. Expecting user_data of type 'String', but got lua type 'string'"));

    runExpectingError(R"--(
        a = String:new('aaa')
        b = String:new('bbb')
        a:copyFrom(b)
    )--",
        testing::StartsWith("Argument at 2 is not a shared_ptr."));

    runExpectingError(R"--(
        a = String:new('aaa')
        b = Numeric:create()
        a:copyFrom(b)
    )--",
        testing::StartsWith("Argument at 2 has invalid type. Expecing 'String' but got 'Numeric'."));

    runExpectingError(
        R"--(
        a = String:new('aaa')
        a:copy(555)
    )--",
        testing::StartsWith(
            "Argument at 2 has invalid type. Expecting user_data of type 'String', but got lua type 'number'"));

    runExpectingError(
        R"--(
        a = String:new('aaa')
        b = Numeric:new()
        a:copy(b)
    )--",
        testing::StartsWith("Argument at 2 has invalid type. Expecting 'String' but got 'Numeric'."));

    runExpectingError(
        R"--(
        u = Numeric:new()
        b:boolean(true)
        b:boolean(false)
        b:boolean(1)
    )--",
        testing::StartsWith("Argument at 2 has invalid type. Expecting 'boolean', but got 'number'."));

    runExpectingError(
        R"--(
        u = Numeric:new()
        b:integral(1)
        b:integral('abc')
    )--",
        testing::StartsWith("Argument at 2 has invalid type. Expecting 'integer', but got 'string'."));

    runExpectingError(
        R"--(
        u = Numeric:new()
        b:integral(1.5)
    )--",
        testing::StartsWith("Argument at 2 has invalid type. Expecting 'integer', but got 'number'."));

    runExpectingError(
        R"--(
        u = Numeric:new()
        b:floating(1.5)
        b:floating('abc')
    )--",
        testing::StartsWith("Argument at 2 has invalid type. Expecting 'number', but got 'string'."));
    // clang-format on
}

TEST_F(StrLuaTest, WrapperErrors) {
    runExpectingError(
        R"--(
        s = String:new()
    )--",
        testing::StartsWith("Invalid number of arguments, should be 1, but 0 were given."));

    runExpectingError(
        R"--(
        s = String:new('abc', 1)
    )--",
        testing::StartsWith("Invalid number of arguments, should be 1, but 2 were given."));

    runExpectingError(
        R"--(
        s = String:create()
    )--",
        testing::StartsWith("Invalid number of arguments, should be 1, but 0 were given."));

    runExpectingError(
        R"--(
        s = String:create('abc', 1)
    )--",
        testing::StartsWith("Invalid number of arguments, should be 1, but 2 were given."));

    runExpectingError(
        R"--(
        s = String:createString()
    )--",
        testing::StartsWith("Invalid number of arguments, should be 1, but 0 were given."));

    runExpectingError(
        R"--(
        s = String:createString('abc', 1)
    )--",
        testing::StartsWith("Invalid number of arguments, should be 1, but 2 were given."));

    runExpectingError(
        R"--(
        s = String:new('abc')
        s:set()
    )--",
        testing::StartsWith("Invalid number of arguments, should be 2, but 1 were given."));

    runExpectingError(
        R"--(
        s = String:createString('abc')
        s:set('def', 2, 3)
    )--",
        testing::StartsWith("Invalid number of arguments, should be 2, but 4 were given."));

    runExpectingError(
        R"--(
        s = String:create('abc')
        s:customError(true)
    )--",
        testing::StartsWith("Custom std::error"));

    runExpectingError(
        R"--(
        s = String:create('abc')
        s:customError(false)
    )--",
        testing::StartsWith("Unknown exception while trying to call C function from Lua."));

    runExpectingError(
        R"--(
        s = String:create('abc')
        n = toNumeric()
    )--",
        testing::StartsWith("Invalid number of arguments, should be 1, but 0 were given."));

    runExpectingError(
        R"--(
        s = String:create('abc')
        n = toNumeric(s)
        n = toNumeric(s, 2)
    )--",
        testing::StartsWith("Invalid number of arguments, should be 1, but 2 were given."));
}
