#pragma once

// Mini-framework de tests sans dépendance : TEST_CASE / CHECK / REQUIRE.

#include <cmath>
#include <filesystem>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace bcctest {

struct TestCase {
    const char* name;
    std::function<void()> fn;
};

std::vector<TestCase>& registry();
void reportFailure(const char* file, int line, const std::string& expr);

struct Registrar {
    Registrar(const char* name, std::function<void()> fn) { registry().push_back({name, std::move(fn)}); }
};

struct RequireFailed : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// Dossier temporaire propre à un test, supprimé à la destruction.
class TempDir {
public:
    TempDir();
    ~TempDir();
    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

} // namespace bcctest

#define BCC_CONCAT2(a, b) a##b
#define BCC_CONCAT(a, b) BCC_CONCAT2(a, b)
#define TEST_CASE(name)                                                                                     \
    static void BCC_CONCAT(bcc_test_fn_, __LINE__)();                                                       \
    static bcctest::Registrar BCC_CONCAT(bcc_test_reg_, __LINE__)(name, &BCC_CONCAT(bcc_test_fn_, __LINE__)); \
    static void BCC_CONCAT(bcc_test_fn_, __LINE__)()

#define CHECK(expr)                                                                                         \
    do {                                                                                                    \
        if (!(expr)) bcctest::reportFailure(__FILE__, __LINE__, #expr);                                     \
    } while (0)

#define REQUIRE(expr)                                                                                       \
    do {                                                                                                    \
        if (!(expr)) {                                                                                      \
            bcctest::reportFailure(__FILE__, __LINE__, #expr);                                              \
            throw bcctest::RequireFailed(#expr);                                                            \
        }                                                                                                   \
    } while (0)

#define CHECK_NEAR(a, b, eps) CHECK(std::fabs(static_cast<double>(a) - static_cast<double>(b)) <= (eps))
