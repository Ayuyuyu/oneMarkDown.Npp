#pragma once

#include <string>

// Front-end assets of the preview page.
// styles()         - shared CSS for light/dark themes (CSS variables)
// shellHtml()      - page loaded by WebView2 inside the panel (message-driven JS)
// browserDocument  - standalone HTML exported to the default browser (no JS)
namespace PreviewAssets
{
    inline const char* styles()
    {
        return R"CSS(
  :root {
    --fg: #1f2328;
    --fg-muted: #59636e;
    --bg: #ffffff;
    --bg-subtle: #f6f8fa;
    --border: #d1d9e0;
    --link: #0969da;
    --code-bg: #f6f8fa;
    --quote-bar: #d1d9e0;
    --mark: #fff8c5;
  }
  html[data-theme="dark"] {
    --fg: #e6edf3;
    --fg-muted: #9198a1;
    --bg: #1e1e1e;
    --bg-subtle: #262626;
    --border: #3d444d;
    --link: #4493f8;
    --code-bg: #2b2b2b;
    --quote-bar: #3d444d;
    --mark: #4b3b00;
  }
  * { box-sizing: border-box; }
  body {
    margin: 0;
    padding: 24px 28px 80px;
    background: var(--bg);
    color: var(--fg);
    font-family: "Segoe UI", "Microsoft YaHei", "PingFang SC", -apple-system, BlinkMacSystemFont, Helvetica, Arial, sans-serif;
    font-size: 15px;
    line-height: 1.65;
    -webkit-font-smoothing: antialiased;
    word-wrap: break-word;
  }
  #md > *:first-child { margin-top: 0 !important; }
  #md > *:last-child { margin-bottom: 0 !important; }

  h1, h2, h3, h4, h5, h6 { margin: 1.4em 0 0.6em; font-weight: 600; line-height: 1.3; }
  h1 { font-size: 1.9em; padding-bottom: .3em; border-bottom: 1px solid var(--border); }
  h2 { font-size: 1.5em; padding-bottom: .3em; border-bottom: 1px solid var(--border); }
  h3 { font-size: 1.25em; }
  h4 { font-size: 1.05em; }
  h5, h6 { font-size: 1em; color: var(--fg-muted); }

  p, ul, ol, dl, table, pre, blockquote, details { margin: 0 0 1em; }
  ul, ol { padding-left: 2em; }
  li + li { margin-top: .25em; }

  a { color: var(--link); text-decoration: none; cursor: pointer; }
  a:hover { text-decoration: underline; }

  img { max-width: 100%; height: auto; border-radius: 4px; }

  hr { height: 1px; border: 0; background: var(--border); margin: 1.8em 0; }

  blockquote {
    padding: .2em 1em;
    color: var(--fg-muted);
    border-left: 4px solid var(--quote-bar);
    margin-left: 0;
    background: var(--bg-subtle);
    border-radius: 0 4px 4px 0;
  }
  blockquote > :last-child { margin-bottom: 0; }

  code, kbd, samp {
    font-family: "Cascadia Mono", Consolas, "Courier New", monospace;
    font-size: .88em;
  }
  :not(pre) > code {
    padding: .18em .4em;
    background: var(--code-bg);
    border-radius: 5px;
    border: 1px solid var(--border);
  }
  pre {
    padding: 14px 16px;
    overflow: auto;
    background: var(--code-bg);
    border: 1px solid var(--border);
    border-radius: 8px;
    line-height: 1.5;
  }
  pre code { padding: 0; background: none; border: 0; font-size: .88em; white-space: pre; }

  table { border-collapse: collapse; margin-bottom: 1em; max-width: 100%; }
  table th, table td { padding: 6px 13px; border: 1px solid var(--border); }
  table th { font-weight: 600; background: var(--bg-subtle); }
  table tr:nth-child(2n) td { background: var(--bg-subtle); }

  del { color: var(--fg-muted); }
  mark { background: var(--mark); }

  .task-list-item { list-style: none; }
  .task-list-item-checkbox { margin: 0 .5em .18em -1.4em; vertical-align: middle; }

  .placeholder { color: var(--fg-muted); font-style: italic; }
  .notice {
    margin-bottom: 1em;
    padding: 8px 12px;
    border-radius: 6px;
    background: var(--mark);
    font-size: .9em;
  }
)CSS";
    }

    // Page used by the WebView2 panel: loaded once, #md is updated via postMessage.
    inline std::string shellHtml()
    {
        std::string html;
        html.reserve(16000);
        html += R"SHELL(<!DOCTYPE html>
<html data-theme="light">
<head>
<meta charset="utf-8">
<base id="baseTag" href="">
<style>
)SHELL";
        html += styles();
        html += R"SHELL(
</style>
</head>
<body>
<div id="md"><p class="placeholder">(empty document)</p></div>
<script>
  var mdRoot = document.getElementById('md');
  var baseTag = document.getElementById('baseTag');

  function applyTheme(isDark) {
    document.documentElement.setAttribute('data-theme', isDark ? 'dark' : 'light');
  }

  function setDocumentBase(url) {
    if (baseTag && url) baseTag.href = url;
  }

  function decorate() {
    var boxes = mdRoot.querySelectorAll('input[type="checkbox"]');
    for (var i = 0; i < boxes.length; i++) boxes[i].disabled = true;
  }

  function render(html) {
    var y = window.scrollY;
    if (!html) {
      mdRoot.innerHTML = '<p class="placeholder">(empty document)</p>';
    } else {
      mdRoot.innerHTML = html;
    }
    decorate();
    window.scrollTo(0, y);
  }

  document.addEventListener('click', function (e) {
    var node = e.target;
    while (node && node !== document.body && node.tagName !== 'A') node = node.parentNode;
    if (!node || node.tagName !== 'A') return;
    if (!node.getAttribute('href')) return;
    e.preventDefault();
    if (window.chrome && window.chrome.webview) {
      window.chrome.webview.postMessage('U' + node.href);
    }
  });

  if (window.chrome && window.chrome.webview) {
    window.chrome.webview.addEventListener('message', function (e) {
      var msg = e.data;
      if (!msg) return;
      var key = msg.charAt(0);
      var value = msg.substring(1);
      if (key === 'T') {
        applyTheme(value === '1');
      } else if (key === 'B') {
        setDocumentBase(value);
      } else if (key === 'H') {
        render(value);
      }
    });
  }
</script>
</body>
</html>
)SHELL";
        return html;
    }

    // Standalone document opened in the default browser (no JS, refresh with F5).
    inline std::string browserDocument(const std::string& bodyHtml,
                                       const std::string& baseUrlUtf8,
                                       bool dark)
    {
        std::string html;
        html.reserve(bodyHtml.size() + 12000);
        html += "<!DOCTYPE html>\n<html data-theme=\"";
        html += dark ? "dark" : "light";
        html += "\">\n<head>\n<meta charset=\"utf-8\">\n";
        if (!baseUrlUtf8.empty())
        {
            html += "<base href=\"";
            html += baseUrlUtf8;
            html += "\">\n";
        }
        html += "<title>OneMarkDown++ Preview</title>\n<style>";
        html += styles();
        html += "\n</style>\n</head>\n<body>\n<div id=\"md\">\n";
        html += bodyHtml;
        html += "\n</div>\n</body>\n</html>\n";
        return html;
    }
}
