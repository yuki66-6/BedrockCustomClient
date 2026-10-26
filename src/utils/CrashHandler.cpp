#include "utils/CrashHandler.h"

#include "utils/Log.h"

#include <windows.h>
#include <dbghelp.h>

#include <atomic>
#include <string>

#pragma comment(lib, "dbghelp.lib")

namespace bcc::crash {
namespace {

std::filesystem::path g_dumpDir;
std::atomic<bool> g_reported{false};

bool isFatal(DWORD code) {
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_ILLEGAL_INSTRUCTION:
    case EXCEPTION_STACK_OVERFLOW:
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
    case EXCEPTION_PRIV_INSTRUCTION:
    case EXCEPTION_IN_PAGE_ERROR:
    case STATUS_HEAP_CORRUPTION:
    case STATUS_STACK_BUFFER_OVERRUN: return true;
    default: return false;
    }
}

std::string describe(DWORD64 address) {
    HANDLE process = GetCurrentProcess();
    char buffer[sizeof(SYMBOL_INFO) + 256] = {};
    auto* sym = reinterpret_cast<SYMBOL_INFO*>(buffer);
    sym->SizeOfStruct = sizeof(SYMBOL_INFO);
    sym->MaxNameLen = 255;
    std::string out;
    DWORD64 disp = 0;
    if (SymFromAddr(process, address, &disp, sym)) out = std::format("{}+0x{:X}", sym->Name, disp);
    IMAGEHLP_LINE64 line{sizeof(line)};
    DWORD lineDisp = 0;
    if (SymGetLineFromAddr64(process, address, &lineDisp, &line))
        out += std::format(" ({}:{})", line.FileName, line.LineNumber);
    HMODULE mod = nullptr;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(address), &mod)) {
        char name[MAX_PATH];
        GetModuleFileNameA(mod, name, MAX_PATH);
        out += std::format(" [{}+0x{:X}]", name, address - reinterpret_cast<DWORD64>(mod));
    }
    return out;
}

void writeDump(EXCEPTION_POINTERS* info) {
    std::error_code ec;
    std::filesystem::create_directories(g_dumpDir, ec);
    SYSTEMTIME t;
    GetLocalTime(&t);
    auto file = g_dumpDir / std::format(L"crash-{:04}{:02}{:02}-{:02}{:02}{:02}.dmp", t.wYear, t.wMonth, t.wDay,
                                        t.wHour, t.wMinute, t.wSecond);
    HANDLE h = CreateFileW(file.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    MINIDUMP_EXCEPTION_INFORMATION mei{GetCurrentThreadId(), info, FALSE};
    MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), h, MiniDumpNormal, &mei, nullptr, nullptr);
    CloseHandle(h);
    log::error("Minidump écrit : {}", file.string());
}

void report(EXCEPTION_POINTERS* info) {
    if (g_reported.exchange(true)) return;
    auto* rec = info->ExceptionRecord;
    log::error("Exception fatale 0x{:08X} à {}", rec->ExceptionCode,
               describe(reinterpret_cast<DWORD64>(rec->ExceptionAddress)));
    if (rec->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && rec->NumberParameters >= 2)
        log::error("  {} de l'adresse 0x{:X}", rec->ExceptionInformation[0] ? "écriture" : "lecture",
                   rec->ExceptionInformation[1]);
    // Pile d'appels (meilleur effort).
    CONTEXT ctx = *info->ContextRecord;
    STACKFRAME64 frame{};
    frame.AddrPC.Offset = ctx.Rip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = ctx.Rbp;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Offset = ctx.Rsp;
    frame.AddrStack.Mode = AddrModeFlat;
    for (int i = 0; i < 24; ++i) {
        if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, GetCurrentProcess(), GetCurrentThread(), &frame, &ctx, nullptr,
                         SymFunctionTableAccess64, SymGetModuleBase64, nullptr) ||
            !frame.AddrPC.Offset)
            break;
        log::error("  #{:02} {}", i, describe(frame.AddrPC.Offset));
    }
    writeDump(info);
}

LONG CALLBACK vectoredHandler(EXCEPTION_POINTERS* info) {
    // Les exceptions levées dans un callback Win32 sont parfois avalées par le
    // noyau (STATUS_FATAL_USER_CALLBACK_EXCEPTION) : on les journalise ici, à
    // la première occurrence.
    if (isFatal(info->ExceptionRecord->ExceptionCode)) report(info);
    return EXCEPTION_CONTINUE_SEARCH;
}

LONG WINAPI unhandledFilter(EXCEPTION_POINTERS* info) {
    report(info);
    return EXCEPTION_EXECUTE_HANDLER;
}

} // namespace

void install(const std::filesystem::path& dumpDir) {
    g_dumpDir = dumpDir;
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
    SymInitialize(GetCurrentProcess(), nullptr, TRUE);
    AddVectoredExceptionHandler(1, &vectoredHandler);
    SetUnhandledExceptionFilter(&unhandledFilter);
}

} // namespace bcc::crash
