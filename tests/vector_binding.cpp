#include "lua_test.hpp"

#include <vector>

std::vector<int> createVector() {
    return {1, 2, 3, 4, 5};
}

void processVector(const std::vector<int>& vec) {
    // Just verify we can receive it - function exists to test parameter passing
    (void)vec; // Suppress unused parameter warning
}

std::vector<int> modifyVector(std::vector<int> vec) {
    for (auto& v : vec) {
        v *= 2;
    }
    return vec;
}

class VectorTest : public LuaTest {
protected:
    void SetUp() override {
        const int top = lua_gettop(L);
        luabind::function(L, "createVector", &createVector);
        luabind::function(L, "processVector", &processVector);
        luabind::function(L, "modifyVector", &modifyVector);
        EXPECT_EQ(lua_gettop(L), top);
    }
};

TEST_F(VectorTest, VectorToLua) {
    std::vector<int> result = runWithResult<std::vector<int>>(R"--(
        return createVector()
    )--");
    
    EXPECT_EQ(result.size(), 5);
    EXPECT_EQ(result[0], 1);
    EXPECT_EQ(result[1], 2);
    EXPECT_EQ(result[2], 3);
    EXPECT_EQ(result[3], 4);
    EXPECT_EQ(result[4], 5);
}

TEST_F(VectorTest, VectorFromLua) {
    int r = run(R"--(
        local vec = {10, 20, 30}
        processVector(vec)
    )--");
    EXPECT_EQ(r, LUA_OK);
}

TEST_F(VectorTest, VectorRoundTrip) {
    std::vector<int> result = runWithResult<std::vector<int>>(R"--(
        local vec = {1, 2, 3}
        return modifyVector(vec)
    )--");
    
    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], 2);  // 1 * 2
    EXPECT_EQ(result[1], 4);  // 2 * 2
    EXPECT_EQ(result[2], 6);  // 3 * 2
}

TEST_F(VectorTest, VectorEmpty) {
    std::vector<int> result = runWithResult<std::vector<int>>(R"--(
        return {}
    )--");
    EXPECT_EQ(result.size(), 0);
}

