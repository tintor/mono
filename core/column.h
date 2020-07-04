#pragma once
#include <ostream>

extern std::ostream* fos;
void column_section(int width, int count);
void end_column_section();
