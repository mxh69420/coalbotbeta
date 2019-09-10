#pragma once

//a c++17 compatible std::bind_front implementation

#include <functional>
#include <utility>

namespace coal {
namespace detail {

template <class F, class A>
struct _bind_obj {
	F originalFunc;
	A arg;
	template <class... Args>
	auto operator()(Args&&... a){
		return std::invoke(originalFunc, arg, std::forward<Args>(a)...);
	}
	_bind_obj(F &&_originalFunc, A &&_arg) :
		originalFunc(std::forward<F>(_originalFunc)),
		arg(std::forward<A>(_arg)){

	}
};

template <class F, class A>
auto bind_front(F &&func, A &&arg){
	return _bind_obj<F, A>(
		std::forward<F>(func),
		std::forward<A>(arg)
	);
}

template <class F, class FirstA, class... A>
auto bind_front(F &&func, FirstA &&firstA, A&&... a){
	return bind_front(
		bind_front(
			std::forward<F>(func),
			std::forward<FirstA>(firstA)
		),
		std::forward<A>(a)...
	);
}

} //end namespace detail
} //end namespace coal
