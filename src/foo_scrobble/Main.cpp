#include "fb2ksdk.h"

#include <optional>
#include <span>

#pragma comment(lib, "Version.lib")

namespace foo_scrobble
{
namespace
{

HMODULE g_DllModule = nullptr;

std::span<std::byte const> GetModuleResource(HMODULE module, wchar_t const* name,
                                             wchar_t const* type) noexcept
{
    auto const resInfo = FindResourceW(module, name, type);
    if (!resInfo)
        return {};

    HGLOBAL const resData = LoadResource(module, resInfo);
    size_t const size = SizeofResource(module, resInfo);
    if (!resData || size == 0)
        return {};

    auto const* ptr = static_cast<std::byte const*>(LockResource(resData));
    return {ptr, size};
}

template<typename Container>
HRESULT GetModuleFileNameW(HMODULE module, Container& path)
{
    constexpr DWORD max = std::numeric_limits<DWORD>::max() / 2;

    for (DWORD size = MAX_PATH; size < max; size *= 2) {
        path.resize(size);
        DWORD const length = GetModuleFileNameW(module, path.data(), size);
        if (length < size)
            return S_OK;
    }

    return HRESULT_FROM_WIN32(GetLastError());
}

std::optional<std::string> GetFileInfoValue(wchar_t const* modulePath,
                                            wchar_t const* valueName)
{
    DWORD handle;
    DWORD infoSize = GetFileVersionInfoSizeW(modulePath, &handle);
    if (infoSize == 0)
        return std::nullopt;

    auto buffer = std::make_unique<BYTE[]>(infoSize);
    if (!GetFileVersionInfoW(modulePath, 0, infoSize, buffer.get()))
        return std::nullopt;

    void* valuePtr;
    UINT valueSize;
    if (!VerQueryValueW(buffer.get(), L"\\VarFileInfo\\Translation", &valuePtr,
                        &valueSize) ||
        valueSize < sizeof(DWORD))
        return std::nullopt;

    DWORD value;
    std::memcpy(&value, valuePtr, sizeof(value));

    LANGID const langId = value & 0xFFFF;
    WORD const codePage = value >> 16;

    char query[48];
    sprintf_s(query, "\\StringFileInfo\\%04x%04x\\%ls", langId, codePage, valueName);
    if (!VerQueryValueA(buffer.get(), query, &valuePtr, &valueSize) || valueSize == 0)
        return std::nullopt;

    return static_cast<char const*>(valuePtr);
}

std::string GetFileInfoValue(wchar_t const* valueName)
{
    std::wstring dllPath;
    if (FAILED(GetModuleFileNameW(g_DllModule, dllPath)))
        return {};

    auto version = GetFileInfoValue(dllPath.c_str(), valueName);
    if (!version)
        return {};

    return *version;
}

std::string_view GetLicenseInfo()
{
    auto data = GetModuleResource(g_DllModule, MAKEINTRESOURCEW(1), L"TXT");
    return {reinterpret_cast<char const*>(data.data()), data.size()};
}

class ComponentVersionImpl : public componentversion
{
public:
    void get_file_name(pfc::string_base& out) override
    {
        out = core_api::get_my_file_name();
    }

    void get_component_name(pfc::string_base& out) override { out = "Scrobble"; }

    void get_component_version(pfc::string_base& out) override
    {
        auto version = GetFileInfoValue(L"FileVersion");
        if (version.empty())
            version = "1.0.0";
        out.set_string(version.data(), version.size());
    }

    void get_about_message(pfc::string_base& out) override
    {
        out = "foo_scrobble - Scrobbling for <http://www.last.fm/>\n"
              "\n"
              "Current version at: <https://github.com/gix/foo_scrobble>\n";

        if (auto const ver = GetFileInfoValue(L"ProductVersion"); !ver.empty()) {
            out += "Version: ";
            out.add_string(ver.data(), ver.size());
            out += "\n";
        }

        if (auto const info = GetLicenseInfo(); !info.empty()) {
            out += "\n\n";
            out.add_string(info.data(), info.size());
        }
    }
};

service_factory_single_t<ComponentVersionImpl> g_componentversion_myimpl_factory;
} // namespace
} // namespace foo_scrobble

VALIDATE_COMPONENT_FILENAME("foo_scrobble.dll")

STDAPI_(BOOL)
DllMain(_In_ HINSTANCE hinstDLL, _In_ DWORD fdwReason, _In_ LPVOID /*lpvReserved*/)
{
    if (fdwReason == DLL_PROCESS_ATTACH) {
        foo_scrobble::g_DllModule = hinstDLL;
    }

    return TRUE;
}
