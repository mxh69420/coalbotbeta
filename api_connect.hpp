#pragma once

//#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/beast/core/async_base.hpp>
//#include <boost/beast.hpp>
#include <memory>
#include <functional>
#include <utility>

/*#define MEM_HAND(m) ( \
	std::bind_front( \
		std::mem_fn( \
			&std::remove_reference_t<decltype(*this)>::m \
		), \
		this->shared_from_this() \
	)\
)*/

#include "mem_hand.hpp"

using namespace boost::asio;
using namespace boost::beast;
namespace sys = boost::system;

ssl::context make_discordapp_context(){
	ssl::context ctx(ssl::context::tlsv12_client);
	ctx.set_default_verify_paths();
	ctx.set_verify_mode(ssl::verify_peer);
	ctx.set_verify_callback(ssl::rfc2818_verification("discordapp.com"));
	return ctx;
}

ssl::context &get_discordapp_context(){
	static char ctx_buf[sizeof(ssl::context)];
	static bool init = false;
	if(init) return *((ssl::context *) ctx_buf);
	new(ctx_buf) ssl::context(make_discordapp_context());
	init = true;
	return *((ssl::context *) ctx_buf);
}

template <class S, class H>
auto discord_api_connect(S &stream, H &&hand){
	using signature = void(sys::error_code);
	using completion_type = async_completion<
		H,
		signature
	>;
	using handler_type = typename completion_type::completion_handler_type;
	using base_type = async_base<
		handler_type,
		typename S::executor_type
	>;
	struct op : public base_type, public std::enable_shared_from_this<op> {
		S &stream;
		ip::tcp::resolver resolve;

		using resolve_type = ip::tcp::resolver::results_type;

		op(S &_stream, handler_type &hand) :
			stream(_stream),
			base_type(std::move(hand), _stream.get_executor()),
			resolve(_stream.get_executor()){

		}

		void start(){
			resolve.async_resolve(
				"discordapp.com",
				"https",
				MEM_HAND(on_resolved)
			);
		}

		void on_resolved(
			const sys::error_code &ec,
			const resolve_type &results
		){
			if(ec){
				this->complete_now(ec);
				return;
			}
			async_connect(
				stream.lowest_layer(),
				results,
				[&](sys::error_code ec, const ip::tcp::endpoint &e){
					if(ec) std::cout << "discord_api_connect: " << ec.message() << '\n';
					std::cout << "discord_api_connect: trying endpoint " << e.address().to_string() << std::endl;
					return true;
				},
				MEM_HAND(on_connected)
			);
		}
		void on_connected(
			const sys::error_code &ec,
			const ip::tcp::endpoint &endpoint
		){
			if(ec){
				this->complete_now(ec);
				return;
			}
		//	stream.async_handshake(S::client, MEM_HAND(complete_now));
			stream.async_handshake(S::client, MEM_HAND(on_handshaked));
		}
		void on_handshaked(
			const sys::error_code &ec
		){
			this->complete_now(ec);
		}
	};
	completion_type init(hand);
	std::make_shared<op>(stream, init.completion_handler)->start();
	return init.result.get();
}

