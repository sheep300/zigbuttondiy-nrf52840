#pragma once
void test_fail(int line);
#define assert(x) do {if(!(x))test_fail(__LINE__);} while(0)
