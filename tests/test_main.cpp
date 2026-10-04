#include "test_registry.hpp"
#include <iostream>
std::vector<TestCase>& tests(){static std::vector<TestCase> t;return t;}
int main(){int failed=0;for(const auto&t:tests()){try{t.fn();std::cout<<"[PASS] "<<t.name<<'\n';}catch(const std::exception&e){++failed;std::cerr<<"[FAIL] "<<t.name<<": "<<e.what()<<'\n';}}std::cout<<"tests="<<tests().size()<<" failed="<<failed<<'\n';return failed?1:0;}
