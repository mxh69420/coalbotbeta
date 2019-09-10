#ifndef MEM_HAND

#include <utility>

#ifndef MEM_HAND_BIND_FRONT
#include <functional>
#define MEM_HAND_BIND_FRONT std::bind_front
#endif //def MEM_HAND_BIND_FRONT

/*#define MEM_HAND(m, ...) ( \
	MEM_HAND_BIND_FRONT( \
		std::mem_fn( \
			&std::remove_reference_t<decltype(*this)>::m \
		), \
		this->shared_from_this() __VA_OPT__(,) \
		__VA_ARGS__ \
	) \
)

#define MEM_HAND_NO_REF(m, ...) ( \
	MEM_HAND_BIND_FRONT( \
		std::mem_fn( \
			&std::remove_reference_t<decltype(*this)>::m \
		), \
		this __VA_OPT__(,) \
		__VA_ARGS__ \
	) \
)*/

#define MEM_HAND(m, ...) ( \
	MEM_HAND_BIND_FRONT( \
		&std::remove_reference_t<decltype(*this)>::m, \
		this->shared_from_this() __VA_OPT__(,) \
		__VA_ARGS__ \
	) \
)

#define MEM_HAND_NO_REF(m, ...) ( \
	MEM_HAND_BIND_FRONT( \
		&std::remove_reference_t<decltype(*this)>::m, \
		this->shared_from_this() __VA_OPT__(,) \
		__VA_ARGS__ \
	) \
)

#endif
