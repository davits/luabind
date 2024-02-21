#ifndef LUABIND_WRAPPER_HPP
#define LUABIND_WRAPPER_HPP

#include "helper.hpp"
#include "mirror.hpp"
#include "traits.hpp"

#include <exception>
#include <type_traits>

namespace luabind {

template <typename CRTP>
struct exception_safe_wrapper {
    static int safe_invoke(lua_State* L) {
        try {
            return CRTP::invoke(L);
        } catch (void*) {
            // lua throws lua_longjmp* if compiled with C++ exceptions when yielding or reporting error
            // rethrow to not interrupt lua logic flow in that case.
            // assuming that normal code would not throw pointer
            throw;
        } catch (const luabind::error& e) {
            lua_pushstring(L, e.what());
        } catch (const std::exception& e) {
            lua_pushstring(L, e.what());
        } catch (...) {
            lua_pushliteral(L, "Unknown exception while trying to call C function from Lua.");
        }
        lua_error(L); // [[noreturn]]
        return 0;
    }
};

template <typename Type, typename... Args>
struct ctor_wrapper : exception_safe_wrapper<ctor_wrapper<Type, Args...>> {
    static_assert(std::conjunction_v<valid_lua_arg<Args>...>);

    static int invoke(lua_State* L) {
        // 1st argument is the metatable
        int num_args = lua_gettop(L) - 1;
        if (num_args != sizeof...(Args)) {
            reportError("Invalid number of arguments, should be %zu, but %i were given.", sizeof...(Args), num_args);
        }
        return indexed_call_helper(L, index_sequence<2, sizeof...(Args)> {});
    }

    template <size_t... Indices>
    static int indexed_call_helper(lua_State* L, std::index_sequence<Indices...>) {
        return lua_user_data<Type>::to_lua(L, value_mirror<Args>::from_lua(L, Indices)...);
    }
};

template <typename Type, typename... Args>
struct shared_ctor_wrapper : exception_safe_wrapper<shared_ctor_wrapper<Type, Args...>> {
    static_assert(std::conjunction_v<valid_lua_arg<Args>...>);

    static int invoke(lua_State* L) {
        // 1st argument is the metatable
        int num_args = lua_gettop(L) - 1;
        if (num_args != sizeof...(Args)) {
            reportError("Invalid number of arguments, should be %zu, but %i were given.", sizeof...(Args), num_args);
        }
        return indexed_call_helper(L, index_sequence<2, sizeof...(Args)> {});
    }

    template <size_t... Indices>
    static int indexed_call_helper(lua_State* L, std::index_sequence<Indices...>) {
        return shared_user_data::to_lua(L, std::make_shared<Type>(value_mirror<Args>::from_lua(L, Indices)...));
    }
};

template <typename T>
struct mem_fun_wrapper;

template <typename R, typename T, typename... Args>
struct mem_fun_wrapper<R (T::*)(Args...)> {
    using Ptr = R (T::*)(Args...);
    Ptr func;

    mem_fun_wrapper(Ptr ptr)
        : func(ptr) {}

    R operator()(T* self, Args... args) {
        return (self->*func)(args...);
    }
};

template <typename R, typename T, typename... Args>
struct mem_fun_wrapper<R (T::*)(Args...) const> {
    using Ptr = R (T::*)(Args...) const;
    Ptr func;

    mem_fun_wrapper(Ptr ptr)
        : func(ptr) {}

    R operator()(const T* self, Args... args) {
        return (self->*func)(args...);
    }
};

template <typename Functor, typename Signature, size_t ArgStart>
struct invoker;

template <typename Functor, typename R, typename... Args, size_t ArgStart>
struct invoker<Functor, R(Args...), ArgStart> {
    static int invoke(lua_State* L, Functor& func) {
        try {
            return invoke_helper(L, func);
        } catch (void*) {
            // lua throws lua_longjmp* if compiled with C++ exceptions when yielding or reporting error
            // rethrow to not interrupt lua logic flow in that case.
            // assuming that normal code would not throw pointer
            throw;
        } catch (const luabind::error& e) {
            lua_pushstring(L, e.what());
        } catch (const std::exception& e) {
            lua_pushstring(L, e.what());
        } catch (...) {
            lua_pushliteral(L, "Unknown exception while trying to call C function from Lua.");
        }
        lua_error(L); // [[noreturn]]
        return 0;
    }

