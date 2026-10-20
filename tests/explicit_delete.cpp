#include "lua_test.hpp"

class Deletable : public luabind::Object {
public:
    Deletable() = default;
    ~Deletable() override {
        deletedCount++;
    }

    int value() const {
        return 1;
    }

public:
    static int deletedCount;
};

int Deletable::deletedCount = 0;

class Kept : public luabind::Object {};

class ExplicitDeleteTest : public LuaTest {
protected:
    void SetUp() override {
        const int top = lua_gettop(L);
        luabind::class_<Deletable>(L, "Deletable")
            .constructor<>("new")
            .construct_shared<>("makeShared")
            .function("value", &Deletable::value)
            .class_function("isNull", [](std::shared_ptr<Deletable> obj) { return obj == nullptr; });

        EXPECT_EQ(lua_gettop(L), top);
    }
};

TEST_F(ExplicitDeleteTest, ExplicitDelete) {
    run(R"--(
        obj = Deletable:new()
        obj:delete()
        obj:delete()
        obj = null
        collectgarbage()
    )--");

    EXPECT_EQ(Deletable::deletedCount, 1);

    run(R"--(
        obj = Deletable:makeShared()
        obj:delete()
        obj:delete()
        obj = null
        collectgarbage()
    )--");

    EXPECT_EQ(Deletable::deletedCount, 2);
}

TEST_F(ExplicitDeleteTest, CallAfterDelete) {
    runExpectingError(R"--(
        obj = Deletable:new()
        obj:delete()
        obj:value()
    )--",
                      testing::StartsWith("Argument at 1 is a deleted 'Deletable'."));

    runExpectingError(R"--(
        obj = Deletable:makeShared()
        obj:delete()
        obj:value()
    )--",
                      testing::StartsWith("Argument at 1 is a deleted 'Deletable'."));
}

TEST_F(ExplicitDeleteTest, DeletedSharedIsNotNull) {
    runExpectingError(R"--(
        obj = Deletable:makeShared()
        obj:delete()
        Deletable:isNull(obj)
    )--",
                      testing::StartsWith("Argument at 2 is a deleted 'Deletable'."));
}

TEST_F(ExplicitDeleteTest, DeleteOfANonObject) {
    runExpectingError(R"--(
        obj = Deletable:new()
        obj.delete(5)
    )--",
                      testing::HasSubstr("userdata expected, got number"));
}

TEST_F(ExplicitDeleteTest, DeleteCanBeRebound) {
    luabind::class_<Kept>(L, "Kept").function("delete", [](lua_State* L) -> int {
        return luaL_error(L, "delete is refused");
    });
    runExpectingError(R"--(
        Kept:new():delete()
    )--",
                      testing::HasSubstr("delete is refused"));
}
