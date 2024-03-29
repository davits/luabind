#include "lua_test.hpp"

#include <iostream>
#include <memory>
#include <string>

class StringBox : public luabind::Object {
public:
    std::string str;

public:
    StringBox(const std::string& s)
        : str(s) {}

    void set(const std::string& s) {
        str = s;
    }

    const std::string& get() {
        return str;
    }
};

class StrLuaTest : public LuaTest {
protected:
    void SetUp() override {
        const int top = lua_gettop(L);
        luabind::class_<StringBox>(L, "StringBox")
            .constructor<const std::string&>("new")
            .function("set", &StringBox::set)
            .function("get", &StringBox::get)
            .property("str", &StringBox::str);

        EXPECT_EQ(lua_gettop(L), top);
    }
};

TEST_F(StrLuaTest, StringBox) {
    const std::string& strFromNew = runWithResult<std::string>(R"--(
        a = StringBox:new('firstStr')
        return a.str
    )--");

    EXPECT_EQ(strFromNew, "firstStr");

    const std::string& strFromSet = runWithResult<std::string>(R"--(
        a = StringBox:new('firstStr')
        a:set('secondStr') 
        return a:get()
    )--");

    EXPECT_EQ(strFromSet, "secondStr");
}
