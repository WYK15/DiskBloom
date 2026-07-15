#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

namespace diskbloom::tests {

using TestFunction = void (*)();

class Registrar {
public:
    Registrar(std::string_view name, TestFunction function);
};

class CheckFailure final : public std::runtime_error {
public:
    CheckFailure(std::string expression, std::string file, int line);
};

void check(bool condition, std::string_view expression, std::string_view file, int line);
int run_all();

} // namespace diskbloom::tests

#define TEST_CASE(name)                                      \
    static void name();                                      \
    static const ::diskbloom::tests::Registrar name##_entry( \
        #name,                                               \
        &name);                                              \
    static void name()

#define CHECK(condition)                                                   \
    ::diskbloom::tests::check(                                             \
        static_cast<bool>(condition), #condition, __FILE__, __LINE__)
