#include "PluginUtil.h"

#include "PluginDefinition.h"
#include "Notepad_plus_msgs.h"
#include "Scintilla.h"

#include <shlobj.h>

#include <vector>

namespace
{
    const wchar_t kAppFolderName[] = L"OneMarkDownNpp";

    HWND currentScintilla()
    {
        int which = 0;
        ::SendMessage(nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, reinterpret_cast<LPARAM>(&which));
        return (which == 0) ? nppData._scintillaMainHandle : nppData._scintillaSecondHandle;
    }

    std::wstring currentFilePath()
    {
        wchar_t buffer[MAX_PATH * 4] = {};
        ::SendMessage(nppData._nppHandle, NPPM_GETFULLCURRENTPATH,
                      static_cast<WPARAM>(_countof(buffer)), reinterpret_cast<LPARAM>(buffer));
        return std::wstring(buffer);
    }

    std::wstring encodeUrlPath(const std::wstring& path)
    {
        std::wstring url;
        url.reserve(path.size() + 16);
        for (wchar_t ch : path)
        {
            switch (ch)
            {
            case L'\\':
                url.push_back(L'/');
                break;
            case L' ':
                url += L"%20";
                break;
            case L'#':
                url += L"%23";
                break;
            case L'?':
                url += L"%3F";
                break;
            case L'%':
                url += L"%25";
                break;
            default:
                url.push_back(ch);
                break;
            }
        }
        return url;
    }
}

std::string nppCurrentText(size_t maxBytes, bool* truncated)
{
    if (truncated)
        *truncated = false;

    HWND sci = currentScintilla();
    if (sci == nullptr)
        return std::string();

    const LRESULT length = ::SendMessage(sci, SCI_GETLENGTH, 0, 0);
    if (length <= 0)
        return std::string();

    size_t want = static_cast<size_t>(length);
    if (maxBytes != 0 && want > maxBytes)
    {
        want = maxBytes;
        if (truncated)
            *truncated = true;
    }

    std::vector<char> buffer(want + 1, '\0');
    const LRESULT got = ::SendMessage(sci, SCI_GETTEXT, static_cast<WPARAM>(want + 1),
                                      reinterpret_cast<LPARAM>(buffer.data()));
    size_t size = (got > 0) ? static_cast<size_t>(got) : 0;
    if (size > want)
        size = want;

    // When truncated, back off to a UTF-8 character boundary to avoid broken characters.
    if (truncated && *truncated)
    {
        while (size > 0 && (static_cast<unsigned char>(buffer[size]) & 0xC0) == 0x80)
            --size;
    }

    return std::string(buffer.data(), size);
}

std::wstring nppCurrentFileBaseUrl()
{
    std::wstring path = currentFilePath();
    if (path.empty())
        return std::wstring();

    const size_t pos = path.find_last_of(L"\\/");
    if (pos != std::wstring::npos)
        path = path.substr(0, pos);

    if (path.size() < 2 || path[1] != L':')
        return std::wstring();

    return L"file:///" + encodeUrlPath(path) + L"/";
}

bool nppIsDarkMode()
{
    return ::SendMessage(nppData._nppHandle, NPPM_ISDARKMODEENABLED, 0, 0) == TRUE;
}

std::wstring appDataDir()
{
    wchar_t buffer[MAX_PATH] = {};
    if (FAILED(::SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, buffer)))
        return std::wstring();

    std::wstring dir = buffer;
    dir += L"\\";
    dir += kAppFolderName;
    ::CreateDirectoryW(dir.c_str(), nullptr);
    return dir;
}

std::wstring utf8ToWide(const std::string& text)
{
    if (text.empty())
        return std::wstring();

    const int size = ::MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (size <= 0)
        return std::wstring();

    std::wstring out(static_cast<size_t>(size), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), size);
    return out;
}

std::string wideToUtf8(const std::wstring& text)
{
    if (text.empty())
        return std::string();

    const int size = ::WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                                           nullptr, 0, nullptr, nullptr);
    if (size <= 0)
        return std::string();

    std::string out(static_cast<size_t>(size), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                          out.data(), size, nullptr, nullptr);
    return out;
}
