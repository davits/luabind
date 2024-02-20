#ifndef LUABIND_TYPE_STORAGE_HPP
#define LUABIND_TYPE_STORAGE_HPP

#include "lua.hpp"
#include "exception.hpp"

#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include <iostream>

namespace luabind {

struct property_data {
    lua_CFunction getter;
    lua_CFunction setter;

    property_data(lua_CFunction g)
        : getter(g)
        , setter(nullptr) {}

    property_data(lua_CFunction g, lua_CFunction s)
        : getter(g)
        , setter(s) {}
};

struct type_info {
    const std::string name;
    const std::vector<type_info*> bases;
    lua_CFunction index;
    lua_CFunction new_index;

    static constexpr int functions_idx = 0;
    static constexpr int properties_idx = 1;
    static constexpr int array_access_idx = 3;

    type_info(lua_State* L,
              std::string&& type_name,
              std::vector<type_info*>&& bases,
              lua_CFunction index_functor,
              lua_CFunction new_index_functor)
        : name(std::move(type_name))
        , bases(std::move(bases))
        , index(index_functor)
        , new_index(new_index_functor) {
        int r = luaL_newmetatable(L, name.c_str());
        if (r == 0) {
            reportError("Type already exists.");
        }
        lua_newtable(L);
        lua_rawseti(L, -2, functions_idx);
        lua_newtable(L);
        lua_rawseti(L, -2, properties_idx);

        lua_setglobal(L, name.c_str());
        //  stack is clean
    }

    void get_metatable(lua_State* L) const {
        luaL_getmetatable(L, name.c_str());
    }

    // [-0, +1, -]
    void get_functions_table(lua_State* L) const {
        get_metatable(L);
        lua_rawgeti(L, -1, functions_idx);
        lua_remove(L, -2);
    }

    // [-0, +1, -]
    void get_property_table(lua_State* L) const {
        get_metatable(L);
        lua_rawgeti(L, -1, properties_idx);
        lua_remove(L, -2);
    }

    /**
     * Sets the member function in the metatable of the class
     * The function is the value at the top of the stack
     * The name of the function is the value just below the top
     * [-2, +0, -]
     */
    void set_function(lua_State* L) {
        get_functions_table(L);
        lua_pushvalue(L, -3);
        lua_pushvalue(L, -3);
        lua_rawset(L, -3);
        lua_pop(L, 3);
    }

    /// [-0, +1, -]
    /**
     * Gets the member function from the metatable of the class
     * The name of the function is the value at the top of the stack
     */
    int get_function(lua_State* L) {
        get_functions_table(L);
        lua_pushvalue(L, -2);
        int r = lua_rawget(L, -2);
        lua_remove(L, -2);
        return r;
    }

    // [-2, +0, -]
    void set_property(lua_State* L) {
        get_property_table(L);
        lua_pushvalue(L, -3);
        lua_pushvalue(L, -3);
        lua_rawset(L, -3);
        lua_pop(L, 3);
    }

    // [-0, +1, -]
    int get_property(lua_State* L) {
        get_property_table(L);
        lua_pushvalue(L, -2);
        int r = lua_rawget(L, -2);
        lua_remove(L, -2);
        return r;
    }

    void set_array_access(lua_State* L) {
        get_metatable(L);
        lua_pushvalue(L, -2);
        lua_rawseti(L, -2, array_access_idx);
        lua_pop(L, 2); // pop metatable and array getter
    }

    int get_array_access(lua_State* L) {
        get_metatable(L);
        int r = lua_rawgeti(L, -1, array_access_idx);
        lua_remove(L, -2);
        return r;
    }
};

class type_storage {
private:
    type_storage() = default;

public:
    static type_storage& get_instance(lua_State* L) {
        static const char* storage_name = "luabind::type_storage";
        int r = lua_getfield(L, LUA_REGISTRYINDEX, storage_name);
        if (r == LUA_TUSERDATA) {
            void* p = lua_touserdata(L, -1);
            auto ud = static_cast<type_storage**>(p);
            type_storage* instance = *ud;
            lua_pop(L, 1);
            return *instance;
        }
        lua_pop(L, 1);
        type_storage* instance = new type_storage;
        void* p = lua_newuserdatauv(L, sizeof(void*), 0);
        auto ud = static_cast<type_storage**>(p);
        *ud = instance;

        lua_newtable(L);
        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, [](lua_State* L) -> int {
            void* p = lua_touserdata(L, 1);
            auto ud = static_cast<type_storage**>(p);
            type_storage* instance = *ud;
            delete instance;
            return 0;
        });
        lua_rawset(L, -3);
        lua_setmetatable(L, -2);
        lua_setfield(L, LUA_REGISTRYINDEX, storage_name);
        return *instance;
    }

    template <typename Type, typename... Bases>
    static type_info*
    add_type_info(lua_State* L, std::string name, lua_CFunction index_functor, lua_CFunction new_index_functor) {
        type_storage& instance = get_instance(L);
        const auto index = std::type_index(typeid(Type));
        auto it = instance.m_types.find(index);
        if (it != instance.m_types.end()) {
            return &it->second;
        }
        std::vector<type_info*> bases;
        bases.reserve(sizeof...(Bases));
        (add_base_class<Bases>(instance, bases), ...);
        auto r = instance.m_types.emplace(
            index, type_info(L, std::move(name), std::move(bases), index_functor, new_index_functor));
        return &(r.first->second);
    }

    template <typename T>
    static type_info* find_type_info(lua_State* L, const T* obj) {
        type_info* info = find_type_info(L, std::type_index(typeid(*obj)));
        if (info == nullptr) {
            info = find_type_info<T>(L);
        }
        return info;
    }

    template <typename T>
    static std::string_view type_name(lua_State* L) {
        type_info* info = find_type_info<T>(L);
        return info != nullptr ? std::string_view {info->name} : std::string_view {typeid(T).name()};
    }

    template <typename T>
    static type_info* find_type_info(lua_State* L) {
        return find_type_info(L, std::type_index(typeid(T)));
    }

    static type_info* find_type_info(lua_State* L, std::type_index idx) {
        type_storage& instance = get_instance(L);
        auto it = instance.m_types.find(idx);
        return it != instance.m_types.end() ? &it->second : nullptr;
    }

private:
    template <typename Base>
    static void add_base_class(type_storage& instance, std::vector<type_info*>& bases) {
        static_assert(std::is_class_v<Base>);
        auto base_it = instance.m_types.find(std::type_index(typeid(Base)));
        if (base_it == instance.m_types.end()) {
            reportError("Base class should be bound before child.");
        }
        bases.push_back(&(base_it->second));
    }

public:
    using types = std::unordered_map<std::type_index, type_info>;

private:
    types m_types;
};

} // namespace luabind

#endif // LUABIND_TYPE_STORAGE_HPP
