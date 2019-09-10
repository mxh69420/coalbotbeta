 #pragma once

//#include <boost/asio.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/beast/core/async_base.hpp>
//#include <boost/beast.hpp>
#include <string>
#include <memory>
#include "mem_hand.hpp"
#include "gateway_bot.hpp"

using namespace boost::asio;
using namespace boost::beast;
namespace sys = boost::system;

template <class S, class H>
auto discord_get_gateway_bot(S &stream, const std::string_view &auth, H &&hand){
	using signature = void(sys::error_code, json_types::gatewayBot);
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
		http::request<http::string_body> req;
		http::response<http::string_body> resp;
		flat_buffer buf;

		op(S &_stream, const std::string_view &auth, handler_type &hand) :
			stream(_stream),
			base_type(std::move(hand), _stream.get_executor()),
			req(http::verb::get, "/api/v6/gateway/bot", 11){

			req.set(http::field::host, "discordapp.com");
			req.set(http::field::user_agent,
				"coal (no url yet, sorry)");
			req.set(http::field::authorization, auth);
		}

		void go(){
			http::async_write(stream, req, MEM_HAND(write_complete));
		}
		void write_complete(const sys::error_code &ec, size_t sz){
			if(ec){
				this->complete_now(ec, json_types::gatewayBot());
				return;
			}
			http::async_read(stream, buf, resp,
				MEM_HAND(read_complete));
		}
		void read_complete(const sys::error_code &ec, size_t sz){
			if(ec){
				this->complete_now(ec, json_types::gatewayBot());
				return;
			}
			json_types::gatewayBot res =
				nlohmann::json::parse(resp.body());
			this->complete_now(
				sys::error_code(),
				std::move(res)
			);
		}
	};
	completion_type init(hand);
	std::make_shared<op>(stream, auth, init.completion_handler)->go();
	return init.result.get();
}
