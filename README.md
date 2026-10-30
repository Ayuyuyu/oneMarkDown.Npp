# OneMarkDown++

Markdown live-preview plugin for Notepad++ (C++).

- Parser: [md4c](https://github.com/mity/md4c) — CommonMark + GFM
- Renderer: WebView2 (Microsoft Edge Chromium)
- No default shortcuts — bind them yourself in Settings > Shortcut Mapper

## Features

- Live preview in a dockable panel (220 ms debounce while typing)
- **Copy as rich text** (CF_HTML) — paste into Word / Outlook with formatting; works without WebView2
- Copy as HTML source
- Open preview in the default browser (fallback when WebView2 is unavailable)
- Follows Notepad++ light/dark theme; relative image paths resolve against the file's folder

## Requirements

- Windows 10/11 x64
- Notepad++ 8.x x64
- Microsoft Edge WebView2 Runtime (bundled with Win11; on Win10 it comes with Edge updates)

## Build

Visual Studio 2022 (MSVC v143) + CMake 3.20+.

```powershell
pwsh -File scripts\build.ps1
# or
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
```

Output: `bin\OneMarkDownNpp.dll` — static CRT, depends on system DLLs only.
Renderer smoke test: `bin\md_render_test.exe`.

The first configure downloads the WebView2 SDK (~11 MB) from NuGet into `build/webview2`,
so the repository only keeps small sources. Overrides:

```powershell
# offline / mirrored SDK (extracted NuGet package)
cmake -S . -B build -DWEBVIEW2_SDK_DIR=D:\sdk\webview2

# pin another version or use a mirror
cmake -S . -B build -DWEBVIEW2_VERSION=1.0.4314.35 -DWEBVIEW2_NUGET_URL=<url>
```

## Install

```powershell

Target: eg. `C:\Program Files\Notepad++\plugins\OneMarkDownNpp\OneMarkDownNpp.dll`.
The plugin folder must match the DLL name. Close Notepad++ before installing —
the DLL cannot be replaced while it is loaded.
```
## Commands (Plugins > OneMarkDown++)

| Command | Description |
| --- | --- |
| Show/Hide Preview Panel | Show or hide the docked preview panel |
| Copy as Rich Text | Rendered content as CF_HTML + plain text |
| Copy as HTML Source | Complete HTML document as text |
| Refresh Preview | Re-render immediately, skipping the debounce |
| Open Preview in Browser | Export a standalone HTML file and open it with the default browser |

## Notes

- Without WebView2 the panel shows a hint instead of the preview; the copy commands
  and browser preview keep working.
- No log files. Runtime artifacts only: `%LOCALAPPDATA%\OneMarkDownNpp\preview.html`
  and `browser-preview.html`.
- Plugins Admin downloads from the official `nppPluginList` only and does not scan
  local DLLs; being listed there requires publishing x64 releases and a PR to that repo.

## Project layout
```
src/
  PluginDefinition.cpp   Npp entry points, menu commands, notification dispatch
  PreviewPanel.cpp       Dockable panel, WebView2 host, message push
  PreviewAssets.h        HTML/CSS/JS assets (styles, panel shell, browser export)
  MarkdownRenderer.cpp   md4c wrapper: Markdown -> HTML / plain text
  ClipboardHelper.cpp    CF_HTML + CF_UNICODETEXT
  PluginUtil.cpp         Document text, paths, dark mode, UTF-8 helpers
third_party/             md4c + Notepad++ headers (WebView2 SDK: fetched at configure time)
tests/render_test.cpp    Renderer smoke test
scripts/                 build.ps1
```
## License

- `third_party/md4c`: MIT
- `third_party/npp` (Notepad++ headers): GPLv3
- WebView2 SDK (downloaded at build time): Microsoft WebView2 SDK license

---


