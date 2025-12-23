#ifndef QUOTE_STORE_HPP
#define QUOTE_STORE_HPP

#include <vector>
#include <fstream>
#include <optional>
#include <random>
#include <algorithm>
#include <shared_mutex>
#include "quote.hpp"

class QuoteStore {
private:
    std::vector<Quote> quotes_;
    std::string filename_;
    int next_id_ = 1;
    mutable std::shared_mutex mutex_;  // Allows multiple readers OR one writer

    // Load quotes from JSON file into memory
    void load() {
        std::ifstream file(filename_);
        if (!file.is_open()) {
            return; // File doesn't exist yet, start with empty list
        }

        json j;
        file >> j;
        quotes_ = j.get<std::vector<Quote>>();

        // Find the highest ID so we can continue from there
        for (const auto& q : quotes_) {
            if (q.id >= next_id_) {
                next_id_ = q.id + 1;
            }
        }
    }

    // Save all quotes to JSON file
    void save() {
        std::ofstream file(filename_);
        json j = quotes_;
        file << j.dump(2); // Pretty-print with 2-space indent
    }

public:
    explicit QuoteStore(const std::string& filename) : filename_(filename) {
        load();
    }

    // GET /quotes - return all quotes
    std::vector<Quote> get_all() const {
        std::shared_lock lock(mutex_);  // Shared lock for reading
        return quotes_;
    }

    // GET /quotes?author=X - filter by author (case-insensitive partial match)
    std::vector<Quote> get_by_author(const std::string& author) const {
        std::shared_lock lock(mutex_);  // Shared lock for reading
        std::vector<Quote> result;

        // Convert search term to lowercase for case-insensitive matching
        std::string author_lower = author;
        std::transform(author_lower.begin(), author_lower.end(),
                       author_lower.begin(), ::tolower);

        for (const auto& q : quotes_) {
            std::string q_author_lower = q.author;
            std::transform(q_author_lower.begin(), q_author_lower.end(),
                           q_author_lower.begin(), ::tolower);

            // Check if the quote's author contains the search term
            if (q_author_lower.find(author_lower) != std::string::npos) {
                result.push_back(q);
            }
        }
        return result;
    }
    // GET /quotes?tag=X - filter by tag (case-insensitive exact match)
    std::vector<Quote> get_by_tag(const std::string& tag) const {
        std::shared_lock lock(mutex_); // As on previous method
        std::vector<Quote> result;

        // Convert search term to lowercase for case-insensitive matching
        std::string tag_lower = tag;
        std::transform (tag_lower.begin(), tag_lower.end(),
                       tag_lower.begin(), ::tolower);

        // Loop through quotes
        for (const auto& q : quotes_) {
            // Check if quote has the specified tag
            for (const auto& quote_tag : q.tags) {
                std::string quote_tag_lower = quote_tag;
                std::transform(quote_tag_lower.begin(), quote_tag_lower.end(),
                               quote_tag_lower.begin(), ::tolower);

            // if matching tag is found, add quote to results
            if (quote_tag_lower == tag_lower) {
                result.push_back(q);
                break; // no need to check other tags for this quote
            }
            }
        }
        return result;
    }

    // GET /quotes/random - return a random quote
    std::optional<Quote> get_random() const {
        std::shared_lock lock(mutex_);  // Shared lock for reading
        if (quotes_.empty()) {
            return std::nullopt;
        }

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dist(0, quotes_.size() - 1);

        return quotes_[dist(gen)];
    }

    // GET /quotes/:id - find by ID
    std::optional<Quote> get_by_id(int id) const {
        std::shared_lock lock(mutex_);  // Shared lock for reading
        for (const auto& q : quotes_) {
            if (q.id == id) {
                return q;
            }
        }
        return std::nullopt; // Not found
    }

    // POST /quotes - create new quote
    Quote create(const std::string& text, const std::string& author) {
        std::unique_lock lock(mutex_);  // Exclusive lock for writing
        Quote q{next_id_++, text, author};
        quotes_.push_back(q);
        save();
        return q;
    }

    // PUT /quotes/:id - update existing quote
    std::optional<Quote> update(int id, const std::string& text, const std::string& author) {
        std::unique_lock lock(mutex_);  // Exclusive lock for writing
        for (auto& q : quotes_) {
            if (q.id == id) {
                q.text = text;
                q.author = author;
                save();
                return q;
            }
        }
        return std::nullopt; // Not found
    }

    // DELETE /quotes/:id - remove a quote
    bool remove(int id) {
        std::unique_lock lock(mutex_);  // Exclusive lock for writing
        auto it = std::remove_if(quotes_.begin(), quotes_.end(),
            [id](const Quote& q) { return q.id == id; });

        if (it != quotes_.end()) {
            quotes_.erase(it, quotes_.end());
            save();
            return true;
        }
        return false; // Not found
    }
};

#endif
