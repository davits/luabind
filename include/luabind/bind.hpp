#ifndef LUABIND_BIND_HPP
#define LUABIND_BIND_HPP

#include "object.hpp"
#include "exception.hpp"
#include "mirror.hpp"
#include "type_storage.hpp"
#include "wrapper.hpp"

#include <type_traits>

namespace luabind {

template <typename Type, typename... Bases>
class class_ {
    static_assert(std::is_base_of_v<Object, Type>,
                  "Type should be descendant from luabind::Object to ensure correct pointer transformations.");

public:
    class_(lua_State* L, const std::string_view name)
        : _L(L) {
        _info = type_storage::add_type_info<Type, Bases...>(L, std::string {name}, index_impl, new_index_impl);
        _info->get_metatable(L);
        int mt_idx = lua_gettop(L);

        if constexpr (std::is_default_constructible_v<Type>) {
            constructor<>("new");
        }

        // one __index to rule them all and in lua bind them
        lua_pushliteral(L, "__index");
        functor_to_lua(L, index_);
        lua_rawset(L, mt_idx);

        lua_pushliteral(L, "__newindex");
        functor_to_lua(L, new_index);
        lua_rawset(L, mt_idx);

        user_data::add_destructing_functions(L, mt_idx);
        function("delete", &user_data::destruct);
        lua_pop(L, 1); // pop metatable
    }

    template <typename... Args>
    class_& constructor(const std::string_view name) {
        static_assert(std::is_constructible_v<Type, Args...>, "class should be constructible with given arguments");
        return constructor(name, &ctor_wrapper<Type, Args...>::safe_invoke);
    }

    template <typename... Args>
    class_& construct_shared(const std::string_view name) {
        static_assert(std::is_constructible_v<Type, Args...>, "class should be constructible with given arguments");
        return constructor(name, &shared_ctor_wrapper<Type, Args...>::safe_invoke);
    }

    template <typename Functor>
    class_& constructor(const std::string_view name, Functor&& func) {
        _info->get_metatable(_L);
        value_mirror<std::string_view>::to_lua(_L, name);
        class_functor_to_lua(_L, std::forward<Functor>(func));
        lua_rawset(_L, -3);
        lua_pop(_L, 1); // pop metatable
        return *this;
    }

    template <typename Func>
    class_& class_function(const std::string_view name, Func&& func) {
        _info->get_metatable(_L);
        value_mirror<std::string_view>::to_lua(_L, name);
        class_functor_to_lua(_L, std::forward<Func>(func));
        lua_rawset(_L, -3);
        lua_pop(_L, 1); // pop metatable
        return *this;
    }

public:
    template <ValidMemberFunctor<Type> Func>
    class_& function(const std::string_view name, Func&& func) {
        functor_to_lua(_L, std::forward<Func>(func));
        _info->set_function(_L, std::string {name});
        return *this;
    }

    template <typename Member>
    class_& property_readonly(const std::string_view name, Member Type::*memberPtr) {
        return property(name, [memberPtr](const Type* obj) { return (obj->*memberPtr); });
    }

    template <typename Functor>
        requires(!std::is_member_object_pointer_v<Functor>)
    class_& property(const std::string_view name, Functor&& getter) {
        functor_to_lua(_L, std::forward<Functor>(getter));
        _info->set_property_readonly(_L, std::string {name});
        return *this;
    }

    template <typename MemberPtr>
        requires(std::is_member_object_pointer_v<MemberPtr>)
    class_& property(const std::string_view name, MemberPtr memberPtr) {
        using Member = member_object_pointer_t<MemberPtr>;
        if constexpr (std::is_const_v<MemberPtr>) {
            return property(name, [memberPtr](const Type* obj) -> Member { return (obj->*memberPtr); });
        } else {
            return property(
                name,
                [memberPtr](Type* obj) -> Member { return (obj->*memberPtr); },
                [memberPtr](Type* obj, Member value) { (obj->*memberPtr) = std::move(value); });
        }
    }

    template <typename GetFunctor, typename SetFunctor>
        requires(!std::is_member_object_pointer_v<GetFunctor> && !std::is_member_object_pointer_v<SetFunctor>)
    class_& property(const std::string_view name, GetFunctor&& getter, SetFunctor&& setter) {
        functor_to_lua(_L, std::forward<GetFunctor>(getter));
        functor_to_lua(_L, std::forward<SetFunctor>(setter));
        _info->set_property(_L, std::string {name});
        return *this;
    }

    template <typename GetFunctor>
    class_& array_access(GetFunctor&& getter) {
        functor_to_lua(_L, std::forward<GetFunctor>(getter));
        _info->set_array_getter(_L);
        return *this;
    }

