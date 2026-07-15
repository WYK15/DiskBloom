#include "test_support.h"

#include <iostream>
#include <sstream>
#include <utility>
#include <vector>

namespace diskbloom::tests {
namespace {

struct TestCase {
    std::string name;
    TestFunction function;
};

std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

} // namespace

Registrar::Registrar(const std::string_view name, const TestFunction function) {
    registry().push_back({std::string(name), function});
}

CheckFailure::CheckFailure(std::string expression, std::string file, const int line)
    : std::runtime_error([&] {
          std::ostringstream message;
          message << file << ':' << line << ": check failed: " << expression;
          return message.str();
      }()) {}

void check(
    const bool condition,
    const std::string_view expression,
    const std::string_view file,
    const int line) {
    if (!condition) {
        throw CheckFailure(std::string(expression), std::string(file), line);
    }
}

int run_all() {
    int failures = 0;
    for (const auto& test : registry()) {
        try {
            test.function();
            std::cout << "[PASS] " << test.name << '\n';
        } catch (const std::exception& error) {
            ++failures;
            std::cerr << "[FAIL] " << test.name << ": " << error.what() << '\n';
        } catch (...) {
            ++failures;
            std::cerr << "[FAIL] " << test.name << ": unknown exception\n";
        }
    }

    std::cout << registry().size() << " tests, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}

} // namespace diskbloom::tests
