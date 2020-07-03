#pragma once
#include <string_view>

void Check(bool value, std::string_view message = "", const char* file = __builtin_FILE(),
                  unsigned line = __builtin_LINE(), const char* function = __builtin_FUNCTION());

#if 0
#define DCheck(A, B) Check(A, B)
#else
#define DCheck(A, B)
#endif

void Fail(std::string_view message = "", const char* file = __builtin_FILE(), unsigned line = __builtin_LINE(),
                 const char* function = __builtin_FUNCTION());
