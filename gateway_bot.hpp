//  To parse this JSON data, first install
//
//      json.hpp  https://github.com/nlohmann/json
//
//  Then include this file, and then do
//
//     gatewayBot data = nlohmann::json::parse(jsonString);

#pragma once

#include <nlohmann/json.hpp>

namespace json_types {
    using nlohmann::json;

    inline json get_untyped(const json & j, const char * property) {
        if (j.find(property) != j.end()) {
            return j.at(property).get<json>();
        }
        return json();
    }

    inline json get_untyped(const json & j, std::string property) {
        return get_untyped(j, property.data());
    }

    struct sessionStartLimit {
        int64_t total;
        int64_t remaining;
        int64_t reset_after;
    };

    struct gatewayBot {
        std::string url;
        int64_t shards;
        sessionStartLimit session_start_limit;
    };
}

namespace nlohmann {
    void from_json(const json & j, json_types::sessionStartLimit & x);
    void to_json(json & j, const json_types::sessionStartLimit & x);

    void from_json(const json & j, json_types::gatewayBot & x);
    void to_json(json & j, const json_types::gatewayBot & x);

    inline void from_json(const json & j, json_types::sessionStartLimit& x) {
        x.total = j.at("total").get<int64_t>();
        x.remaining = j.at("remaining").get<int64_t>();
        x.reset_after = j.at("reset_after").get<int64_t>();
    }

    inline void to_json(json & j, const json_types::sessionStartLimit & x) {
        j = json::object();
        j["total"] = x.total;
        j["remaining"] = x.remaining;
        j["reset_after"] = x.reset_after;
    }

    inline void from_json(const json & j, json_types::gatewayBot& x) {
        x.url = j.at("url").get<std::string>();
        x.shards = j.at("shards").get<int64_t>();
        x.session_start_limit = j.at("session_start_limit").get<json_types::sessionStartLimit>();
    }

    inline void to_json(json & j, const json_types::gatewayBot & x) {
        j = json::object();
        j["url"] = x.url;
        j["shards"] = x.shards;
        j["session_start_limit"] = x.session_start_limit;
    }
}
