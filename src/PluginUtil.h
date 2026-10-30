#pragma once

#include <windows.h>
#include <string>

// Reads the whole current document as UTF-8.
// maxBytes = 0 means no limit; truncation keeps UTF-8 character boundaries.
std::string nppCurrentText(size_t maxBytes, bool* truncated = nullptr);

// file:/// URL of the current file directory (trailing slash),
// used as <base> so relative image paths resolve in the preview.
std::wstring nppCurrentFileBaseUrl();

bool nppIsDarkMode();

// %LOCALAPPDATA%\OneMarkDownNpp (created if missing).
std::wstring appDataDir();

std::wstring utf8ToWide(const std::string& text);
std::string wideToUtf8(const std::wstring& text);
