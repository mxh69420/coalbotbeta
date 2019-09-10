/* main.cpp
 *
 * NOTE BEFORE LINKING: this software comes with no warranty. i am not responsible for any damages, including
 * but not limited to rate limit managers falling apart and causing tokens to get revoked or banned, or a really
 * bad buffer overflow that causes your computer to undergo nuclear fission and destroy the city. by compiling
 * and linking, you agree to these terms.
 * speaking of which: i do still get 429s every once in a while with this implementation, so be careful and dont
 * use this implementation with bots that already have a presence on servers. and definitely dont use your actual
 * account. wait until i come up with a new implementation for that
 *
 * compile with g++ -O3 -c -std=c++17 main.cpp
*/


#include <iostream>
#include <string>
#include <random>
#include <unordered_map>
#include <list>

#include <stdlib.h>
#include <sys/random.h>

#define BOOST_ASIO_SEPARATE_COMPILATION
#define BOOST_BEAST_SEPARATE_COMPILATION

//uncomment these if something breaks with the minimized headers
//#include <boost/asio.hpp>
//#include <boost/beast.hpp>
/*yeah i use some features from asio and beast that are only included
	by my headers. i know this is bad but i at least got it working with
	minimized headers, so i can compile much faster.*/

#include <boost/asio/io_context.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/connect.hpp>
//#include <boost/asio/spawn.hpp> //stupid
#include <boost/beast/core/async_base.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/span_body.hpp>
#include <boost/beast/websocket/stream.hpp>
#include <boost/beast/websocket/ssl.hpp>

#include "coal_bind_front.hpp"
#define MEM_HAND_BIND_FRONT coal::detail::bind_front
#include "mem_hand.hpp"

#include "get_gateway.hpp"
#include "api_connect.hpp"
#include "payload_in.hpp"
#include "hand_attach.hpp"

//i decided to reuse some old code i wrote a long time ago
#include "events/gateway_events.cpp"

extern template class nlohmann::basic_json<>;

//ill be using these to keep track of how much time compiling takes
#pragma message("end of headers")

using namespace boost::asio;
using namespace boost::beast;
namespace sys = boost::system;
namespace ws = websocket;

namespace pt = boost::posix_time;

#define DUMP(x) (std::cout << #x ": " << x << '\n')

std::ostream &operator<<(std::ostream &to, const sys::error_code &ec){
	to << '(' << ec.category().name() << ") " << ec.message();
	return to;
}

/*unsigned long kb_time(double lo, double hi){
	hi -= lo;
	double i_max = pow(hi/2.0, 1.0/3.0);
	std::uniform_real_distribution<double> unif(i_max * -1, i_max);

	static std::default_random_engine re;
	static bool seeded = false;

	if(!seeded){
;		std::default_random_engine::result_type seed;
		getrandom(&seed, sizeof(seed), 0);
		seeded = true;
	}

	double r_val = unif(re);
	r_val + i_max;
	DUMP(r_val);

	double ret = pow(r_val, 3.0);
	DUMP(ret);

	ret += hi/2.0;
	ret -= lo;
	std::cout << "kb_time: returning " << ret << " seconds" << std::endl;
	return ret * 1000;
}*/ //moved to kb_rand.cpp so i can tinker without recompiling the whole thing

bool randbool(){
	static uint64_t buf;
	static uint8_t left = 0;
	if(left == 0){
		getrandom(&buf, 8, 0);
		left = 64;
	}
	bool ret = buf & 1;
	--left;
	buf >>= 1;
	return ret;
}

uint8_t mksel(uint8_t bits){
	uint8_t ret = 0;
	for(uint8_t i = 0; i < bits; ++i)
		ret |= (randbool() ? 1 : 0) << i;
	return ret;
}

unsigned long kb_rand(double, double);

std::string bsv_c(const boost::beast::string_view &v){
	return std::string(v.data(), v.size());
}

/*bool donut_eater(const std::string_view &str){
	std::string lstr;
	lstr.reserve(str.length());
	for(auto it = str.begin(); it != str.end(); ++it){
		if(*it >= 'A' && *it <= 'Z') lstr.push_back(*it - 'A' + 'a');
		else if(*it >= 'a' && *it <= 'z') lstr.push_back(*it);
	}
	return lstr == "doyouknowwhoateallthedonuts";
}*/ //also now in kb_rand.cpp
bool donut_eater(const std::string_view &str);

template <class Sv = std::string_view>
struct basic_strtoker {
	using sv_t = Sv;
	basic_strtoker(sv_t &&_str) : str(std::forward<sv_t>(_str)), pos(0){
		/* usage:
			strtoker my_tok(my_string); //my_tok is now loaded with my_string
			for(;;){
				auto next = my_tok(chars); //next now contains the next thing strtok wouldve returned
				if(next.empty()) break; //no remaining tokens
				//do something with next
			}
		*/
	}

	//steal state, load with different string type (but should be same string value)
	template <class oSv>
	basic_strtoker(const basic_strtoker<oSv> &state, sv_t &&_str)
		: str(std::forward<sv_t>(_str)), pos(state.pos){

		/* usage:
		 *	strtoker mytok(my_string); //load with std::string converted to string_view
			//crap, from this point i decided i want to store the regular std::string
			basic_strtoker<std::string> mynewtok(std::move(my_string));
			//then when done, you can call release() to release it
		*/
	}

	sv_t str;
	size_t pos;

	sv_t operator()(const sv_t &chars){
		if(pos == sv_t::npos) return sv_t(); //already hit end of input

		size_t fpos = str.find_first_not_of(chars.data(), pos, chars.size());
		if(fpos == sv_t::npos) return sv_t();
		size_t lpos = str.find_first_of(chars.data(), fpos + 1, chars.size());
		if(lpos == sv_t::npos){
			pos = sv_t::npos;
			return sv_t(str.data() + fpos, str.size() - fpos);
		}
		pos = lpos + 1;
		return sv_t(str.data() + fpos, lpos - fpos);
	}

	sv_t release(){ //state remains the same, but str remains in unspecified state
		//dont make any more calls to your strtoker, only use it to steal state from here
		return std::move(str);
	}
};

using strtoker = basic_strtoker<std::string_view>;

#pragma message("begin main_op")
template <class Executor>
struct main_op : public std::enable_shared_from_this<main_op<Executor>> {
	ssl::stream<ip::tcp::socket> sock;
	ws::stream<ssl::stream<ip::tcp::socket>> gateway_sock;
	ip::tcp::resolver resolve;

