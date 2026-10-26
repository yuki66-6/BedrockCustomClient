#include "TestFramework.h"

#include "utils/Log.h"

#include <windows.h>

#include <chrono>
#include <cstdio>
#include <random>

namespace bcctest {
namespace {
int g_failures = 0;
}

std::vector<TestCase>& registry() {
    static std::vector<TestCase> r;
    return r;
}

void reportFailure(const char* file, int line, const std::string& expr) {
    ++g_failures;
    std::printf("    ÉCHEC %s:%d : %s\n", file, line, expr.c_str());
}

TempDir::TempDir() {
    std::mt19937_64 rng(static_cast<unsigned long long>(std::chrono::steady_clock::now().time_since_epoch().count()));
    path_ = std::filesystem::temp_directory_path() / ("bcc_test_" + std::to_string(rng() % 100000000));
    std::filesystem::create_directories(path_);
}

TempDir::~TempDir() {
    std::error_code ec;
    std::filesystem::remove_all(path_, ec);
}

} // namespace bcctest

int main(int argc, char** argv) {
    SetConsoleOutputCP(CP_UTF8);
    bcc::log::setThrowOnImGuiAssert(true);
    const char* filter = argc > 1 ? argv[1] : nullptr;
    int run = 0, failedCases = 0;
    for (auto& tc : bcctest::registry()) {
        if (filter && std::string(tc.name).find(filter) == std::string::npos) continue;
        ++run;
        int before = bcctest::g_failures;
        std::printf("[ RUN  ] %s\n", tc.name);
        try {
            tc.fn();
        } catch (const bcctest::RequireFailed&) {
        } catch (const std::exception& e) {
            bcctest::reportFailure(__FILE__, __LINE__, std::string("exception : ") + e.what());
        }
        bool ok = bcctest::g_failures == before;
        if (!ok) ++failedCases;
        std::printf("[ %s ] %s\n", ok ? " OK " : "FAIL", tc.name);
    }
    std::printf("\n%d test(s), %d échec(s), %d vérification(s) en échec\n", run, failedCases, bcctest::g_failures);
    return failedCases == 0 ? 0 : 1;
}