    static int invoke_helper(lua_State* L, Functor& func) {
        const int num_args = lua_gettop(L) - (ArgStart - 1);
        if (num_args != sizeof...(Args)) {
            reportError("Invalid number of arguments, should be %zu, but %i were given.", sizeof...(Args), num_args);
        }
        return indexed_invoke_helper(L, func, index_sequence<ArgStart, sizeof...(Args)> {});
    }

    template <size_t... Indices>
    static int indexed_invoke_helper(lua_State* L, Functor& func, std::index_sequence<Indices...>) {
        if constexpr (std::is_same_v<R, void>) {
            std::invoke(func, value_mirror<Args>::from_lua(L, Indices)...);
            return 0;
        } else {
            return value_mirror<R>::to_lua(L, std::invoke(func, value_mirror<Args>::from_lua(L, Indices)...));
        }
    }
};

template <typename Functor, size_t ArgStart = 1>
struct functor_wrapper {
    using Signature = signature_t<Functor>;

    static int invoke(lua_State* L) {
        const int func_idx = lua_upvalueindex(1);
        if constexpr (is_function_ptr_v<Functor>) {
            Functor func = reinterpret_cast<Functor>(lua_touserdata(L, func_idx));
            return invoker<Functor, Signature, ArgStart>::invoke(L, func);
        } else {
            Functor* func = static_cast<Functor*>(lua_touserdata(L, func_idx));
            return invoker<Functor, Signature, ArgStart>::invoke(L, *func);
        }
    }

    static void to_lua(lua_State* L, const Functor& func) {
        to_lua(L, Functor {func});
    }

    static void to_lua(lua_State* L, Functor&& func) {
        if constexpr (is_function_ptr_v<Functor>) {
            lua_pushlightuserdata(L, reinterpret_cast<void*>(func));
            lua_pushcclosure(L, &invoke, 1); // create closure with user data as upvalue
        } else {
            void* ud = lua_newuserdatauv(L, sizeof(Functor), 0);
            new (ud) Functor(std::move(func)); // std::function as user data

            if constexpr (!std::is_trivially_destructible_v<Functor>) {
                lua_newtable(L);
                lua_pushliteral(L, "__gc");
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    void* ud = lua_touserdata(L, 1);
                    Functor* storage = static_cast<Functor*>(ud);
                    storage->~Functor();
                    return 0;
                });
                lua_rawset(L, -3); // set __gc in table
                lua_setmetatable(L, -2); // set table as metatable for user data
            }
            lua_pushcclosure(L, &invoke, 1); // create closure with user data as upvalue
        }
    }
};

template <typename Functor, size_t ArgStart = 1>
void functor_to_lua(lua_State* L, Functor&& func) {
    using F = std::remove_cvref_t<Functor>;
    if constexpr (is_lua_c_function_v<F>) {
        lua_pushcfunction(L, func);
    } else if constexpr (std::is_member_function_pointer_v<F>) {
        functor_to_lua(L, mem_fun_wrapper<F>(func));
    } else if (is_function_ptr_v<F>) {
        functor_wrapper<F, ArgStart>::to_lua(L, func);
    } else if constexpr (stateless_lambda_v<F>) {
        auto fptr = static_cast<lambda_convertible_t<F>>(func);
        functor_wrapper<lambda_convertible_t<F>, ArgStart>::to_lua(L, fptr);
    } else if constexpr (callable_object_v<F>) {
        functor_wrapper<F, ArgStart>::to_lua(L, std::forward<Functor>(func));
    } else {
        static_assert("Unsupported function type.");
    }
}

template <typename Functor>
void class_functor_to_lua(lua_State* L, Functor&& func) {
    functor_to_lua<Functor, 2>(L, std::forward<Functor>(func));
}

} // namespace luabind

#endif // LUABIND_WRAPPER_HPP