	Executor &exec;
	deadline_timer heartbeat_timer;

	multi_buffer ws_in;

	json_types::gatewayBot gateway;

	std::string session_id;

	unsigned int heartbeat_interval;
	uint64_t user_id;
	unsigned long last_sequence;
	bool had_sequence;
	bool last_hb_acked;

	std::string_view authtok;

	main_op(Executor &_e, std::string_view _authtok) :
		sock(_e, get_discordapp_context()),
		gateway_sock(_e, get_discordapp_context()),
		resolve(_e),
		heartbeat_timer(_e),
		exec(_e),
		authtok(_authtok),
		limits_shrinker(_e),
		role_change_timer(_e){

		limits.max_load_factor(4.0);
	}
	void go(){
		discord_api_connect(sock, MEM_HAND(on_connect));
		limits_shrink_arm();
	}
	void on_connect(sys::error_code ec){
		if(ec){
			std::cerr << "failed to connect to discord api: "
				<< ec << std::endl;
			return;
		}
		discord_get_gateway_bot(
			sock,
			authtok,
			MEM_HAND(on_bot)
		);
	}
	void on_bot(sys::error_code ec, json_types::gatewayBot _gateway){
		if(ec){
			std::cerr << "failed to get gateway: " << ec
				<< std::endl;
			return;
		}

		if(_gateway.session_start_limit.remaining == 0){
			std::cerr << "no more session starts available\n";
			std::exit(1);
		}

		gateway = std::move(_gateway);

		std::cout << nlohmann::json(gateway).dump(1, '\t') << std::endl;

		std::string host(gateway.url.substr(6));

		gateway_sock.next_layer().set_verify_callback(
			ssl::rfc2818_verification(host));

		std::cout << "resolving gateway host " << host << std::endl;
		std::string_view hsv(host); //make string_view now, instead of later, because gcc likes to screw things up :(
		resolve.async_resolve(hsv, "https", MEM_HAND(on_gw_resolved,
			std::move(host)));
	}
	void on_gw_resolved(std::string &host,
		sys::error_code ec, ip::tcp::resolver::results_type endpoints){

		if(ec){
			std::cerr << "failed to resolve host: " << ec
				<< std::endl;
			return;
		}

		std::string_view hsv(host);

		async_connect(
			get_lowest_layer(gateway_sock),
			endpoints,
			[hsv](sys::error_code ec, const ip::tcp::endpoint &e){
				if(ec) std::cout << ec << '\n';
				std::cout << "trying " << hsv << " -> " << e.address().to_string() << std::endl;
				return true;
			},
			MEM_HAND(on_gw_connected, std::move(host))
		);
	}
	void on_gw_connected(std::string &host,
		sys::error_code ec, ip::tcp::endpoint chosen){

		if(ec){
			std::cerr << "failed to connect to host: " << ec
				<< std::endl;
			return;
		}

		gateway_sock.next_layer().async_handshake(
			ssl::stream_base::client,
			MEM_HAND(on_gw_ssl_hs, std::move(host))
		);
	}
	void on_gw_ssl_hs(std::string &host, sys::error_code ec){
		if(ec){
			std::cerr << "secure connection to host failed: "
				<< ec << std::endl;
			return;
		}
		boost::beast::string_view hsv(host);
		gateway_sock.async_handshake(hsv, "/", hand_attach(
			MEM_HAND(on_gw_hs), std::move(host)
		));
	}
	void on_gw_hs(sys::error_code ec){
		if(ec){
			std::cerr << "failed to initialize websocket to gateway: "
				<< ec << std::endl;
			return;
		}

		had_sequence = false;
		last_hb_acked = true;

		gateway_sock.async_read(
			ws_in,
			MEM_HAND(on_gw_message)
		);

		if(session_id.empty()) identify();
		else resume();
	}
	void on_gw_message(sys::error_code ec, size_t bread){
		if(ec){
			std::cerr << "message reader returned error: "
				<< ec
				<< "\n\tclose code: " << gateway_sock.reason()
				.code << "\n\tclose reason: "
				<< gateway_sock.reason().reason << std::endl;
//			shutdown();
			if(gateway_sock.reason().code == 1001){
				std::cout << "relaunching" << std::endl;
				raise(SIGUSR1);
			}
			return;
		}
/*		auto front = buffers_front(ws_in.data());
		std::string_view str((const char *) front.data(), front.size());*/
		std::string str(buffers_to_string(ws_in.data()));
		json_types::payload_in msg = nlohmann::json::parse(str);
		ws_in.clear();

		bool should_display = on_msg(msg);
		if(should_display) std::cout << "incoming message: "
			<< nlohmann::json(msg).dump(1, '\t') << std::endl;


		gateway_sock.async_read(
			ws_in,
			MEM_HAND(on_gw_message)
		);
	}

	//return true if payload should be displayed
	bool on_msg(json_types::payload_in &msg){
		switch(msg.op){
		case 10:
			on_hello(msg);
		return false;
		case 11:
			last_hb_acked = true;
		return false;
		case 1:
			send_heartbeat_ack();
		return false;
		case 0:
			return do_event(msg);
		case 9:
			on_invalid_session(msg);
		default:
			return true;
		}
	}

	void on_invalid_session(json_types::payload_in &msg){
		raise(msg.d.get<bool>() ? SIGUSR1 : SIGUSR2);
//		std::cerr << "raise did nothing\n";
//		std::exit(1);
	}

	void on_hello(json_types::payload_in &msg){
		heartbeat_interval =
			msg.d.at("heartbeat_interval").get<unsigned int>();
		heartbeat_timer.cancel();
		heartbeat_timer.expires_from_now(
			pt::milliseconds(heartbeat_interval)
		);
		heartbeat_timer.async_wait(MEM_HAND(run_heartbeat));
	}

