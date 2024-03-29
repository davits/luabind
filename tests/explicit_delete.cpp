#include "lua_test.hpp"

class Deletable : public luabind::Object {
public:
    Deletable() = default;
    ~Deletable() override {
        deletedCount++;
    }

public:
    static int deletedCount;
};

int Deletable::deletedCount = 0;

class ExplicitDeleteTest : public LuaTest {
protected:
    void SetUp() override {
        const int top = lua_gettop(L);
        luabind::class_<Deletable>(L, "Deletable").constructor<>("new").construct_shared<>("makeShared");

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
