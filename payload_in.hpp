#pragma once

#include <nlohmann/json.hpp>

namespace json_types {
    using nlohmann::json;

    struct payload_in {
        int64_t op;
        nlohmann::json d;
        int64_t s;
        std::string t;
    };
}

namespace nlohmann {
    void from_json(const json & j, json_types::payload_in & x);
    void to_json(json & j, const json_types::payload_in & x);

    inline void from_json(const json & j, json_types::payload_in& x) {
        x.op = j.at("op").get<int64_t>();
        x.d = j.at("d").get<nlohmann::json>();
	if(x.op == 0){
        	x.s = j.at("s").get<int64_t>();
	        x.t = j.at("t").get<std::string>();
	}
    }

    inline void to_json(json & j, const json_types::payload_in & x) {
        j = json::object();
        j["op"] = x.op;
        j["d"] = x.d;
	if(x.op == 0){
	        j["s"] = x.s;
	        j["t"] = x.t;
	}
    }
}