	void run_heartbeat(sys::error_code ec){
		if(ec == boost::asio::error::operation_aborted) return;

		if(!last_hb_acked){
			std::cerr << "heartbeat ACK timed out, shutting down"
				<< std::endl;
			shutdown();
			return;
		}
		last_hb_acked = false;

		/*nlohmann::json j;
		//j["d"] = had_sequence ? last_sequence : nullptr;
		j["op"] = 1;
		if(had_sequence)
			j["d"] = last_sequence;
		j["d"] = nullptr;*/
		gateway_sock.text(true);

		json_types::payload_in pl;
		pl.op = 1;
		if(had_sequence)
			pl.d = last_sequence;
		else pl.d = nullptr;

		nlohmann::json j(pl);

	/*	so uhhh when i dont pretty print this payload and the "d" field
		is non null it boots me?
		and now it seems to be working if i add this extra field
	*/
		j["_"] = 0;

		std::string str(j.dump());
/*		std::stringstream strs;
		strs << "{\"op\":1,\"d\":";
		if(had_sequence) strs << last_sequence;
		else strs << "null";
		strs << '}';
		std::string str = strs.str();*/
		auto hbbuf = std::make_unique<const_buffer>(str.data(),
			str.length());

//		std::cout << "sending heartbeat: " << str << std::endl;

//		gateway_sock.async_write(*hbbuf, MEM_HAND(heartbeat_written,
//			std::move(str), std::move(hbbuf)));
/*		gateway_sock.async_write(*hbbuf, [self = this->shared_from_this(),
			str = std::move(str), buf = std::move(hbbuf)]
			(sys::error_code ec, size_t bwrite){

			self->heartbeat_written(ec, bwrite);
		});*/
		auto &h = *hbbuf;
		gateway_sock.async_write(h, hand_attach(
			MEM_HAND(heartbeat_written),
			std::move(hbbuf),
			std::move(str)
		));

		heartbeat_timer.expires_at(heartbeat_timer.expires_at()
			+ pt::milliseconds(heartbeat_interval));
		heartbeat_timer.async_wait(MEM_HAND(run_heartbeat));

	}
	void heartbeat_written(sys::error_code ec, size_t bwrite){
		if(ec){
			std::cerr << "failed to write heartbeat or heartbeat ACK: "
				<< ec << std::endl;
			std::cout << "shutting down" << std::endl;
			shutdown();
			return;
		}
	}

	void send_heartbeat_ack(){
		static const char *pl = "{\"op\":11}";
		static const const_buffer plbuf(pl, 9);
		gateway_sock.async_write(plbuf, MEM_HAND(heartbeat_written));
	}

	void identify(){
/*		nlohmann::json j;
		j["op"] = 2;*/

		json_types::payload_in pl;
		pl.op = 2;
		pl.d = nlohmann::json::object({
			{"token", authtok},
			{"properties",
				{
					{"$os", "linux"},
					{"$browser", "coal"},
					{"$device", "coal"}
				}
			},
			{"shard",
				nlohmann::json::array({
					0,
					gateway.shards
				})
			},
			{"presence",
				{
					{"since", nullptr},
					{"game",
						{
							{"name",
								"timer fixing "
								"simulator"},
							{"type", 0}
						}
					},
					{"status", "*((int *) 0 = 0;"},
					{"afk", false}
				}
			}
		});

		nlohmann::json j(pl);

		std::string str(j.dump());
		auto buf = std::make_unique<const_buffer>(str.data(),
			str.length());

//		auto dn = std::make_unique<dsnotify>();
//		std::cout << "now sending identify payload: " << j.dump(1, '\t')
//			<< std::endl;

/*		gateway_sock.async_write(*buf, [self = this->shared_from_this(),
			str = std::move(str), buf = std::move(buf)]
			(sys::error_code ec, size_t bwrite){

			if(ec){
				std::cerr << "failed to write identify payload"
					<< std::endl;
				self->shutdown();
			}
		});*/
/*		gateway_sock.async_write(*buf, hand_attach(
			MEM_HAND(identify_sent),
//			std::move(str),
//			std::move(buf),
			std::move(dn)
		));*/
//		assert(buf);
//		assert(str->data());
/*		gateway_sock.async_write(*buf, MEM_HAND(identify_sent));
		buf.release();*/

		auto &b = *buf; /*youll see this across each of these
			files, it turns out the reason it didnt work with
			gcc is because on gcc the std::move(buf) is being
			evaluated before the *buf, while on clang it
			would get the reference returned by operator*
			before making the move. i changed it to evaluate
			the reference before the move instead.*/
		gateway_sock.async_write(b, hand_attach(
			MEM_HAND(identify_sent),
			std::move(str),
			std::move(buf)
		));
	}

	void identify_sent(sys::error_code ec, size_t bwrite){
//		std::cout << "safe for object to be destroyed" << std::endl;
		if(ec){
			std::cerr << "failed to write identify/resume payload"
				<< std::endl;
			shutdown();
		}
	}

	void resume(){
		json_types::payload_in pl;
		pl.op = 6; //resume
		pl.d = {
			{"token", authtok},
			{"session_id", session_id},
			{"seq", last_sequence}
		};
		nlohmann::json j(pl);
		std::string str(j.dump());
		auto buf = std::make_unique<const_buffer>(str.data(),
			str.length());

		std::cout << "sending resume payload: " << j.dump(1, '\t')
			<< std::endl;

		auto &b = *buf;

		gateway_sock.async_write(b, hand_attach(
			MEM_HAND(identify_sent),
			std::move(str),
			std::move(buf)
		));
	}

	bool do_event(json_types::payload_in &msg){
		had_sequence = true;
		last_sequence = msg.s;

		auto code = events::strToCode(msg.t.c_str());
		switch(code){
		case events::MESSAGE_CREATE:
			return on_message(msg);
		case events::READY:
			return on_ready(msg);
		case events::RESUMED:
			change_role_name(true);
			return true;
		case events::INVALID:
			std::cerr << "unknown event name " << msg.t
				<< std::endl;
		default:
			return false;
		}
	}

	bool on_ready(json_types::payload_in &msg){
		msg.d["session_id"].get_to(session_id);
		user_id = std::stoul(msg.d["user"]["id"].get<std::string>());
		change_role_name(true);
		return true;
	}

