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
struct function_first_arg_helper {
    using type = void;
};

template <typename R, typename First, typename... Args>
struct function_first_arg_helper<R(First, Args...)> {
    using type = First;
};

template <typename R, typename First, typename... Args>
struct function_first_arg_helper<R(First, Args...) const> {
    using type = First;
};

template <typename R, typename First, typename... Args>
struct function_first_arg_helper<R(First, Args...) noexcept> {
    using type = First;
};

template <typename R, typename First, typename... Args>
struct function_first_arg_helper<R(First, Args...) const noexcept> {
    using type = First;
};

template <typename T>
using function_first_arg_t = typename function_first_arg_helper<T>::type;

template <typename T>
struct callable_object_helper;

template <typename R, typename T, typename... Args>
struct callable_object_helper<R (T::*)(Args...)> {
    using type = R(Args...);
    using lambda_convertible_type = R (*)(Args...);
};

template <typename R, typename T, typename... Args>
struct callable_object_helper<R (T::*)(Args...) const> : callable_object_helper<R (T::*)(Args...)> {};

template <typename R, typename T, typename... Args>
struct callable_object_helper<R (T::*)(Args...) noexcept> : callable_object_helper<R (T::*)(Args...)> {
    using lambda_convertible_type = R (*)(Args...) noexcept;
};

template <typename R, typename T, typename... Args>
struct callable_object_helper<R (T::*)(Args...) const noexcept> : callable_object_helper<R (T::*)(Args...) noexcept> {};

template <typename T>
struct callable_helper {
    static_assert(!std::is_function_v<T>, "Please use function pointer notation.");
    using type = typename callable_object_helper<decltype(&T::operator())>::type;
    static constexpr bool is_function_ptr = false;
    using self_type = void*;
};

template <typename R, typename... Args>
struct callable_helper<R (*)(Args...)> {
    using type = R(Args...);
    static constexpr bool is_function_ptr = true;
    using self_type = void*;
};

template <typename R, typename... Args>
struct callable_helper<R (*)(Args...) noexcept> : callable_helper<R (*)(Args...)> {};

template <typename R, typename T, typename... Args>
struct callable_helper<R (T::*)(Args...)> : callable_helper<R (*)(T*, Args...)> {
    using self_type = T*;
};

template <typename R, typename T, typename... Args>
struct callable_helper<R (T::*)(Args...) noexcept> : callable_helper<R (*)(T*, Args...)> {
    using self_type = T*;
};

template <typename R, typename T, typename... Args>
struct callable_helper<R (T::*)(Args...) const> : callable_helper<R (*)(const T*, Args...)> {
    using self_type = const T*;
};

template <typename R, typename T, typename... Args>
struct callable_helper<R (T::*)(Args...) const noexcept> : callable_helper<R (*)(const T*, Args...) noexcept> {
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
using lambda_convertible_t = typename callable_object_helper<decltype(&F::operator())>::lambda_convertible_type;

template <typename F>
concept StatelessLambda = requires(F f) { static_cast<lambda_convertible_t<F>>(f); };

template <typename T>
concept CallableObject = requires { decltype(&T::operator()) {}; };

template <typename F>
using signature_t = typename callable_helper<std::remove_cvref_t<F>>::type;

template <typename F>
using first_arg_t = function_first_arg_t<signature_t<F>>;

template <typename T>
using strip_t = std::remove_const_t<std::remove_pointer_t<std::remove_cvref_t<T>>>;

template <typename F>
using self_t = typename callable_helper<std::remove_cvref_t<F>>::self_type;

template <typename F>
constexpr bool is_lua_c_function_v = LuaCFunction<F>;

template <typename F>
constexpr bool is_lua_c_lambda_v = CallableObject<F> && std::is_same_v<signature_t<F>, int(lua_State*)>;

template <typename F>
constexpr bool is_function_ptr_v = std::is_pointer_v<F> && std::is_function_v<std::remove_pointer_t<F>>;

template <typename F>
constexpr bool stateless_lambda_v = StatelessLambda<F>;

template <typename F>
constexpr bool callable_object_v = CallableObject<F>;

template <typename Functor, typename Class>
concept ValidMemberFunctor =
    is_lua_c_function_v<Functor> || is_lua_c_lambda_v<Functor> ||
    ((std::is_pointer_v<first_arg_t<Functor>> ||
      std::is_reference_v<first_arg_t<Functor>>)&&std::is_same_v<strip_t<first_arg_t<Functor>>, Class>);

} // namespace luabind

#endif // LUABIND_HELPER_HPP
