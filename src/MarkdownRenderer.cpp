#include "MarkdownRenderer.h"

#include "md4c-html.h"

#include <cctype>
#include <string>

namespace
{
    void appendOutput(const MD_CHAR* text, MD_SIZE size, void* userdata)
    {
        static_cast<std::string*>(userdata)->append(text, size);
    }

    // Normalizes CRLF / CR to LF, otherwise md4c sees the whole file as one line.
    std::string normalizeNewlines(const std::string& in)
    {
        if (in.find('\r') == std::string::npos)
            return in;

        std::string out;
        out.reserve(in.size());
        for (size_t i = 0; i < in.size(); ++i)
        {
            if (in[i] == '\r')
            {
                out.push_back('\n');
                if (i + 1 < in.size() && in[i + 1] == '\n')
                    ++i;
            }
            else
            {
                out.push_back(in[i]);
            }
        }
        return out;
    }

    bool isParagraphTag(const std::string& name)
    {
        static const char* kTags[] = {
            "p", "blockquote", "pre", "table", "ul", "ol",
            "h1", "h2", "h3", "h4", "h5", "h6"
        };
        for (const char* tag : kTags)
        {
            if (name == tag)
                return true;
        }
        return false;
    }

    bool isBlockTag(const std::string& name)
    {
        static const char* kTags[] = {
            "div", "section", "article", "details", "summary", "thead", "tbody"
        };
        for (const char* tag : kTags)
        {
            if (name == tag)
                return true;
        }
        return false;
    }

    size_t trailingNewlines(const std::string& text)
    {
        size_t count = 0;
        for (size_t k = text.size(); k > 0 && text[k - 1] == '\n'; --k)
            ++count;
        return count;
    }

    void appendNewlines(std::string& out, size_t count)
    {
        const size_t existing = trailingNewlines(out);
        if (existing < count)
            out.append(count - existing, '\n');
    }

    // Decodes entities such as &amp;, &#39; or &#x4e2d; and appends the result to out.
    void decodeEntity(const std::string& html, size_t& index, std::string& out)
    {
        const size_t end = html.find(';', index);
        if (end == std::string::npos || end - index > 10)
        {
            out.push_back('&');
            return;
        }

        const std::string entity = html.substr(index + 1, end - index - 1);
        if (entity == "amp") { out.push_back('&'); }
        else if (entity == "lt") { out.push_back('<'); }
        else if (entity == "gt") { out.push_back('>'); }
        else if (entity == "quot") { out.push_back('"'); }
        else if (entity == "apos" || entity == "#39") { out.push_back('\''); }
        else if (entity == "nbsp" || entity == "#160") { out.push_back(' '); }
        else if (entity == "hellip") { out += "..."; }
        else if (entity == "mdash") { out += "-"; }
        else if (entity == "ndash") { out += "-"; }
        else if (!entity.empty() && (entity[0] == '#'))
        {
            unsigned long code = 0;
            bool ok = false;
            if (entity.size() > 2 && (entity[1] == 'x' || entity[1] == 'X'))
            {
                for (size_t i = 2; i < entity.size(); ++i)
                {
                    if (!std::isxdigit(static_cast<unsigned char>(entity[i])))
                    {
                        ok = false;
                        break;
                    }
                    ok = true;
                    const char ch = static_cast<char>(std::tolower(static_cast<unsigned char>(entity[i])));
                    code = code * 16 + static_cast<unsigned long>(std::isdigit(static_cast<unsigned char>(ch)) ? ch - '0' : ch - 'a' + 10);
                }
            }
            else
            {
                for (size_t i = 1; i < entity.size(); ++i)
                {
                    if (!std::isdigit(static_cast<unsigned char>(entity[i])))
                    {
                        ok = false;
                        break;
                    }
                    ok = true;
                    code = code * 10 + static_cast<unsigned long>(entity[i] - '0');
                }
            }

            if (ok && code > 0)
            {
                // Encode the code point as UTF-8.
                if (code < 0x80)
                {
                    out.push_back(static_cast<char>(code));
                }
                else if (code < 0x800)
                {
                    out.push_back(static_cast<char>(0xC0 | (code >> 6)));
                    out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                }
                else if (code < 0x10000)
                {
                    out.push_back(static_cast<char>(0xE0 | (code >> 12)));
                    out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                    out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                }
                else
                {
                    out.push_back(static_cast<char>(0xF0 | (code >> 18)));
                    out.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3F)));
                    out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                    out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                }
                index = end;
                return;
            }
            out.push_back('&');
            return;
        }
        else
        {
            out.push_back('&');
            return;
        }

        index = end;
    }
}

