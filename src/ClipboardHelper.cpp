#include "ClipboardHelper.h"

#include <cstdio>
#include <cstring>

namespace
{
    const wchar_t* kHtmlFormatName = L"HTML Format";

    // Builds CF_HTML. Offsets are byte offsets in UTF-8, always 10 decimal digits.
    std::string buildCfHtml(const std::string& fragment)
    {
        const std::string prefix = "<html><head><meta charset=\"utf-8\"></head><body>\r\n<!--StartFragment-->";
        const std::string suffix = "<!--EndFragment-->\r\n</body></html>";

        char probe[256] = {};
        const int headerLen = std::snprintf(probe, sizeof(probe),
            "Version:0.9\r\n"
            "StartHTML:%010u\r\n"
            "EndHTML:%010u\r\n"
            "StartFragment:%010u\r\n"
            "EndFragment:%010u\r\n",
            0u, 0u, 0u, 0u);
        if (headerLen <= 0)
            return std::string();

        const unsigned startHtml = static_cast<unsigned>(headerLen);
        const unsigned startFragment = startHtml + static_cast<unsigned>(prefix.size());
        const unsigned endFragment = startFragment + static_cast<unsigned>(fragment.size());
        const unsigned endHtml = endFragment + static_cast<unsigned>(suffix.size());

        char header[256] = {};
        std::snprintf(header, sizeof(header),
            "Version:0.9\r\n"
            "StartHTML:%010u\r\n"
            "EndHTML:%010u\r\n"
            "StartFragment:%010u\r\n"
            "EndFragment:%010u\r\n",
            startHtml, endHtml, startFragment, endFragment);

        std::string out;
        out.reserve(static_cast<size_t>(headerLen) + prefix.size() + fragment.size() + suffix.size());
        out.append(header, static_cast<size_t>(headerLen));
        out += prefix;
        out += fragment;
        out += suffix;
        return out;
    }

    bool putBytes(UINT format, const void* data, size_t bytes)
    {
        if (bytes == 0)
            return false;

        HGLOBAL handle = ::GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (handle == nullptr)
            return false;

        void* dest = ::GlobalLock(handle);
        if (dest == nullptr)
        {
            ::GlobalFree(handle);
            return false;
        }
        std::memcpy(dest, data, bytes);
        ::GlobalUnlock(handle);

        if (::SetClipboardData(format, handle) == nullptr)
        {
            ::GlobalFree(handle);
            return false;
        }
        return true;
    }
}

namespace ClipboardHelper
{
    bool setHtml(HWND owner, const std::string& htmlFragmentUtf8, const std::wstring& plainText)
    {
        const std::string cfHtml = buildCfHtml(htmlFragmentUtf8);

        if (!::OpenClipboard(owner))
            return false;

        ::EmptyClipboard();

        bool ok = false;
        const UINT htmlFormat = ::RegisterClipboardFormatW(kHtmlFormatName);
        if (htmlFormat != 0 && putBytes(htmlFormat, cfHtml.data(), cfHtml.size()))
            ok = true;

        if (!plainText.empty() &&
            putBytes(CF_UNICODETEXT, plainText.c_str(), (plainText.size() + 1) * sizeof(wchar_t)))
        {
            ok = true;
        }

        ::CloseClipboard();
        return ok;
    }

    bool setText(HWND owner, const std::wstring& text)
    {
        if (!::OpenClipboard(owner))
            return false;

        ::EmptyClipboard();
        const bool ok = putBytes(CF_UNICODETEXT, text.c_str(), (text.size() + 1) * sizeof(wchar_t));
        ::CloseClipboard();
        return ok;
    }
}
