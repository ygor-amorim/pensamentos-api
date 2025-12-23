#include "httplib.h"
#include "quote_store.hpp"

// === Security Constants ===
constexpr size_t MAX_TEXT_LENGTH = 2000;    // Max characters for quote text
constexpr size_t MAX_AUTHOR_LENGTH = 200;   // Max characters for author name
constexpr size_t MAX_BODY_SIZE = 8192;      // Max request body size (8KB)

// Validation helper - returns error message or empty string if valid
std::string validate_quote_input(const std::string& text, const std::string& author) {
    if (text.empty()) {
        return "Text cannot be empty";
    }
    if (author.empty()) {
        return "Author cannot be empty";
    }
    if (text.length() > MAX_TEXT_LENGTH) {
        return "Text exceeds maximum length of " + std::to_string(MAX_TEXT_LENGTH) + " characters";
    }
    if (author.length() > MAX_AUTHOR_LENGTH) {
        return "Author exceeds maximum length of " + std::to_string(MAX_AUTHOR_LENGTH) + " characters";
    }
    return ""; // Valid
}

int main() {
    httplib::Server server;
    QuoteStore store("quotes.json");

    // Limit request body size to prevent memory exhaustion attacks
    server.set_payload_max_length(MAX_BODY_SIZE);

    // ---------------------
    // GET /quotes/random
    // Must be defined BEFORE /quotes/:id, otherwise "random" gets matched as an ID
    // ---------------------
    server.Get("/quotes/random", [&store](const httplib::Request&, httplib::Response& res) {
        auto quote = store.get_random();

        if (quote) {
            json j = *quote;
            res.set_content(j.dump(), "application/json");
        } else {
            res.status = 404;
            res.set_content(R"({"error": "No quotes available"})", "application/json");
        }
    });

    // ---------------------
    // GET /quotes
    // GET /quotes?author=X
    // GET /quotes?tag=Y
    // ---------------------
    server.Get("/quotes", [&store](const httplib::Request& req, httplib::Response& res) {
        std::vector<Quote> quotes;

        // Check if there's a "tag" query parameter
        if (req.has_param("tag")) {
            std::string tag = req.get_param_value("tag");
            quotes = store.get_by_tag(tag);
        }
        // Check if there's an "author" query parameter
        else if (req.has_param("author")) {
            std::string author = req.get_param_value("author");
            quotes = store.get_by_author(author);
        }
        // No parameters: return all quotes
        else {
            quotes = store.get_all();
        }

        json j = quotes;
        res.set_content(j.dump(), "application/json");
    });

    // ---------------------
    // GET /quotes/:id
    // ---------------------
    server.Get(R"(/quotes/(\d+))", [&store](const httplib::Request& req, httplib::Response& res) {
        // The regex capture group gives us the ID
        int id = std::stoi(req.matches[1]);
        auto quote = store.get_by_id(id);

        if (quote) {
            json j = *quote;
            res.set_content(j.dump(), "application/json");
        } else {
            res.status = 404;
            res.set_content(R"({"error": "Quote not found"})", "application/json");
        }
    });

    // ---------------------
    // POST /quotes
    // Body: {"text": "...", "author": "..."}
    // ---------------------
    server.Post("/quotes", [&store](const httplib::Request& req, httplib::Response& res) {
        try {
            json body = json::parse(req.body);

            // Validate required fields exist
            if (!body.contains("text") || !body.contains("author")) {
                res.status = 400;
                res.set_content(R"({"error": "Missing 'text' or 'author' field"})", "application/json");
                return;
            }

            // Validate field types
            if (!body["text"].is_string() || !body["author"].is_string()) {
                res.status = 400;
                res.set_content(R"({"error": "'text' and 'author' must be strings"})", "application/json");
                return;
            }

            std::string text = body["text"];
            std::string author = body["author"];

            // Validate input lengths and content
            std::string validation_error = validate_quote_input(text, author);
            if (!validation_error.empty()) {
                res.status = 400;
                json error_response = {{"error", validation_error}};
                res.set_content(error_response.dump(), "application/json");
                return;
            }

            Quote created = store.create(text, author);

            res.status = 201; // 201 Created
            json j = created;
            res.set_content(j.dump(), "application/json");

        } catch (const json::parse_error&) {
            res.status = 400;
            res.set_content(R"({"error": "Invalid JSON"})", "application/json");
        }
    });

    // ---------------------
    // PUT /quotes/:id
    // Body: {"text": "...", "author": "..."}
    // ---------------------
    server.Put(R"(/quotes/(\d+))", [&store](const httplib::Request& req, httplib::Response& res) {
        int id = std::stoi(req.matches[1]);

        try {
            json body = json::parse(req.body);

            if (!body.contains("text") || !body.contains("author")) {
                res.status = 400;
                res.set_content(R"({"error": "Missing 'text' or 'author' field"})", "application/json");
                return;
            }

            // Validate field types
            if (!body["text"].is_string() || !body["author"].is_string()) {
                res.status = 400;
                res.set_content(R"({"error": "'text' and 'author' must be strings"})", "application/json");
                return;
            }

            std::string text = body["text"];
            std::string author = body["author"];

            // Validate input lengths and content
            std::string validation_error = validate_quote_input(text, author);
            if (!validation_error.empty()) {
                res.status = 400;
                json error_response = {{"error", validation_error}};
                res.set_content(error_response.dump(), "application/json");
                return;
            }

            auto updated = store.update(id, text, author);

            if (updated) {
                json j = *updated;
                res.set_content(j.dump(), "application/json");
            } else {
                res.status = 404;
                res.set_content(R"({"error": "Quote not found"})", "application/json");
            }

        } catch (const json::parse_error&) {
            res.status = 400;
            res.set_content(R"({"error": "Invalid JSON"})", "application/json");
        }
    });

    // ---------------------
    // DELETE /quotes/:id
    // ---------------------
    server.Delete(R"(/quotes/(\d+))", [&store](const httplib::Request& req, httplib::Response& res) {
        int id = std::stoi(req.matches[1]);
        bool deleted = store.remove(id);

        if (deleted) {
            res.status = 204; // 204 No Content (success, nothing to return)
        } else {
            res.status = 404;
            res.set_content(R"({"error": "Quote not found"})", "application/json");
        }
    });

    // ---------------------
    // Start the server
    // ---------------------
    std::cout << "Quotes API running at http://localhost:8080\n";
    std::cout << "Endpoints:\n";
    std::cout << "  GET    /quotes          - List all quotes\n";
    std::cout << "  GET    /quotes?author=X - Filter by author\n";
    std::cout << "  GET    /quotes/random   - Get random quote\n";
    std::cout << "  GET    /quotes/:id      - Get quote by ID\n";
    std::cout << "  GET    /quotes?tag=Y    - Filter by tag\n";
    std::cout << "  POST   /quotes          - Create quote\n";
    std::cout << "  PUT    /quotes/:id      - Update quote\n";
    std::cout << "  DELETE /quotes/:id      - Delete quote\n";

    server.listen("0.0.0.0", 8080);

    return 0;
}
