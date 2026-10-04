#include "test_registry.hpp"
#include "spsc_queue.hpp"

TEST(spsc_push_pop_and_capacity) {
    md::SpscQueue<int, 4> q;
    REQUIRE(q.empty());
    REQUIRE(q.try_push(10));
    REQUIRE(q.try_push(20));
    REQUIRE(q.try_push(30));
    REQUIRE(!q.try_push(40));
    int x = 0;
    REQUIRE(q.try_pop(x)); REQUIRE_EQ(x, 10);
    REQUIRE(q.try_push(40));
    REQUIRE(q.try_pop(x)); REQUIRE_EQ(x, 20);
    REQUIRE(q.try_pop(x)); REQUIRE_EQ(x, 30);
    REQUIRE(q.try_pop(x)); REQUIRE_EQ(x, 40);
    REQUIRE(!q.try_pop(x));
}
