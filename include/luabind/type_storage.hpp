#ifndef LUABIND_TYPE_STORAGE_HPP
#define LUABIND_TYPE_STORAGE_HPP

#include "lua.hpp"
#include "exception.hpp"

#include <unordered_map>
#include <string>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace luabind {

enum class entry_type {
    none,
    function,
    property,
};

struct entry {
    entry_type type;
    int getter;
    int setter;

    bool has_setter() const {
        return setter != 0;
    }

    static entry none() {
        return entry {.type = entry_type::none};
    }

    static entry function(int idx) {
        return entry {.type = entry_type::function, .getter = idx, .setter = 0};
    }

    static entry property(int getter) {
        return entry {.type = entry_type::property, .getter = getter, .setter = 0};
    }

    static entry property(int getter, int setter) {
        return entry {.type = entry_type::property, .getter = getter, .setter = setter};
    }
};

struct string_hash {
    using is_transparent = void;
    [[nodiscard]] size_t operator()(const char* txt) const {
        return std::hash<std::string_view> {}(txt);
    }
    [[nodiscard]] size_t operator()(std::string_view txt) const {
        return std::hash<std::string_view> {}(txt);
    }
    [[nodiscard]] size_t operator()(const std::string& txt) const {
        return std::hash<std::string> {}(txt);
    }
};

struct type_info;

using index_function_t = int (*)(lua_State*, type_info*);

struct type_info {
    const std::string name;
    const std::vector<type_info*> bases;
    index_function_t index;
    index_function_t new_index;

    int array_getter = 0;
    int array_setter = 0;
    std::unordered_map<std::string, entry, string_hash, std::equal_to<>> entries;
    lua_State* storage;

    type_info(lua_State* L,
              std::string&& type_name,
              std::vector<type_info*>&& bases,
              index_function_t index_functor,
              index_function_t new_index_functor)
        : name(std::move(type_name))
        , bases(std::move(bases))
        , index(index_functor)
        , new_index(new_index_functor) {
        int r = luaL_newmetatable(L, name.c_str());
        if (r == 0) {
            reportError("Type already exists.");
        }
        storage = lua_newthread(L);
        lua_checkstack(storage, 128);
        lua_rawseti(L, -2, 1);

        lua_setglobal(L, name.c_str());
        //  stack is clean
    }

    ~type_info() {
        // TODO add metatable cleanup
    }

    void get_metatable(lua_State* L) const {
        luaL_getmetatable(L, name.c_str());
    }

    std::string_view to_string_view(lua_State* L, int idx) {
        size_t len;
        const char* str = lua_tolstring(L, idx, &len);
        return std::string_view {str, len};
    }

    // [-0, +0|+2, -]
    entry get_entry(lua_State* L, int key_idx, bool setter) {
        const auto name = to_string_view(L, key_idx);
        auto it = entries.find(name);
        if (it == entries.end()) {
            return entry::none();
        }
        const auto& e = it->second;
        const int idx = setter ? e.setter : e.getter;
        if (idx != 0) {
            lua_pushvalue(storage, idx);
            lua_xmove(storage, L, 1);
        }
        return e;
    }

    /**
     * Sets the member function in the metatable of the class
     * The function is the value at the top of the stack
     * The name of the function is the value just below the top
     * [-1, +0, -]
     */
    void set_function(lua_State* L, std::string&& name) {
        lua_xmove(L, storage, 1);
        const int idx = lua_gettop(storage);
        entries.try_emplace(std::move(name), entry::function(idx));
    }

    // [-1, +0, -]
    void set_property_readonly(lua_State* L, std::string&& name) {
        lua_xmove(L, storage, 1);
        const int idx = lua_gettop(storage);
        entries.try_emplace(std::move(name), entry::property(idx));
    }

    // [-2, +0, -]
    void set_property(lua_State* L, std::string&& name) {
        lua_xmove(L, storage, 2);
        const int setter_idx = lua_gettop(storage);
        const int getter_idx = setter_idx - 1;
        entries.try_emplace(std::move(name), entry::property(getter_idx, setter_idx));
    }

    // [-1, +0, -]
    void set_array_getter(lua_State* L) {
        lua_xmove(L, storage, 1);
        array_getter = lua_gettop(storage);
    }

    // [-2, +0, -]
    void set_array_access(lua_State* L) {
        lua_xmove(L, storage, 2);
        array_setter = lua_gettop(storage);
        array_getter = array_setter - 1;
    }

    // [-0, +0|+1, -]
    int get_array_getter(lua_State* L) {
        if (array_getter == 0) {
            return LUA_TNIL;
        }
        lua_pushvalue(storage, array_getter);
        lua_xmove(storage, L, 1);
        return lua_type(L, -1);
    }

    int get_array_setter(lua_State* L) {
        if (array_setter == 0) {
            return LUA_TNIL;
        }
        lua_pushvalue(storage, array_setter);
        lua_xmove(storage, L, 1);
        return lua_type(L, -1);
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
    add_type_info(lua_State* L, std::string name, index_function_t index_functor, index_function_t new_index_functor) {
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
