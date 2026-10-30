#include "PluginDefinition.h"

#include <string>

#include "ClipboardHelper.h"
#include "MarkdownRenderer.h"
#include "PluginUtil.h"
#include "PreviewPanel.h"

#include "Notepad_plus_msgs.h"

NppData nppData = {};
HINSTANCE g_instance = nullptr;

namespace
{
    const wchar_t kPluginName[] = L"OneMarkDown++";

    constexpr unsigned kTypingDebounceMs = 220;
    constexpr unsigned kSwitchDebounceMs = 30;

    // No default shortcuts; users can bind them in Settings > Shortcut Mapper.
    FuncItem funcItems[] = {
        { L"Show/Hide Preview Panel", cmdTogglePreview, 0, false, nullptr },
        { L"Copy as Rich Text", cmdCopyRichText, 0, false, nullptr },
        { L"Copy as HTML Source", cmdCopyHtmlSource, 0, false, nullptr },
        { L"Refresh Preview", cmdRefreshPreview, 0, false, nullptr },
        { L"Open Preview in Browser", cmdOpenInBrowser, 0, false, nullptr },
        { L"", nullptr, 0, false, nullptr },
        { L"About", cmdAbout, 0, false, nullptr },
    };

    void showInfoBox(const wchar_t* text)
    {
        ::MessageBoxW(nppData._nppHandle, text, kPluginName, MB_OK | MB_ICONINFORMATION);
    }
}

void pluginCleanUp()
{
    PreviewPanel::get().shutdown();
}

void cmdTogglePreview()
{
    PreviewPanel& panel = PreviewPanel::get();

    // NPPM_DMMREGASDCKDLG shows the panel immediately, so the first call must not
    // go through toggle() (it would treat the panel as visible and hide it).
    if (panel.hwnd() == nullptr)
    {
        panel.create(g_instance);
        panel.showPanel();
        return;
    }

    panel.toggle();
}

void cmdCopyRichText()
{
    PreviewPanel::get().copyRichText();
}

void cmdCopyHtmlSource()
{
    const std::string markdown = nppCurrentText(0, nullptr);
    const std::string html = MarkdownRenderer::toHtmlDocument(markdown);
    ClipboardHelper::setText(nppData._nppHandle, utf8ToWide(html));
}

void cmdRefreshPreview()
{
    PreviewPanel::get().renderNow();
}

void cmdOpenInBrowser()
{
    PreviewPanel::get().openInBrowser();
}

void cmdAbout()
{
    showInfoBox(
        L"OneMarkDown++ 0.1.0\n\n"
        L"Markdown preview plugin for Notepad++.\n"
        L"Parsing: md4c (CommonMark + GFM)\n"
        L"Rendering: WebView2\n\n"
        L"All commands are under Plugins > OneMarkDown++.\n"
        L"");
}

extern "C" __declspec(dllexport) void setInfo(NppData data)
{
    nppData = data;
}

extern "C" __declspec(dllexport) const wchar_t* getName()
{
    return kPluginName;
}

extern "C" __declspec(dllexport) FuncItem* getFuncsArray(int* nbF)
{
    *nbF = static_cast<int>(_countof(funcItems));
    return funcItems;
}

extern "C" __declspec(dllexport) void beNotified(SCNotification* notifyCode)
{
    if (notifyCode == nullptr)
        return;

    PreviewPanel& panel = PreviewPanel::get();

    switch (notifyCode->nmhdr.code)
    {
    case SCN_MODIFIED:
        if ((notifyCode->modificationType & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT)) != 0)
            panel.scheduleRender(kTypingDebounceMs);
        break;

    case NPPN_BUFFERACTIVATED:
    case NPPN_FILEOPENED:
    case NPPN_FILESAVED:
    case NPPN_FILERENAMED:
        panel.scheduleRender(kSwitchDebounceMs);
        break;

    case NPPN_DARKMODECHANGED:
        panel.syncTheme();
        break;

    case NPPN_SHUTDOWN:
        pluginCleanUp();
        break;

    default:
        break;
    }
}

extern "C" __declspec(dllexport) LRESULT messageProc(UINT /*message*/, WPARAM /*wParam*/, LPARAM /*lParam*/)
{
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL isUnicode()
{
    return TRUE;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID /*reserved*/)
{
    if (reason == DLL_PROCESS_ATTACH)
        g_instance = reinterpret_cast<HINSTANCE>(hModule);

    return TRUE;
}