	bool on_message(json_types::payload_in &msg){
		std::string content = msg.d["content"].get<std::string>();

		std::cout << "message from " <<
			msg.d["author"]["username"].get<std::string>() << '#'
			<< msg.d["author"]["discriminator"].get<std::string>()
			<< ": " << content << '\n';

		uint64_t channel_id = std::stoul(msg.d["channel_id"].get<std::string>());
		if(std::stoul(msg.d["author"]["id"].get<std::string>())
			== user_id) return false; //no loopin today pal
		if(channel_id == 616526283316264970){
			if(!donut_eater(content))
				type_message_j(nlohmann::json({{"content", "`//TODO: remove all messages in channel id 616526283316264970 that dont "
				"say \"do you know who ate all the donuts\"`\n`//TODO: implement delete message api wrapper`"}}), channel_id, detached);
		} else if(content.find("c_poem") == 0){
			std::cout << "do the potic\n";
			using namespace std::literals;
#define MAKE_POME(x) "{\"content\":\"hey look guyse i can the pomes\\n```" x "```\"}"sv

//			type_message("{\"content\":\"hey look guyse i can the poeme\\n```roses are red\\ni have to use the bathroom```\"}",
//				channel_id, detached);

			type_message(std::string(randbool() ?
				MAKE_POME("roses are red\\ni have to use the bathroom")
				: MAKE_POME("ooh pome time i can do the potic\\nwatch this\\n***rimes****")), channel_id, detached);
#undef MAKE_POME
		} else if(content.find("c_feed ") == 0){
			using namespace std::literals;
			strtoker mytoker(std::string_view(content.data() + 7, content.size() - 7));

			constexpr const std::string_view chars(" \r\n;,*`_~"sv);
			std::string_view cur = mytoker(chars);
			if(cur.empty()){
				type_message("{\"content\":\"i detect a little communism\"}"s, channel_id, detached);
				return false;
			}
			std::ostringstream outss;
			outss << (randbool() ? "thamks"sv : "thank"sv)
				<< " *eats the foonds: "sv;
			bool had_the = false;
			for(;;){
				std::string_view next = mytoker(chars);
				if(next.empty()){
					if(cur == "vegetal"){
						type_message("{\"content\":\"i taste a vegetal\"}"s, channel_id, [&, channel_id](sys::error_code ec){
							if(ec) return;
							type_message("{\"content\":\"***A N G E R Y !!!!!!!***\"}"s, channel_id, detached);
						});
						return false;
					}
					if(cur.empty()){
						type_message("{\"content\":\"bruh moment (pls report to the worst developer in the world)\"}"s,
							channel_id, detached);
						std::cerr << "nonfatal bruh moment: next.empty() && cur.empty() in c_feed" << std::endl;
						return false;
					}
					if(had_the) outss << "and the "sv;
					outss << cur << '*';
					break;
				}
				std::cout << "found foond " << next << '\n';
				if(!cur.empty()){
					had_the = true;
					outss << cur << ", "sv;
				}
				cur = next;
			}
			type_message_j(nlohmann::json({{"content", outss.str()}}), channel_id, detached);
		} else if(content.find("c_") == 0 && content.length() > 2){
			std::cout << "this guy speede\n";

/*			spawn(heartbeat_timer.get_executor(), MEM_HAND(
				trig_coro_wrapper,
				std::stoul(msg.d["channel_id"].get<std::string>())
			));*/

			std::stringstream content;

			const char *noun = randbool() ? (randbool() ? "pal" : "man")
				: (randbool() ? "bud" : "sir");

			if(randbool() || randbool() || randbool() || randbool()){
				content << "hold on " << noun << " wait a second your"
					"doing it to fast";
			} else {
				content << "you fool you blongus you absolute utter "
					"clampongus your are doing the fast do the "
					"slower pls " << noun << " i cant do the"
					" velocety";
			}

			type_message_j(nlohmann::json({{"content", content.str()}}), channel_id, detached);
/*		} else if(content == "coal fix youre timers"){
			std::cout << "oh no" << std::endl;
			type_message("{\"content\":\"oh no\"}", channel_id, detached);
		}*/
		} else switch(hash(content.c_str())){
		case hash("coal fix youre timers"):
			std::cout << "oh no\n";
			type_message("{\"content\":\"oh no\"}", channel_id, detached);
		break;
		case hash("thanks coal"):
		case hash("thamks coal"):
			std::cout << "your welcome\n";
			type_message("{\"content\":\"your welcome :)\"}", channel_id, detached);
		break;
		case hash("tocch"):
			std::cout << "think about the consequences of youre actions\n";
			type_message("{\"content\":\"you fool, you blongus, you absolute utter clampongus, think about the consequences of your actions\"}",
				channel_id, detached);
		}

		std::cout.flush();
		return false;
	}

/*	void trig_coro_wrapper(uint64_t channel_id, yield_context yield){
		try {
			trig_coro(channel_id, yield);
		} catch(boost::exception &e){
			std::cerr << "oopsy from trig_coro: "
				<< boost::diagnostic_information(e)
				<< std::endl;
		} catch(std::exception &e){
			std::cerr << "oopsy from trig_coro: "
				<< boost::diagnostic_information(e)
				<< std::endl;
		}
	}*/

	//holy crap, coal bot features timers????????
	template <class E>
	struct rate_limit;

	typedef std::unordered_map<uint64_t, std::weak_ptr<rate_limit<Executor>>> limits_map_t;

	template <class E>
	struct rate_limit : public std::enable_shared_from_this<rate_limit<E>>{
		uint32_t t_inflight;
		uint32_t t_remaining;
		uint64_t t_reset;

		uint32_t c_inflight;
		uint32_t c_remaining;
		uint64_t c_reset;

		rate_limit(E &e, limits_map_t &_owner) :
			t_remaining(-1),
			t_reset(0),
			t_inflight(0),
			c_remaining(-1),
			c_reset(0),
			c_inflight(0),

			it(_owner.end()),
			owner(_owner),

			t_resets(e),
			c_resets(e){

			t_resets.expires_from_now(pt::seconds(0));
			c_resets.expires_from_now(pt::seconds(0));

		}

		limits_map_t &owner;
		typename limits_map_t::iterator it;

		deadline_timer t_resets;
		deadline_timer c_resets;

		~rate_limit(){
			if(it != owner.end()){
				std::cout << "destroying rate_limit object for channel id " << it->first << std::endl;
				owner.erase(it);
			}
		}

		void t_set_reset(uint64_t time){
			if(time < t_reset) return;
			t_reset = time;
			t_resets.expires_at(
				pt::ptime(
					boost::gregorian::date(1970, 1, 1),
					pt::seconds(time)
				)
			);
			t_resets.async_wait(MEM_HAND(on_reset, &t_reset));
		}

		void c_set_reset(uint64_t time){
			if(time < c_reset) return;
			c_reset = time;
			c_resets.expires_at(
				pt::ptime(
					boost::gregorian::date(1970, 1, 1),
					pt::seconds(time)
				)
			);
			c_resets.async_wait(MEM_HAND(on_reset, &c_reset));
		}

