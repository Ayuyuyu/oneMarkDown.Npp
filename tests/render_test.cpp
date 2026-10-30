// Renderer smoke test: verifies the md4c output without Notepad++.
#include "MarkdownRenderer.h"

#include <windows.h>
#include <cstdio>
#include <string>

int main()
{
    ::SetConsoleOutputCP(CP_UTF8);

    const std::string sample =
        "# Title\r\n"
        "\r\n"
        "Some **bold** and ~~strike~~ and `code`, plus https://example.com autolink.\r\n"
        "\r\n"
        "| a | b |\r\n"
        "|---|---|\r\n"
        "| 1 | 2 |\r\n"
        "\r\n"
        "- [x] done\r\n"
        "- [ ] todo\r\n"
        "\r\n"
        "```cpp\r\n"
        "int main() { return 0; }\r\n"
        "```\r\n"
        "\r\n"
        "> quote\r\n";

    const std::string html = MarkdownRenderer::toHtmlFragment(sample);

    if (html.empty())
    {
        std::printf("FAIL: empty html\n");
        return 1;
    }

    struct Expect
    {
        const char* token;
        const char* label;
    } expects[] = {
        { "<h1>", "heading" },
        { "<strong>", "bold" },
        { "<del>", "strikethrough" },
        { "<table>", "table" },
        { "type=\"checkbox\"", "tasklist" },
        { "<pre>", "codeblock" },
        { "<blockquote>", "blockquote" },
        { "href=\"https://example.com\"", "autolink" },
    };

    int failed = 0;
    for (const auto& e : expects)
    {
        if (html.find(e.token) == std::string::npos)
        {
            std::printf("FAIL: missing %s (%s)\n", e.label, e.token);
            ++failed;
        }
        else
        {
            std::printf("ok   %s\n", e.label);
        }
    }

    // Plain-text path (CF_UNICODETEXT fallback when copying rich text).
    const std::string plain = MarkdownRenderer::toPlainText(sample);
    const bool plainOk =
        plain.find("Title") != std::string::npos &&
        plain.find('#') == std::string::npos &&
        plain.find("**") == std::string::npos &&
        plain.find("- done") != std::string::npos;

    std::printf("%s plain-text\n", plainOk ? "ok  " : "FAIL");
    if (!plainOk)
        ++failed;

    std::printf("\n--- html (%zu bytes) ---\n%s\n", html.size(), html.c_str());
    std::printf("--- plain (%zu bytes) ---\n%s\n", plain.size(), plain.c_str());
    return failed == 0 ? 0 : 1;
}
