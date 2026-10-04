#pragma once
#include <functional>
#include <vector>
#include <stdexcept>
#include <string>
#include <string>
struct TestCase { const char* name; std::function<void()> fn; };
std::vector<TestCase>& tests();
#define TEST(name) void name(); static struct R_##name{R_##name(){tests().push_back({#name,name});}} r_##name; void name()
#define REQUIRE(cond) do{if(!(cond))throw std::runtime_error(std::string("REQUIRE failed: ")+ #cond);}while(0)
#define REQUIRE_EQ(a,b) do{auto va=(a);auto vb=(b);if(!(va==vb))throw std::runtime_error(std::string("REQUIRE_EQ failed: ")+ #a + " != " + #b);}while(0)