namespace MarkdownRenderer
{
    std::string toHtmlFragment(const std::string& markdownUtf8)
    {
        std::string source = normalizeNewlines(markdownUtf8);
        if (source.empty())
            return std::string();

        // GFM dialect: tables, strikethrough, task lists, permissive autolinks.
        const unsigned parserFlags = MD_DIALECT_GITHUB;
        const unsigned rendererFlags = MD_HTML_FLAG_SKIP_UTF8_BOM;

        std::string html;
        html.reserve(source.size() * 2);

        const int rc = md_html(source.data(), static_cast<MD_SIZE>(source.size()),
                               appendOutput, &html, parserFlags, rendererFlags);
        if (rc != 0)
            return std::string();

        return html;
    }

    std::string toPlainText(const std::string& markdownUtf8)
    {
        const std::string html = toHtmlFragment(markdownUtf8);
        if (html.empty())
            return std::string();

        std::string out;
        out.reserve(html.size());

        bool inTag = false;
        bool inPre = false;
        std::string tag;

        for (size_t i = 0; i < html.size(); ++i)
        {
            const char ch = html[i];

            if (inTag)
            {
                if (ch != '>')
                {
                    tag.push_back(ch);
                    continue;
                }

                inTag = false;
                const bool closing = !tag.empty() && tag[0] == '/';
                std::string name;
                for (size_t k = closing ? 1 : 0; k < tag.size(); ++k)
                {
                    const char c = tag[k];
                    if (std::isalnum(static_cast<unsigned char>(c)))
                        name.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
                    else
                        break;
                }

                if (name == "br")
                {
                    appendNewlines(out, trailingNewlines(out) + 1);
                }
                else if (name == "td" || name == "th")
                {
                    if (closing)
                        out.push_back('\t');
                }
                else if (name == "tr")
                {
                    if (closing)
                    {
                        if (!out.empty() && out.back() == '\t')
                            out.pop_back();
                        appendNewlines(out, 1);
                    }
                }
                else if (name == "li")
                {
                    appendNewlines(out, 1);
                    if (!closing)
                        out += "- ";
                }
                else if (name == "pre")
                {
                    inPre = !closing;
                    appendNewlines(out, closing ? 2 : 1);
                }
                else if (isParagraphTag(name))
                {
                    appendNewlines(out, closing ? 2 : 1);
                }
                else if (isBlockTag(name))
                {
                    appendNewlines(out, 1);
                }
                continue;
            }

            if (ch == '<')
            {
                inTag = true;
                tag.clear();
                continue;
            }

            if (ch == '&')
            {
                decodeEntity(html, i, out);
                continue;
            }

            // Newlines between tags are pure HTML formatting and must not leak into
            // plain text; whitespace inside <pre> is kept verbatim.
            if (!inPre && (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n'))
            {
                size_t k = i;
                bool hasNewline = false;
                while (k < html.size())
                {
                    const char c = html[k];
                    if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
                        break;
                    if (c == '\n')
                        hasNewline = true;
                    ++k;
                }
                i = k - 1;

                if (!hasNewline && !out.empty() && out.back() != '\n' && out.back() != ' ')
                    out.push_back(' ');
                continue;
            }

            out.push_back(ch);
        }

        // Trim leading and trailing whitespace.
        while (!out.empty() && (out.back() == '\n' || out.back() == ' ' || out.back() == '\t'))
            out.pop_back();

        size_t begin = 0;
        while (begin < out.size() && (out[begin] == '\n' || out[begin] == ' ' || out[begin] == '\t'))
            ++begin;
        out.erase(0, begin);

        return out;
    }

    std::string toHtmlDocument(const std::string& markdownUtf8)
    {
        std::string body = toHtmlFragment(markdownUtf8);
        std::string doc;
        doc += "<!DOCTYPE html><html><head><meta charset=\"utf-8\"></head><body>\n";
        doc += body;
        doc += "\n</body></html>";
        return doc;
    }
}
