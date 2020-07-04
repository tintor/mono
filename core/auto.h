#pragma once

template <typename T>
class AutoOutOfScope {
   public:
    AutoOutOfScope(T& destructor) : m_destructor(destructor) {}
    ~AutoOutOfScope() { m_destructor(); }

   private:
    T& m_destructor;
};

#define TOKEN_PASTEx(x, y) x##y
#define TOKEN_PASTE(x, y) TOKEN_PASTEx(x, y)

#define AUTO_INTERNAL(Destructor, counter)                                        \
    auto TOKEN_PASTE(auto_func_, counter) = [&]() { Destructor; };                \
    AutoOutOfScope<decltype(TOKEN_PASTE(auto_func_, counter))> TOKEN_PASTE(auto_, \
                                                                           counter)(TOKEN_PASTE(auto_func_, counter));

#define ON_SCOPE_EXIT(Destructor) AUTO_INTERNAL(Destructor, __COUNTER__)
