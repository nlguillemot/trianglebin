#include "dxutil.h"

#include <comdef.h>
#include <cstdio>
#include <cstdarg>
#include <set>
#include <tuple>
#include <memory>
#include <mutex>

std::set<std::tuple<std::string, std::string, int>> g_IgnoredAsserts;
std::mutex g_IgnoredAssertsMutex;

std::wstring WideFromMultiByte(const char* s)
{
    int bufSize = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, NULL, 0);
    CHECKWIN32(bufSize != 0);

    std::wstring ws(bufSize, 0);
    CHECKWIN32(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, &ws[0], bufSize));
    ws.pop_back(); // remove null terminator
    return ws;
}

std::wstring WideFromMultiByte(const std::string& s)
{
    return WideFromMultiByte(s.c_str());
}

std::string MultiByteFromWide(const wchar_t* ws)
{
    int bufSize = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, ws, -1, NULL, 0, NULL, NULL);
    CHECKWIN32(bufSize != 0);

    std::string s(bufSize, 0);
    CHECKWIN32(WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, ws, -1, &s[0], bufSize, NULL, NULL));
    s.pop_back(); // remove null terminator
    return s;
}

std::string MultiByteFromWide(const std::wstring& ws)
{
    return MultiByteFromWide(ws.c_str());
}

std::string MultiByteFromHR(HRESULT hr)
{
    _com_error err(hr);
    return MultiByteFromWide(err.ErrorMessage());
}

bool detail_CheckHR(HRESULT hr, const char* file, const char* function, int line)
{
    if (SUCCEEDED(hr))
    {
        return true;
    }

    std::lock_guard<std::mutex> lock(g_IgnoredAssertsMutex);

    if (g_IgnoredAsserts.find(std::make_tuple(file, function, line)) != g_IgnoredAsserts.end())
    {
        return false;
    }

    std::wstring wfile = WideFromMultiByte(file);
    std::wstring wfunction = WideFromMultiByte(function);
    _com_error err(hr);

    std::wstring msg = std::wstring() +
        L"File: " + wfile + L"\n" +
        L"Function: " + wfunction + L"\n" +
        L"Line: " + std::to_wstring(line) + L"\n" +
        L"ErrorMessage: " + err.ErrorMessage() + L"\n";

    int result = MessageBoxW(NULL, msg.c_str(), L"Error", MB_ABORTRETRYIGNORE);
    if (result == IDABORT)
    {
        ExitProcess(-1);
    }
    else if (result == IDRETRY)
    {
        DebugBreak();
    }
    else if (result == IDIGNORE)
    {
        g_IgnoredAsserts.insert(std::make_tuple(file, function, line));
    }

    return false;
}

void SimpleMessageBox_FatalError(const char* fmt, ...)
{
    va_list vl;
    va_start(vl, fmt);

    int nc = _vscprintf(fmt, vl);
    std::unique_ptr<char[]> chars = std::make_unique<char[]>(nc + 1);
    vsnprintf_s(chars.get(), nc + 1, nc, fmt, vl);

    std::wstring wmsg = WideFromMultiByte(chars.get());
    MessageBoxW(NULL, wmsg.c_str(), L"Fatal Error", MB_OK);

    va_end(vl);

#ifdef _DEBUG
    DebugBreak();
#endif

    ExitProcess(-1);
}

bool detail_CheckWin32(BOOL okay, const char* file, const char* function, int line)
{
    if (okay)
    {
        return true;
    }

    return detail_CheckHR(HRESULT_FROM_WIN32(GetLastError()), file, function, line);
}