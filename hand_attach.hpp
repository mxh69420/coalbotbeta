#pragma once

/* useful for binding smart pointers to a handler so a reference to an argument
 * in an initiator stays alive
 * writing composed operations is usually the best way to go for more complex
 * stuff though
*/


#include <utility>

template <class H, class A>
struct _hand_attachment {
	H originalHand;
	A attachment;
	template <class... Args>
	auto operator()(Args... a){
		return originalHand(a...);
	}
	_hand_attachment(H &&_originalHand, A &&_attachment) :
		originalHand(std::forward<H>(_originalHand)),
		attachment(std::forward<A>(_attachment)){

	}
};

template <class H, class A>
auto hand_attach(H &&hand, A &&attachment){
	return _hand_attachment<H, A>(
		std::forward<H>(hand),
		std::forward<A>(attachment)
	);
}

template <class H, class FirstA, class... A>
auto hand_attach(H &&hand, FirstA &&firstA, A&&... a){
	return hand_attach(
		hand_attach(
			std::forward<H>(hand),
			std::forward<FirstA>(firstA)
		),
		std::forward<A>(a)...
	);
}