		void on_reset(uint64_t *rsval, sys::error_code ec){
			if(ec != boost::asio::error::operation_aborted)
				*rsval = 0;
		}
	};

	limits_map_t limits;


	//IT EVEN KEEPS THE HASH TABLE SMALL!!!
	deadline_timer limits_shrinker;
	void limits_shrink(bool set_timer = false, sys::error_code ec = {}){
		if(ec) return;
		//equivalent to shrink_to_fit members on other containers:
		limits.rehash(0);
		if(set_timer) limits_shrink_arm();
	}
	void limits_shrink_arm(){
		limits_shrinker.expires_from_now(pt::seconds(60));
		limits_shrinker.async_wait(
//			MEM_HAND(limits_shrink, true)
			MEM_HAND_NO_REF(limits_shrink, true)
		);
	}


	std::shared_ptr<rate_limit<Executor>> find_limit(uint64_t channel_id){
		auto nel = std::make_shared<rate_limit<Executor>>(exec, limits);
		auto e = limits.emplace(channel_id, nel);
		if(e.second){
			std::cout << "creating rate_limit object for channel id " << channel_id << std::endl;
			nel->it = e.first;
			return nel;
		}
		if(e.first->second.expired()){
			std::cerr << "warning: rate_limit for channel id "
				<< channel_id << " left expired" << std::endl;
			e.first->second = nel;
			nel->it = e.first;
			return nel;
		}
		return e.first->second.lock();
	}

/*	void trig_coro(uint64_t channel_id, yield_context &yield){
		auto self = this->shared_from_this(); //hold a reference
		sys::error_code ec;

		deadline_timer t(
			heartbeat_timer.get_executor(),
			pt::milliseconds(kb_rand(0, 1.1))
		);

		//too lazy to coordinate the streams myself
		ssl::stream<ip::tcp::socket> hsock(
			heartbeat_timer.get_executor(),
			get_discordapp_context()
		);

		auto results = resolve.async_resolve(
			"discordapp.com",
			"https",
			yield[ec]
		);
		if(ec){
			std::cerr << "async_resolve: " << ec
				<< std::endl;
			return;
		}

		async_connect(hsock.lowest_layer(), results, yield[ec]);
		if(ec){
			std::cerr << "async_connect: " << ec
				<< std::endl;
			return;
		}

		hsock.async_handshake(ssl::stream_base::client, yield[ec]);
		if(ec){
			std::cerr << "async_handshake: " << ec
				<< std::endl;
			return;
		}

		//if it connected faster wait for it
		t.async_wait(yield[ec]);

		auto my_lim = find_limit(channel_id);

		std::stringstream uri_s;
		uri_s << "/api/v6/channels/" << channel_id;
		std::string uri_typing = uri_s.str() + "/typing",
			uri_messages = uri_s.str() + "/messages";

		http::request<http::string_body> req(
			http::verb::post,
			uri_typing,
			11
		);
		req.set(http::field::host, "discordapp.com");
		req.set(http::field::authorization, authtok);
		req.set(http::field::content_type, "application/json");
//		req.set("X-RateLimit-Precision", "millisecond");
		req.prepare_payload();

		if(my_lim->t_remaining == 0) my_lim->t_resets.async_wait(yield[ec]);

		http::async_write(hsock, req, yield[ec]);
		if(ec){
			std::cerr << "warning: failed to request trigger "
				"typing: " << ec << std::endl;
			return;
		}

		flat_buffer buf;
		http::response<http::string_body> resp;
		http::async_read(hsock, buf, resp, yield[ec]);
		if(ec){
			std::cerr << "warning: failed to request trigger "
				"typing: " << ec << std::endl;
			return;
		}

		my_lim->t_remaining = std::min((uint32_t) std::stoul(bsv_c(resp.find("X-RateLimit-Remaining")->value())), my_lim->t_remaining);
		my_lim->t_set_reset(std::stoul(bsv_c(resp.find("X-RateLimit-Reset")->value())));

		std::cout << "set typing: " << resp << std::endl;

		t.expires_from_now(
			boost::posix_time::milliseconds(kb_rand(1, 4))
		);
		t.async_wait(yield[ec]);

		req.target(uri_messages);

		std::stringstream content;

		const char *noun = randbool() ? (randbool() ? "pal" : "man")
				: (randbool() ? "bud" : "sir");


		if(randbool() || randbool() || randbool() || randbool()){
			content << "hold on " << noun << " wait a second your"
				"doing it to fast";
		} else {
			content << "you fool you blongus you absolute utter "
				"clampongus your are doing the fast do the "
				"slower pls " << noun << " i cant do the"
				" velocety";
		}

		req.body() = nlohmann::json({
			{"content", content.str()}
		}).dump();
		req.prepare_payload();
		if(my_lim->c_remaining == 0) my_lim->c_resets.async_wait(yield[ec]);
		http::async_write(hsock, req, yield[ec]);
		if(ec){
			std::cerr << "async_write: " << ec
				<< std::endl;
			return;
		}

		http::async_read(hsock, buf, resp, yield[ec]);
		if(ec){
			std::cerr << "async_read: " << ec
				<< std::endl;
			return;
		}

		my_lim->c_remaining = std::min((uint32_t) std::stoul(bsv_c(resp.find("X-RateLimit-Remaining")->value())), my_lim->c_remaining);
		my_lim->c_set_reset(std::stoul(bsv_c(resp.find("X-RateLimit-Reset")->value())));

		resp.body() = nlohmann::json::parse(resp.body()).dump(1, '\t');
		std::cout << "wrote message: " << resp << std::endl;

		t.expires_from_now(boost::posix_time::seconds(10));
		t.async_wait([&](sys::error_code ec){
			if(ec) return;
			hsock.lowest_layer().cancel();
		});
		hsock.async_shutdown(yield[ec]);
	}*/

	template <class H>
	auto type_message_j(const nlohmann::json &message, uint64_t channel_id, H &&hand){
		return type_message(message.dump(), channel_id, std::forward<H>(hand));
	}

