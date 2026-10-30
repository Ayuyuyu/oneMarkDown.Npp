#pragma once

#include <windows.h>
#include <string>

namespace ClipboardHelper
{
    // Puts CF_HTML (rich text) plus CF_UNICODETEXT on the clipboard.
    bool setHtml(HWND owner, const std::string& htmlFragmentUtf8, const std::wstring& plainText);

    bool setText(HWND owner, const std::wstring& text);
}
