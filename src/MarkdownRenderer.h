#pragma once

#include <string>

namespace MarkdownRenderer
{
    // Markdown (UTF-8) -> HTML fragment (UTF-8), without <html>/<body>.
    std::string toHtmlFragment(const std::string& markdownUtf8);

    // Markdown -> plain text, derived from the HTML fragment.
    // Used as the CF_UNICODETEXT fallback when copying rich text.
    std::string toPlainText(const std::string& markdownUtf8);

    // Markdown -> complete HTML document.
    std::string toHtmlDocument(const std::string& markdownUtf8);
}
