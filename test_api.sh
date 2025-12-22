#!/bin/bash
# Integration tests for Quotes API
# Run with: bash test_api.sh
# Requires: curl, server running on localhost:8080

BASE_URL="http://localhost:8080"
PASSED=0
FAILED=0

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test helper function
test_endpoint() {
    local description="$1"
    local expected_status="$2"
    local method="$3"
    local endpoint="$4"
    local data="$5"
    local expected_content="$6"

    if [ -n "$data" ]; then
        response=$(curl -s -w "\n%{http_code}" -X "$method" "$BASE_URL$endpoint" \
            -H "Content-Type: application/json" -d "$data")
    else
        response=$(curl -s -w "\n%{http_code}" -X "$method" "$BASE_URL$endpoint")
    fi

    # Split response body and status code
    status_code=$(echo "$response" | tail -n1)
    body=$(echo "$response" | sed '$d')

    # Check status code
    if [ "$status_code" != "$expected_status" ]; then
        echo -e "${RED}FAIL${NC}: $description"
        echo "       Expected status: $expected_status, Got: $status_code"
        ((FAILED++))
        return 1
    fi

    # Check content if specified
    if [ -n "$expected_content" ]; then
        if ! echo "$body" | grep -q "$expected_content"; then
            echo -e "${RED}FAIL${NC}: $description"
            echo "       Expected content containing: $expected_content"
            echo "       Got: $body"
            ((FAILED++))
            return 1
        fi
    fi

    echo -e "${GREEN}PASS${NC}: $description"
    ((PASSED++))
    return 0
}

echo -e "${YELLOW}========================================${NC}"
echo -e "${YELLOW}  Quotes API Integration Tests${NC}"
echo -e "${YELLOW}========================================${NC}"
echo ""

# Check if server is running
if ! curl -s "$BASE_URL/quotes" > /dev/null 2>&1; then
    echo -e "${RED}ERROR: Server not running at $BASE_URL${NC}"
    echo "Please start the server first: ./api.exe"
    exit 1
fi

echo "Server is running. Starting tests..."
echo ""

# ==========================================
# GET Tests
# ==========================================
echo -e "${YELLOW}--- GET Endpoints ---${NC}"

test_endpoint "GET /quotes returns 200" "200" "GET" "/quotes"
test_endpoint "GET /quotes/random returns 200" "200" "GET" "/quotes/random"
test_endpoint "GET /quotes/1 returns 200" "200" "GET" "/quotes/1"
test_endpoint "GET /quotes/99999 returns 404" "404" "GET" "/quotes/99999" "" "not found"
test_endpoint "GET /quotes?author=marx (filter)" "200" "GET" "/quotes?author=marx"

echo ""

# ==========================================
# POST Tests
# ==========================================
echo -e "${YELLOW}--- POST Endpoints ---${NC}"

test_endpoint "POST /quotes creates quote" "201" "POST" "/quotes" \
    '{"text":"Integration test quote","author":"Tester"}' "Integration test quote"

test_endpoint "POST /quotes missing fields returns 400" "400" "POST" "/quotes" \
    '{"text":"Only text"}' "Missing"

test_endpoint "POST /quotes invalid JSON returns 400" "400" "POST" "/quotes" \
    'not valid json' "Invalid JSON"

test_endpoint "POST /quotes empty text returns 400" "400" "POST" "/quotes" \
    '{"text":"","author":"Author"}' "empty"

test_endpoint "POST /quotes non-string fields returns 400" "400" "POST" "/quotes" \
    '{"text":123,"author":"Author"}' "must be strings"

echo ""

# ==========================================
# PUT Tests
# ==========================================
echo -e "${YELLOW}--- PUT Endpoints ---${NC}"

# First create a quote to update
created=$(curl -s -X POST "$BASE_URL/quotes" \
    -H "Content-Type: application/json" \
    -d '{"text":"To be updated","author":"Original"}')
update_id=$(echo "$created" | grep -o '"id":[0-9]*' | grep -o '[0-9]*')

test_endpoint "PUT /quotes/$update_id updates quote" "200" "PUT" "/quotes/$update_id" \
    '{"text":"Updated text","author":"Updated Author"}' "Updated text"

test_endpoint "PUT /quotes/99999 returns 404" "404" "PUT" "/quotes/99999" \
    '{"text":"Text","author":"Author"}' "not found"

echo ""

# ==========================================
# DELETE Tests
# ==========================================
echo -e "${YELLOW}--- DELETE Endpoints ---${NC}"

# Create a quote to delete
created=$(curl -s -X POST "$BASE_URL/quotes" \
    -H "Content-Type: application/json" \
    -d '{"text":"To be deleted","author":"Temp"}')
delete_id=$(echo "$created" | grep -o '"id":[0-9]*' | grep -o '[0-9]*')

test_endpoint "DELETE /quotes/$delete_id returns 204" "204" "DELETE" "/quotes/$delete_id"
test_endpoint "DELETE /quotes/99999 returns 404" "404" "DELETE" "/quotes/99999" "" "not found"

echo ""

# ==========================================
# Security Tests
# ==========================================
echo -e "${YELLOW}--- Security Tests ---${NC}"

# Test max length validation (create a very long string)
long_text=$(printf 'x%.0s' {1..2500})
test_endpoint "POST with oversized text returns 400" "400" "POST" "/quotes" \
    "{\"text\":\"$long_text\",\"author\":\"Author\"}" "exceeds maximum"

echo ""

# ==========================================
# Summary
# ==========================================
echo -e "${YELLOW}========================================${NC}"
echo -e "  Results: ${GREEN}$PASSED passed${NC}, ${RED}$FAILED failed${NC}"
echo -e "${YELLOW}========================================${NC}"

# Cleanup: delete test quotes
curl -s -X DELETE "$BASE_URL/quotes/$update_id" > /dev/null 2>&1

if [ $FAILED -gt 0 ]; then
    exit 1
fi
exit 0
