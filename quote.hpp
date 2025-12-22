#ifndef QUOTE_HPP
#define QUOTE_HPP

#include <string>
#include "json.hpp"

using json = nlohmann::json;

struct Quote {
    int id;
    std::string text;
    std::string author;
};

// These functions tell nlohmann/json how to convert between Quote and JSON.
// "to_json" is called when you do: json j = quote;
// "from_json" is called when you do: Quote q = j.get<Quote>();

inline void to_json(json& j, const Quote& q) {
    j = json{
        {"id", q.id},
        {"text", q.text},
        {"author", q.author}
    };
}

inline void from_json(const json& j, Quote& q) {
    j.at("id").get_to(q.id);
    j.at("text").get_to(q.text);
    j.at("author").get_to(q.author);
}

#endif
