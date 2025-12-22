#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "quote_store.hpp"
#include <filesystem>

// Helper to create a temporary test file
class TestFixture {
public:
    std::string test_file = "test_quotes.json";

    TestFixture() {
        // Clean up any existing test file
        std::filesystem::remove(test_file);
    }

    ~TestFixture() {
        // Clean up after tests
        std::filesystem::remove(test_file);
    }
};

TEST_CASE_FIXTURE(TestFixture, "QuoteStore - Create and retrieve quotes") {
    QuoteStore store(test_file);

    SUBCASE("Empty store returns no quotes") {
        auto all = store.get_all();
        CHECK(all.empty());
    }

    SUBCASE("Create a quote and retrieve it") {
        auto created = store.create("Test quote", "Test Author");

        CHECK(created.id == 1);
        CHECK(created.text == "Test quote");
        CHECK(created.author == "Test Author");

        auto retrieved = store.get_by_id(1);
        REQUIRE(retrieved.has_value());
        CHECK(retrieved->text == "Test quote");
    }

    SUBCASE("Create multiple quotes with auto-incrementing IDs") {
        store.create("First", "Author1");
        store.create("Second", "Author2");
        store.create("Third", "Author3");

        auto all = store.get_all();
        CHECK(all.size() == 3);
        CHECK(all[0].id == 1);
        CHECK(all[1].id == 2);
        CHECK(all[2].id == 3);
    }
}

TEST_CASE_FIXTURE(TestFixture, "QuoteStore - Update quotes") {
    QuoteStore store(test_file);
    store.create("Original text", "Original Author");

    SUBCASE("Update existing quote") {
        auto updated = store.update(1, "Updated text", "Updated Author");

        REQUIRE(updated.has_value());
        CHECK(updated->text == "Updated text");
        CHECK(updated->author == "Updated Author");
    }

    SUBCASE("Update non-existent quote returns nullopt") {
        auto result = store.update(999, "Text", "Author");
        CHECK_FALSE(result.has_value());
    }
}

TEST_CASE_FIXTURE(TestFixture, "QuoteStore - Delete quotes") {
    QuoteStore store(test_file);
    store.create("Quote to delete", "Author");

    SUBCASE("Delete existing quote") {
        bool deleted = store.remove(1);
        CHECK(deleted);
        CHECK(store.get_all().empty());
    }

    SUBCASE("Delete non-existent quote returns false") {
        bool deleted = store.remove(999);
        CHECK_FALSE(deleted);
    }
}

TEST_CASE_FIXTURE(TestFixture, "QuoteStore - Filter by author") {
    QuoteStore store(test_file);
    store.create("Quote 1", "Friedrich Nietzsche");
    store.create("Quote 2", "Karl Marx");
    store.create("Quote 3", "Friedrich Engels");

    SUBCASE("Case-insensitive search") {
        auto results = store.get_by_author("FRIEDRICH");
        CHECK(results.size() == 2);
    }

    SUBCASE("Partial match") {
        auto results = store.get_by_author("Marx");
        CHECK(results.size() == 1);
        CHECK(results[0].author == "Karl Marx");
    }

    SUBCASE("No matches returns empty") {
        auto results = store.get_by_author("Plato");
        CHECK(results.empty());
    }
}

TEST_CASE_FIXTURE(TestFixture, "QuoteStore - Random quote") {
    QuoteStore store(test_file);

    SUBCASE("Empty store returns nullopt") {
        auto random = store.get_random();
        CHECK_FALSE(random.has_value());
    }

    SUBCASE("Non-empty store returns a quote") {
        store.create("Only quote", "Author");
        auto random = store.get_random();
        REQUIRE(random.has_value());
        CHECK(random->text == "Only quote");
    }
}

TEST_CASE_FIXTURE(TestFixture, "QuoteStore - Persistence") {
    // Create store and add data
    {
        QuoteStore store(test_file);
        store.create("Persistent quote", "Author");
    }

    // Create new store instance - should load from file
    {
        QuoteStore store(test_file);
        auto all = store.get_all();
        REQUIRE(all.size() == 1);
        CHECK(all[0].text == "Persistent quote");
    }
}

TEST_CASE_FIXTURE(TestFixture, "QuoteStore - ID continuity after reload") {
    {
        QuoteStore store(test_file);
        store.create("Quote 1", "Author");
        store.create("Quote 2", "Author");
    }

    {
        QuoteStore store(test_file);
        auto created = store.create("Quote 3", "Author");
        CHECK(created.id == 3);  // Should continue from highest ID
    }
}