	template <class H>
	auto type_message(std::string message, uint64_t channel_id, H &&hand){
		using signature = void(sys::error_code);
		using completion_type = async_completion<
			H,
			signature
		>;
		using handler_type =
			typename completion_type::completion_handler_type;
		using base_type = async_base<
			handler_type,
			typename Executor::executor_type
		>;
		struct op :
			public base_type,
			public std::enable_shared_from_this<op> {

			std::shared_ptr<main_op<Executor>> parent;
			ssl::stream<ip::tcp::socket> sock;
			std::string msg;
			deadline_timer t;
			http::request<http::string_body> req;
			http::response<http::string_body> resp;
			std::shared_ptr<rate_limit<Executor>> my_lim;
			flat_buffer buf;

			std::string uri_message;

			op(std::shared_ptr<main_op<Executor>> _parent, std::string _msg, uint64_t channel_id, handler_type &hand) :
				parent(_parent),
				msg(std::move(_msg)),
				base_type(std::move(hand),
					_parent->exec.get_executor()),
				t(_parent->exec),
				my_lim(_parent->find_limit(channel_id)),
				sock(_parent->exec, get_discordapp_context()){

				std::stringstream uri_s;
				uri_s << "/api/v6/channels/" << channel_id;

				req.method(http::verb::post);
				req.version(11);
				req.target(uri_s.str() + "/typing");
				uri_message = uri_s.str() + "/messages";

				req.set(http::field::host, "discordapp.com");
				req.set(http::field::authorization, parent->authtok);
				req.set(http::field::content_type, "application/json");
//				req.set("X-RateLimit-Precision", "millisecond");
				req.prepare_payload();
			}
			void go(){
				t.expires_from_now(pt::milliseconds(kb_rand(0, 1.1)));

				discord_api_connect(sock, MEM_HAND(on_connect));
			}

			void on_connect(sys::error_code ec){
				if(ec){
					std::cerr << "type_message: warning: failed to connect: "
						<< ec << std::endl;
					this->complete_now(ec);
					return;
				}
				t.async_wait(MEM_HAND(on_timer_expire, 0));
			}
/*				parent->resolve.async_resolve(
					"discordapp.com",
					"https",
					MEM_HAND(on_resolved)
				);
			}
			void on_resolved(sys::error_code ec, const ip::tcp::resolver::results_type &results){
				if(ec){
					this->complete_now(ec);
					return;
				}
				parent.reset();
				async_connect(sock.lowest_layer(), results, MEM_HAND(on_connected));
			}
			void on_connected(sys::error_code ec, const ip::tcp::endpoint &endpoint){
				if(ec){
					this->complete_now(ec);
					return;
				}
				sock.async_handshake(ssl::stream_base::client, MEM_HAND(on_handshaked));
			}
			void on_handshaked(sys::error_code ec){
				if(ec){
					this->complete_now(ec);
					return;
				}
				t.async_wait(MEM_HAND(on_timer_expire, 0));
			}*/
			void on_timer_expire(int state, sys::error_code ec){
				if(ec){
					this->complete_now(ec);
					return;
				}

				DUMP(state);
				DUMP(my_lim->t_inflight);
				DUMP(my_lim->c_inflight);
				DUMP(my_lim->t_remaining);
				DUMP(my_lim->c_remaining);
				DUMP(my_lim->t_reset);
				DUMP(my_lim->c_reset).flush();

				switch(state){
				case 0:
					if(my_lim->t_inflight - my_lim->t_remaining == 0 && my_lim->t_reset != 0) my_lim->t_resets.async_wait(MEM_HAND(on_timer_expire, 0));
					else send_typing_now();
				break;
				case 1:
					if(my_lim->c_inflight - my_lim->c_remaining == 0 && my_lim->c_reset != 0) my_lim->c_resets.async_wait(MEM_HAND(on_timer_expire, 1));
					else send_message_now();
				break;
				default:
					std::cerr << "bruh moment: invalid state passed to on_timer_expire (state: " << state << ')' << std::endl;
					std::abort();
					std::exit(1);
				}
			}
			void send_typing_now(sys::error_code ec = {}){
				if(ec){
					this->complete_now(ec);
					return;
				}
				++my_lim->t_inflight;
				http::async_write(sock, req, MEM_HAND(typing_sent));
			}
			void typing_sent(sys::error_code ec, size_t bwrite){
				if(ec){
					--my_lim->t_inflight;
					std::cerr << "warning: failed to request trigger typing: "
						<< ec << std::endl;
					this->complete_now(ec);
					return;
				}
				http::async_read(sock, buf, resp, MEM_HAND(typing_respond));
			}
			void typing_respond(sys::error_code ec, size_t bread){
				--my_lim->t_inflight;
				if(ec){
					std::cerr << "warning: failed to read trigger typing: "
						<< ec << std::endl;
					this->complete_now(ec);
					return;
				}
				t.expires_from_now(pt::milliseconds(kb_rand(1, 4)));
				std::cout << "triggered typing: " << resp << std::endl;

				my_lim->t_remaining = std::min((uint32_t) std::stoul(bsv_c(resp.find("X-RateLimit-Remaining")->value())), my_lim->t_remaining);
				my_lim->t_set_reset(std::stoul(bsv_c(resp.find("X-RateLimit-Reset")->value())));

				req.target(uri_message);
				req.body() = std::move(msg);
				req.prepare_payload();
				t.async_wait(MEM_HAND(on_timer_expire, 1));
			}
			void send_message_now(sys::error_code ec = {}){
				if(ec){
					this->complete_now(ec);
					return;
				}
				++my_lim->c_inflight;
				http::async_write(sock, req, MEM_HAND(message_sent));
			}
			void message_sent(sys::error_code ec, size_t bwrite){
				if(ec){
					--my_lim->c_inflight;
					std::cerr << "warning: failed to request send message: "
						<< ec << std::endl;
					this->complete_now(ec);
					return;
				}
				http::async_read(sock, buf, resp, MEM_HAND(message_respond));
			}
			void message_respond(sys::error_code ec, size_t bread){
				--my_lim->c_inflight;
				if(ec){
					std::cerr << "warning: failed to read send message: "
						<< ec << std::endl;
					this->complete_now(ec);
					return;
				}

				try { //pretty print if possible
					resp.body() = nlohmann::json::parse(resp.body()).dump(1, '\t');
				} catch(...){

				}
				std::cout << "message sent: " << resp << std::endl;

				my_lim->c_remaining = std::min((uint32_t) std::stoul(bsv_c(resp.find("X-RateLimit-Remaining")->value())), my_lim->c_remaining);
				my_lim->c_set_reset(std::stoul(bsv_c(resp.find("X-RateLimit-Reset")->value())));

				this->complete_now(ec);

				sock.async_shutdown(MEM_HAND(shutdown));
				t.expires_from_now(pt::seconds(5));
				t.async_wait([&](sys::error_code ec){
					sock.lowest_layer().cancel(ec);
					sock.lowest_layer().close(ec);
				});
			}
			void shutdown(sys::error_code ec){
				t.cancel(ec);
				sock.lowest_layer().close(ec);
			}
		};
		completion_type init(hand);
		std::make_shared<op>(
			this->shared_from_this(),
			std::move(message),
			channel_id,
			init.completion_handler
		)->go();
		return init.result.get();
	}

