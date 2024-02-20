#ifndef LUABIND_HELPER_HPP
#define LUABIND_HELPER_HPP

#include "lua.hpp"

#include <concepts>
#include <type_traits>

namespace luabind {

// Helper functions to provide easy ways to differentiate between following function/functor types
// 1. Lua_CFunction int(*)(lua_State*) // User should make sure to make this function noexcept
// 2. Function pointer R(*)(Args...) // except/noexcept versions
// 3. Stateless Lambda functor optimized into function pointer as in point 2.
// 4. Stateful Lambda functor

template <typename T>
struct callable_object_helper;

template <typename R, typename T, typename... Args>
struct callable_object_helper<R (T::*)(Args...)> {
    using type = R(Args...);
    using stateless_lambda_type = R (*)(Args...);
    static constexpr bool is_noexcept = false;
};

template <typename R, typename T, typename... Args>
struct callable_object_helper<R (T::*)(Args...) const> : callable_object_helper<R (T::*)(Args...)> {};

template <typename R, typename T, typename... Args>
struct callable_object_helper<R (T::*)(Args...) noexcept> : callable_object_helper<R (T::*)(Args...)> {
    using stateless_lambda_type = R (*)(Args...) noexcept;
    static constexpr bool is_noexcept = true;
};

template <typename R, typename T, typename... Args>
struct callable_object_helper<R (T::*)(Args...) const noexcept> : callable_object_helper<R (T::*)(Args...) noexcept> {};

template <typename T>
struct callable_helper {
    using type = typename callable_object_helper<decltype(&T::operator())>::type;
    static constexpr bool is_noexcept = callable_object_helper<decltype(&T::operator())>::is_noexcept;
    static constexpr bool is_function_ptr = false;
    static constexpr bool is_member_function_ptr = false;
    using self_type = void*;
};

template <typename R, typename... Args>
struct callable_helper<R (*)(Args...)> {
    using type = R(Args...);
    static constexpr bool is_noexcept = false;
    static constexpr bool is_function_ptr = true;
    static constexpr bool is_member_function_ptr = false;
    using self_type = void*;
};

template <typename R, typename... Args>
struct callable_helper<R (*)(Args...) noexcept> : callable_helper<R (*)(Args...)> {
    static constexpr bool is_noexcept = true;
};

template <typename R, typename T, typename... Args>
struct callable_helper<R (T::*)(Args...)> : callable_helper<R (*)(T*, Args...)> {
    static constexpr bool is_member_function_ptr = true;
    using self_type = T*;
};

template <typename R, typename T, typename... Args>
struct callable_helper<R (T::*)(Args...) noexcept> : callable_helper<R (*)(T*, Args...)> {
    static constexpr bool is_noexcept = true;
    static constexpr bool is_member_function_ptr = true;
    using self_type = T*;
};

template <typename R, typename T, typename... Args>
struct callable_helper<R (T::*)(Args...) const> : callable_helper<R (*)(const T*, Args...)> {
    static constexpr bool is_member_function_ptr = true;
    using self_type = const T*;
};

template <typename R, typename T, typename... Args>
struct callable_helper<R (T::*)(Args...) const noexcept> : callable_helper<R (*)(const T*, Args...) noexcept> {
    static constexpr bool is_member_function_ptr = true;
    using self_type = const T*;
};

template <typename T>
struct member_object_pointer_helper;

template <typename R, typename T>
struct member_object_pointer_helper<R T::*> {
    using type = R;
};

template <typename T>
using member_object_pointer_t = member_object_pointer_helper<T>::type;

template <typename F>
concept LuaCFunction = requires(F f) { static_cast<lua_CFunction>(f); };

template <typename F>
concept CFunction = !LuaCFunction<F> && callable_helper<F>::is_function_ptr;

template <typename F>
using stateless_lambda_fptr_t = typename callable_object_helper<decltype(&F::operator())>::stateless_lambda_type;

template <typename F>
concept StatelessLambda = requires(F f) { static_cast<stateless_lambda_fptr_t<F>>(f); };

template <typename T>
concept StateLambda = !StatelessLambda<T> && requires { decltype(&T::operator()) {}; };

template <typename F>
using signature_t = typename callable_helper<std::remove_cvref_t<F>>::type;

template <typename F>
using self_t = typename callable_helper<std::remove_cvref_t<F>>::self_type;

template <typename F>
constexpr bool lua_c_function_v = LuaCFunction<F>;

template <typename F>
constexpr bool function_ptr_v = CFunction<F>;

template <typename F>
constexpr bool member_function_ptr_v = callable_helper<std::remove_cvref_t<F>>::is_member_function_ptr;

template <typename F>
constexpr bool stateless_lambda_v = StatelessLambda<F>;

template <typename F>
constexpr bool state_lambda_v = StateLambda<F>;

template <typename F>
constexpr bool is_noexcept_v = callable_helper<std::remove_cvref_t<F>>::is_noexcept;

} // namespace luabind

#endif // LUABIND_HELPER_HPP