    template <typename GetFunctor, typename SetFunctor>
    class_& array_access(GetFunctor&& getter, SetFunctor&& setter) {
        functor_to_lua(_L, std::forward<GetFunctor>(getter));
        functor_to_lua(_L, std::forward<SetFunctor>(setter));
        _info->set_array_access(_L);
        return *this;
    }

private:
    static int index_(lua_State* L) {
        auto* ud = user_data::from_lua(L, 1);
        int r = index_impl(L, ud->info);
        if (r >= 0) return r;
        // if there is no result from bound C++, look in the lua table bound to this object
        user_data::get_custom_table(L, 1); // custom table
        lua_pushvalue(L, 2); // key
        lua_rawget(L, -2);
        return 1;
    }

    static int index_impl(lua_State* L, type_info* info) {
        const bool is_integer = lua_isinteger(L, 2);
        const int key_type = lua_type(L, 2);
        if (!is_integer && key_type != LUA_TSTRING) {
            luaL_error(L, "Key type should be integer or string, '%s' is provided.", lua_typename(L, key_type));
        }
        if (is_integer) {
            const int top = lua_gettop(L);
            int r = info->get_array_getter(L);
            if (r == LUA_TNIL) {
                luaL_error(L, "Type '%s' does not provide array get access.", info->name.c_str());
            }
            lua_pushvalue(L, 1); // self
            lua_pushvalue(L, 2); // key
            lua_call(L, 2, LUA_MULTRET);
            return lua_gettop(L) - top;
        }
        // key is a string
        auto e = info->get_entry(L, 2, false);
        switch (e.type) {
        case entry_type::property: {
            const int top = lua_gettop(L) - 1; // -1 for property getter
            lua_pushvalue(L, 1); // self
            lua_call(L, 1, LUA_MULTRET); // call property getter
            return lua_gettop(L) - top;
        }
        case entry_type::function:
            return 1; // return function on the top of the stack
        case entry_type::none:
            break;
        }
        //  lua doesn't do recursive index lookup through metatables
        //  if __index is bound to a C function, so we do it ourselves.
        for (type_info* base : info->bases) {
            int r = base->index(L, base);
            if (r >= 0) {
                return r;
            }
        }
        return -1;
    }

private:
    static int new_index(lua_State* L) {
        auto* ud = user_data::from_lua(L, 1);
        int r = new_index_impl(L, ud->info);
        if (r >= 0) return r;
        // if there is no result in C++ add new value to the lua table bound to this object
        user_data::get_custom_table(L, 1); // custom table
        lua_pushvalue(L, 2); // key
        lua_pushvalue(L, 3); // new value
        lua_rawset(L, -3);
        return 0;
    }

    static int new_index_impl(lua_State* L, type_info* info) {
        const bool is_integer = lua_isinteger(L, 2);
        const int key_type = lua_type(L, 2);
        if (!is_integer && key_type != LUA_TSTRING) {
            luaL_error(L, "Key type should be integer or string, '%s' is provided.", lua_typename(L, key_type));
        }
        if (is_integer) {
            int r = info->get_array_setter(L);
            if (r == LUA_TNIL) {
                luaL_error(L, "Type '%s' does not provide array set access.", info->name.c_str());
            }
            const int top = lua_gettop(L) - 1; // -1 for array setter
            lua_pushvalue(L, 1); // self
            lua_pushvalue(L, 2); // key
            lua_pushvalue(L, 3); // value
            lua_call(L, 3, LUA_MULTRET);
            return lua_gettop(L) - top;
        }
        // key is a string
        auto e = info->get_entry(L, 2, true);
        switch (e.type) {
        case entry_type::property: {
            if (!e.has_setter()) {
                auto key = value_mirror<std::string_view>::from_lua(L, 2);
                luaL_error(L, "Property '%s' is read only.", key.data());
            }
            const int top = lua_gettop(L) - 1; // -1 for property setter
            lua_pushvalue(L, 1); // self
            lua_pushvalue(L, 3); // value
            lua_call(L, 2, LUA_MULTRET); // call property setter
            return lua_gettop(L) - top;
        }
        case entry_type::function:
            return -1; // asigning to a function, redirect to custom table
        case entry_type::none:
            break;
        }

        // lua doesn't do recursive index lookup through metatables
        // if __new_index is bound to a C function, so we do it ourselves.
        for (type_info* base : info->bases) {
            int r = base->new_index(L, base);
            if (r >= 0) {
                return r;
            }
        }
        return -1; // redirect to custom table
    }

private:
    lua_State* _L;
    type_info* _info;
};

template <typename Functor>
inline void function(lua_State* L, const std::string_view name, Functor&& func) {
    functor_to_lua(L, std::forward<Functor>(func));
    lua_setglobal(L, name.data());
}

} // namespace luabind

#endif // LUABIND_BIND_HPP