	uint8_t role_sel = 0xFF;
	void change_role_name(bool set_timer = false, sys::error_code ec = {}){
		if(ec) return;
		auto req = std::make_unique<
			http::request<
				http::span_body<const char>
			>
		>(
			http::verb::patch,
			"/api/v6/guilds/614637750326657044"
				"/roles/616129527118299157",
			11
		);
		req->set(http::field::host, "discordapp.com");
		req->set(http::field::authorization, authtok);
		req->set(http::field::content_type, "application/json");

		using namespace std::literals;
#define ROLE_NAME(x) "{\"name\":\"" x "\"}"sv
		static const std::string_view vals[] = {
			ROLE_NAME("dont let it overcharge"),
			ROLE_NAME("Segmentation fault (core dumped)"),
			ROLE_NAME("timer repair man"),
			ROLE_NAME("timer repair guy"),
			ROLE_NAME("fuel"),
			ROLE_NAME("the"),
			ROLE_NAME("free(): double free detected in tcache 2"),
			ROLE_NAME("im running out of role names")
		};
#undef ROLE_NAME
		uint8_t sel = mksel(3);
		if(sel == role_sel){ //take some load off of the discord servers
			if(set_timer){
				role_change_timer.expires_from_now(pt::seconds(30));
				role_change_timer.async_wait(
					MEM_HAND_NO_REF(change_role_name, true)
				);
			}
			return;
		}
		auto &choice = vals[mksel(3)];
		req->body() = span<const char>(choice.data(), choice.size());
		req->prepare_payload();

/*		http::async_write(sock, *req,
			MEM_HAND(role_change_written, set_timer,
			std::move(req)));*///I HATE TEMPLATE ERRORS
		auto &r = *req;
/*		http::async_write(sock, r, [
			self = this->shared_from_this(),
			set_timer,
			req = std::move(req)
		](sys::error_code ec, size_t bwrite){
			self->role_change_written(set_timer, ec, bwrite);
		});*/
		http::async_write(sock, r, hand_attach(
			MEM_HAND(role_change_written, set_timer),
			std::move(req)
		));
	}
	void role_change_written(
		bool set_timer,
/*		std::unique_ptr<
			http::request<
				http::span_body<const char>
			>
		> req,*/
		sys::error_code ec,
		size_t bwrite
	){

		if(set_timer){
			role_change_timer.expires_from_now(pt::seconds(30));
			role_change_timer.async_wait(
				MEM_HAND_NO_REF(change_role_name, true)
			);
		}

		if(ec){
			std::cerr << "warning: failed to write role change: "
				<< ec << std::endl;
			return;
		}

		auto resp = std::make_unique<http::response<http::string_body>>
			();
		auto buf = std::make_unique<flat_buffer>();
//		http::async_read(sock, *buf, *resp, MEM_HAND(role_change_read,
//			std::move(resp), std::move(buf)));
		auto &r = *resp;
		auto &b = *buf;
/*		http::async_read(sock, b, r, [
			self = this->shared_from_this(),
			resp = std::move(resp),
			buf = std::move(buf)
		](sys::error_code ec, size_t bread){
			self->role_change_read(resp.get(), ec, bread);
		});*/
		http::async_read(sock, b, r, hand_attach(
			MEM_HAND(
				role_change_read,
				std::move(resp)
			),
			std::move(buf)
		));
	}
	void role_change_read(
		std::unique_ptr<http::response<http::string_body>> &resp,
		sys::error_code ec,
		size_t bread
	){

		if(ec){
			std::cerr << "warning: failed to read role change: "
				<< ec << std::endl;
			return;
		}

		resp->body() = nlohmann::json::parse(resp->body())
			.dump(1, '\t');
		std::cout << "role changed: " << *resp << std::endl;
	}
	deadline_timer role_change_timer;

	void shutdown(){
		//probably not the best shutdown code
		sys::error_code ec;
		heartbeat_timer.cancel(ec);
		gateway_sock.next_layer().lowest_layer().close(ec);
		sock.lowest_layer().close(ec);
	}
};
#pragma message("end main_op")

template <class O>
void sigusr1(io_context &io, O o){
	std::cout << "reexec + resume requested" << std::endl;

	DUMP(o.get()).flush();
	o->shutdown();
	auto session_id = std::move(o->session_id);
	auto last_sequence = o->last_sequence;
	auto user_id = o->user_id;
	o = {};

	setenv("RESUMING", "true", 1);
	setenv("SESSION_ID", session_id.c_str(), 1);
	setenv("LAST_SEQUENCE", [&](){
		std::stringstream s;
		s << last_sequence;
		return s.str();
	}().c_str(), 1);
	setenv("USER_ID", [&](){
		std::stringstream s;
		s << user_id;
		return s.str();
	}().c_str(), 1);

	//intruder alert red spy in base
	unsetenv("LD_PRELOAD");

	//AUTH_TOK should still be set

	char buf[PATH_MAX + 1];
	char *exe = (char *) "./gconn"; //default value if error
	ssize_t len = readlink("/proc/self/exe", buf, PATH_MAX);
	if(len != -1){
		buf[len] = 0;
		exe = buf;
	}

	execl(exe, exe, NULL);
	std::cerr << "exec: " << std::strerror(errno) << std::endl;
	execl("./gconn", "./gconn", NULL);
	std::cerr << "exec: " << std::strerror(errno) << std::endl;
	std::terminate();
}

/*extern "C" void __gcov_flush();

void on_interrupt(int sn){
	__gcov_flush();
	exit(128 + sn);
}*/

template <class O>
void sigusr2(io_context &io, O o){
	std::cout << "plain reexec requested" << std::endl;
	o->shutdown();
	o = {};

	unsetenv("RESUMING");
	//yeah even i dont trust my own code
	unsetenv("LD_PRELOAD");

	char buf[PATH_MAX + 1];
	char *exe = (char *) "./gconn"; //default value if error
	ssize_t len = readlink("/proc/self/exe", buf, PATH_MAX);
	if(len != -1){
		buf[len] = 0;
		exe = buf;
	} else std::cerr << "readlink: " << std::strerror(errno)
		<< "\nusing default value ./gconn" << std::endl;

	execl(exe, exe, NULL);
	std::cerr << "exec: " << std::strerror(errno) << std::endl;
	execl("./gconn", "./gconn", NULL);
	std::cerr << "exec: " << std::strerror(errno) << std::endl;
	std::terminate();
}

template <class O>
void signal_handler(io_context &io, O o, sys::error_code ec, int signo) noexcept {
	if(ec){
		std::cerr << "signal handler error: " << ec
			<< std::endl;
		return;
	}
//	__gcov_flush();
	std::shared_ptr<typename O::element_type> t(o);
	switch(signo){
	case SIGUSR1:
		sigusr1(io, t);
	return;
	case SIGUSR2:
		sigusr2(io, t);
	return;
	}
}

/*using usock = local::stream_protocol;

const char schar = 0;
const const_buffer sbuf = buffer(&schar, 1);

class ctl_session : public std::enable_shared_from_this<ctl_session> {
	bufferred_read_stream<usock::socket> sock;
	mutable_buffer buf;
	std::shared_ptr<main_op<io_context>> op;
	ctl_session(
		usock::socket _sock,
		std::shared_ptr<main_op<io_context>> _op
	) :
		sock(std::move(_sock), 256), //256b already plenty
		op(std::move(_op)){

	}

	void go(){
		start_msg();
	}

	uint8_t code;
	void start_msg(){
		buf = buffer(&code, 1);
		async_read(sock, buf, MEM_HAND(on_code));
	}


	void on_code(sys::error_code ec, size_t bread){
		if(ec){
			std::cerr << "on_code: " << ec << std::endl;
			return;
		}

		using namespace std::literals;

		switch(code){
		case 0: //NOP / ping
			out_msg(sbuf, MEM_HAND(wcomplete));
			start_msg();
			return;
		case 1: //shutdown bot
			out_msg(sbuf, MEM_HAND(wcomplete));
			op->shutdown();
			start_msg();
			return;
		case 2: //get session id
			get_session_id();
			start_msg();
			return;
		case 3: //get last sequence
			get_last_sequence();
			start_msg();
			return;
		case 4: //reload
			out_msg(sbuf, MEM_HAND(reload_now));
			return;
		}
	}

	void reload_now(sys::error_code ec, size_t bwrite){
		raise(SIGUSR1);
	}

	void get_session_id(){
		uint64_t l = op->session_id.size();
		uint64_t length = 1 + 8 + l;
		auto resp = std::unique_ptr<char[]>(new char[length]);
		resp[0] = 0;
		std::memcpy(resp.get() + 1, &l, 8);
		std::memcpy(resp.get() + 9, op->session_id.data(), l);
		auto buf = buffer(resp.get(), length);
		out_msg(buf, hand_attach(MEM_HAND(wcomplete),
			std::move(resp)));
	}
	void get_last_sequence(){
		if(op->has_sequence){
			constexpr const uint64_t length = 1 + 8;
			auto resp = std::unique_ptr<char[]>(new char[length]);
			resp[0] = 0;
			std::memcpy(resp.get() + 1, &op->last_sequence, 8);
			auto buf = buffer(resp.get(), length);
			out_msg(buf, hand_attach(MEM_HAND(wcomplete),
				std::move(resp)));
		} else {
			static const char c = 1;
			static const const_buffer buf = buffer(&c, 1);
			out_msg(buf, MEM_HAND(wcomplete));
		}
	}

	template <class H>
	void out_msg(std::string msg, H &&hand){
		auto buf = buffer(msg.data(), msg.size());
		out_msg(
			buf,
			hand_attach(
				std::forward<H>(hand),
				std::move(msg)
			)
		);
	}

	template <class B, class H>
	void out_msg(B &&b, H &&hand){
		auto buf = std::make_unique<B>(
			std::forward<B>(b)
		);
		auto &_b = *buf;
		async_write(sock, _b, hand_attach(
			std::forward<H>(hand),
			std::move(buf)
		));
	}
	void wcomplete(sys::error_code ec, size_t bwrite){
		if(ec) sock.cancel();
	}
};

void acceptor_loop(
	io_context &io,
	usock::acceptor &a,
	usock::socket &sock,
	std::weak_ptr<main_op<io_context>> op
){
	a.async_accept(sock, [&, op](sys::error_code ec){
		if(ec){
			std::cerr << "warning: failed to accept ctl session: "
				<< ec << std::endl;
			acceptor_loop(io, a, sock, op);
			return;
		}
//		std::cout << "incoming ctl session" << std::endl;
		std::make_shared<ctl_session>(
			std::exchange(
				sock,
				usock::socket(a.get_executor())
			)
		)->go();
		acceptor_loop(io, a, sock, op);
	});
}*/

#pragma message("begin main")
int main(int argc, char **argv){
	std::ios_base::sync_with_stdio(false);

	char *authtok = getenv("AUTH_TOK");
	if(authtok == NULL){
		std::cerr << "please set your AUTH_TOK environment variable\n";
		return 1;
	}

	io_context io(1);

/*	char *sock_file = getenv("SOCK_FILE");
	if(sock_file == NULL) sock_file = (char *) "gconn.sock";
	std::cout << "using sock file " << sock_file << std::endl;
	usock::acceptor acceptor(io, {sock_file});
	usock::socket sock(io);*/


	using op_type = main_op<io_context>;

	auto op = std::make_shared<op_type>(io, authtok);
	if(getenv("RESUMING")){
		/*too lazy to make sure these env vars are set, so
			dont set RESUMING without the others*/
		std::cout << "resuming detected\n";
		op->session_id = getenv("SESSION_ID");
		std::cout << "got session id " << op->session_id << '\n';
		op->last_sequence = std::stoul(getenv("LAST_SEQUENCE"));
		std::cout << "got last sequence " << op->last_sequence
			<< std::endl;
		op->user_id = std::stoul(getenv("USER_ID"));
	}
	op->go();

	signal_set signals(io, SIGUSR1, SIGUSR2);
	signals.async_wait(coal::detail::bind_front(signal_handler<std::shared_ptr<op_type>>,
		std::ref(io), op));

//	signal(SIGINT, on_interrupt);

//	op.reset();

	io.run();
	return 0;
}
#pragma message("end main")
