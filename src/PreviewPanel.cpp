#include "PreviewPanel.h"

#include <shellapi.h>
#include <wrl/event.h>

#include <string>

#include "ClipboardHelper.h"
#include "MarkdownRenderer.h"
#include "PluginDefinition.h"
#include "PluginUtil.h"
#include "PreviewAssets.h"

#include "Docking.h"
#include "Notepad_plus_msgs.h"

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

namespace
{
    const wchar_t kPanelClass[] = L"OneMarkDownNppPreviewPanel";
    const wchar_t kPanelTitle[] = L"Markdown Preview";
    const wchar_t kModuleName[] = L"OneMarkDownNpp.dll";

    constexpr UINT_PTR kRenderTimerId = 1;
    constexpr UINT_PTR kLayoutTimerId = 2;
    constexpr size_t kMaxRenderBytes = 2u * 1024u * 1024u;

    std::wstring pathToFileUrl(const std::wstring& path)
    {
        std::wstring url = L"file:///";
        for (wchar_t ch : path)
            url.push_back(ch == L'\\' ? L'/' : ch);
        return url;
    }

    bool writeTextFile(const std::wstring& path, const std::string& content)
    {
        HANDLE file = ::CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                                    CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
            return false;

        DWORD written = 0;
        const bool ok = ::WriteFile(file, content.data(), static_cast<DWORD>(content.size()), &written, nullptr) != FALSE;
        ::CloseHandle(file);
        return ok && written == content.size();
    }
}

PreviewPanel& PreviewPanel::get()
{
    static PreviewPanel instance;
    return instance;
}

bool PreviewPanel::create(HINSTANCE instance)
{
    if (hwnd_ != nullptr)
        return true;

    instance_ = instance;

    // WebView2 requires COM (STA). Notepad++ usually initializes it already; this is a fallback.
    ::OleInitialize(nullptr);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &PreviewPanel::staticWndProc;
    wc.hInstance = instance;
    wc.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = kPanelClass;
    ::RegisterClassExW(&wc);

    hwnd_ = ::CreateWindowExW(0, kPanelClass, kPanelTitle,
                              WS_OVERLAPPEDWINDOW,
                              CW_USEDEFAULT, CW_USEDEFAULT, 560, 720,
                              nppData._nppHandle, nullptr, instance, this);
    if (hwnd_ == nullptr)
        return false;

    registerDockedWindow();
    initWebView();
    return true;
}

void PreviewPanel::registerDockedWindow()
{
    ::SendMessage(nppData._nppHandle, NPPM_MODELESSDIALOG, MODELESSDIALOGADD,
                  reinterpret_cast<LPARAM>(hwnd_));

    DockedWidgetData data;
    data.hClient = hwnd_;
    data.pszName = kPanelTitle;
    data.dlgID = 0;                       // index of the toggle command in funcItems
    data.uMask = DWS_DF_CONT_RIGHT;
    data.pszModuleName = kModuleName;

    ::SendMessage(nppData._nppHandle, NPPM_DMMREGASDCKDLG, 0, reinterpret_cast<LPARAM>(&data));
}

void PreviewPanel::shutdown()
{
    if (shuttingDown_)
        return;

    shuttingDown_ = true;

    if (hwnd_ != nullptr)
    {
        ::KillTimer(hwnd_, kRenderTimerId);
        ::KillTimer(hwnd_, kLayoutTimerId);
        ::SendMessage(nppData._nppHandle, NPPM_MODELESSDIALOG, MODELESSDIALOGREMOVE,
                      reinterpret_cast<LPARAM>(hwnd_));
    }

    if (controller_)
    {
        controller_->put_IsVisible(FALSE);
        controller_->Close();
    }

    webView_.Reset();
    controller_.Reset();
    environment_.Reset();
    webViewReady_ = false;

    if (hwnd_ != nullptr)
    {
        ::DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }

    messageLabel_ = nullptr;
}

LRESULT CALLBACK PreviewPanel::staticWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    PreviewPanel* self = nullptr;

    if (msg == WM_NCCREATE)
    {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<PreviewPanel*>(cs->lpCreateParams);
        if (self != nullptr)
        {
            self->hwnd_ = hWnd;
            ::SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
    }
    else
    {
        self = reinterpret_cast<PreviewPanel*>(::GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    }

    if (self != nullptr)
        return self->wndProc(hWnd, msg, wParam, lParam);

    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

LRESULT PreviewPanel::wndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_SIZE:
        updateBounds();
        return 0;

    case WM_SHOWWINDOW:
        updateWebViewVisibility();
        if (wParam != 0)
        {
            updateBounds();
            scheduleRender(0);
        }
        return 0;

    case WM_TIMER:
        if (wParam == kRenderTimerId)
        {
            ::KillTimer(hWnd, kRenderTimerId);
            renderNow();
            return 0;
        }
        if (wParam == kLayoutTimerId)
        {
            // The docking container may not be laid out yet on the first DMMSHOW.
            ::KillTimer(hWnd, kLayoutTimerId);
            updateBounds();
            renderNow();
            return 0;
        }
        break;

    case WM_DPICHANGED:
        updateBounds();
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_CTLCOLORSTATIC:
    {
        // Keep the fallback message in sync with Notepad++ light/dark mode.
        HDC dc = reinterpret_cast<HDC>(wParam);
        static HBRUSH darkBrush = ::CreateSolidBrush(RGB(30, 30, 30));
        static HBRUSH lightBrush = ::CreateSolidBrush(RGB(255, 255, 255));
        const bool dark = nppIsDarkMode();
        ::SetTextColor(dc, dark ? RGB(230, 237, 243) : RGB(31, 35, 40));
        ::SetBkColor(dc, dark ? RGB(30, 30, 30) : RGB(255, 255, 255));
        return reinterpret_cast<LRESULT>(dark ? darkBrush : lightBrush);
    }

    default:
        break;
    }

    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

void PreviewPanel::updateBounds()
{
    if (hwnd_ == nullptr)
        return;

    RECT rc = {};
    ::GetClientRect(hwnd_, &rc);

    if (controller_)
        controller_->put_Bounds(rc);

    if (messageLabel_ != nullptr)
    {
        const int width = (rc.right - rc.left) - 24;
        const int height = (rc.bottom - rc.top) - 24;
        ::MoveWindow(messageLabel_, 12, 12, width > 0 ? width : 0, height > 0 ? height : 0, TRUE);
    }
}

void PreviewPanel::updateWebViewVisibility()
{
    if (controller_)
        controller_->put_IsVisible(isVisible() ? TRUE : FALSE);
}

void PreviewPanel::initWebView()
{
    if (webViewCreating_ || webViewReady_)
        return;

    webViewCreating_ = true;

    std::wstring dataDir = appDataDir();
    std::wstring userDataFolder = dataDir.empty() ? std::wstring() : dataDir + L"\\WebView2";

    const HRESULT hr = ::CreateCoreWebView2EnvironmentWithOptions(
        nullptr,
        userDataFolder.empty() ? nullptr : userDataFolder.c_str(),
        nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this](HRESULT result, ICoreWebView2Environment* env) -> HRESULT
            {
                if (shuttingDown_)
                    return S_OK;

                if (FAILED(result) || env == nullptr)
                {
                    enterFallbackMode(
                        L"Microsoft Edge WebView2 Runtime is not available.\n\n"
                        L"The in-panel preview is disabled, other features still work:\n"
                        L"  - Plugins > OneMarkDown++ > Open Preview in Browser\n"
                        L"  - Plugins > OneMarkDown++ > Copy as Rich Text\n\n"
                        L"Install the runtime to enable the live preview:\n"
                        L"https://developer.microsoft.com/microsoft-edge/webview2/");
                    return S_OK;
                }

                environment_ = env;
                environment_->CreateCoreWebView2Controller(
                    hwnd_,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [this](HRESULT result2, ICoreWebView2Controller* created) -> HRESULT
                        {
                            if (shuttingDown_)
                                return S_OK;

                            if (FAILED(result2) || created == nullptr)
                            {
                                enterFallbackMode(
                                    L"Failed to create the WebView2 view.\n"
                                    L"Use Plugins > OneMarkDown++ > Open Preview in Browser to see the output.");
                                return S_OK;
                            }

                            controller_ = created;
                            controller_->get_CoreWebView2(&webView_);
                            configureWebView();
                            updateBounds();
                            updateWebViewVisibility();

                            EventRegistrationToken navToken = {};
                            webView_->add_NavigationCompleted(
                                Callback<ICoreWebView2NavigationCompletedEventHandler>(
                                    [this](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs*) -> HRESULT
                                    {
                                        if (shuttingDown_)
                                            return S_OK;
                                        webViewReady_ = true;
                                        renderNow();
                                        return S_OK;
                                    }).Get(), &navToken);

                            EventRegistrationToken msgToken = {};
                            webView_->add_WebMessageReceived(
                                Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                                    [this](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT
                                    {
                                        LPWSTR text = nullptr;
                                        if (SUCCEEDED(args->TryGetWebMessageAsString(&text)) && text != nullptr)
                                        {
                                            std::wstring message(text);
                                            ::CoTaskMemFree(text);
                                            // 'U' + url : open the link in the default browser
                                            if (!message.empty() && message[0] == L'U')
                                            {
                                                const std::wstring url = message.substr(1);
                                                ::ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                                            }
                                        }
                                        return S_OK;
                                    }).Get(), &msgToken);

                            navigateShell();
                            return S_OK;
                        }).Get());
                return S_OK;
            }).Get());

    if (FAILED(hr))
    {
        enterFallbackMode(
            L"The WebView2 loader is missing or failed to initialize.\n"
            L"Use Plugins > OneMarkDown++ > Open Preview in Browser to see the output.");
    }
}

void PreviewPanel::configureWebView()
{
    if (webView_ == nullptr)
        return;

    ComPtr<ICoreWebView2Settings> settings;
    if (SUCCEEDED(webView_->get_Settings(&settings)) && settings)
    {
        settings->put_IsStatusBarEnabled(FALSE);
        settings->put_IsZoomControlEnabled(FALSE);
    }

    applyBackgroundColor(nppIsDarkMode());
}

void PreviewPanel::applyBackgroundColor(bool dark)
{
    ComPtr<ICoreWebView2Controller2> controller2;
    if (!controller_ || FAILED(controller_.As(&controller2)) || !controller2)
        return;

    COREWEBVIEW2_COLOR color = {};
    color.A = 255;
    color.R = dark ? 30 : 255;
    color.G = dark ? 30 : 255;
    color.B = dark ? 30 : 255;
    controller2->put_DefaultBackgroundColor(color);
}

void PreviewPanel::navigateShell()
{
    std::wstring dir = appDataDir();
    if (dir.empty() || webView_ == nullptr)
        return;

    const std::wstring file = dir + L"\\preview.html";
    if (!writeTextFile(file, PreviewAssets::shellHtml()))
    {
        showPanelMessage(L"Failed to write the preview page:\n%LOCALAPPDATA%\\OneMarkDownNpp\\preview.html");
        return;
    }

    webView_->Navigate(pathToFileUrl(file).c_str());
}

void PreviewPanel::enterFallbackMode(const wchar_t* message)
{
    fallback_ = true;
    showPanelMessage(message);
}

void PreviewPanel::showPanelMessage(const wchar_t* message)
{
    webViewCreating_ = false;

    if (hwnd_ == nullptr || messageLabel_ != nullptr)
        return;

    messageLabel_ = ::CreateWindowExW(0, L"STATIC", message,
                                      WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
                                      12, 12, 480, 200, hwnd_, nullptr, instance_, nullptr);
    updateBounds();
}

void PreviewPanel::postWebMessage(const std::wstring& message)
{
    if (webView_ == nullptr || !webViewReady_)
        return;

    webView_->PostWebMessageAsString(message.c_str());
}

void PreviewPanel::pushTheme()
{
    const bool dark = nppIsDarkMode();
    postWebMessage(dark ? L"T1" : L"T0");
    applyBackgroundColor(dark);
}

void PreviewPanel::pushBase()
{
    std::wstring baseUrl = nppCurrentFileBaseUrl();
    postWebMessage(L"B" + baseUrl);
}

void PreviewPanel::pushHtml(const std::wstring& html)
{
    postWebMessage(L"H" + html);
}

void PreviewPanel::scheduleRender(unsigned delayMs)
{
    if (hwnd_ == nullptr || !isVisible())
        return;

    ::KillTimer(hwnd_, kRenderTimerId);
    ::SetTimer(hwnd_, kRenderTimerId, delayMs == 0 ? 1u : delayMs, nullptr);
}

void PreviewPanel::renderNow()
{
    if (hwnd_ == nullptr || !isVisible() || fallback_)
        return;

    if (!webViewReady_)
    {
        // WebView2 is not ready yet; NavigationCompleted will trigger the render.
        return;
    }

    bool truncated = false;
    const std::string markdown = nppCurrentText(kMaxRenderBytes, &truncated);
    std::string fragment = MarkdownRenderer::toHtmlFragment(markdown);

    if (truncated)
    {
        const std::string notice =
            "<div class=\"notice\">Document is larger than 2 MB; only the beginning is previewed.</div>";
        fragment = notice + fragment;
    }

    pushTheme();
    pushBase();
    pushHtml(utf8ToWide(fragment));
}

void PreviewPanel::syncTheme()
{
    pushTheme();
    renderNow();
}

bool PreviewPanel::isVisible() const
{
    return hwnd_ != nullptr && ::IsWindowVisible(hwnd_) != FALSE;
}

void PreviewPanel::showPanel()
{
    if (hwnd_ == nullptr)
        return;

    ::SendMessage(nppData._nppHandle, NPPM_DMMSHOW, 0, reinterpret_cast<LPARAM>(hwnd_));
    updateBounds();
    updateWebViewVisibility();
    scheduleRender(0);
    ::SetTimer(hwnd_, kLayoutTimerId, 120, nullptr);
}

void PreviewPanel::hidePanel()
{
    if (hwnd_ == nullptr)
        return;

    ::SendMessage(nppData._nppHandle, NPPM_DMMHIDE, 0, reinterpret_cast<LPARAM>(hwnd_));
}

void PreviewPanel::toggle()
{
    if (hwnd_ == nullptr)
        return;

    if (isVisible())
        hidePanel();
    else
        showPanel();
}

void PreviewPanel::copyRichText()
{
    // Rendered straight from md4c, so it works even without WebView2 or an open panel.
    const std::string markdown = nppCurrentText(0, nullptr);
    if (markdown.empty())
        return;

    const std::string fragment = MarkdownRenderer::toHtmlFragment(markdown);
    const std::string plain = MarkdownRenderer::toPlainText(markdown);
    ClipboardHelper::setHtml(nppData._nppHandle, fragment, utf8ToWide(plain));
}

void PreviewPanel::openInBrowser()
{
    const std::wstring dir = appDataDir();
    if (dir.empty())
        return;

    bool truncated = false;
    const std::string markdown = nppCurrentText(kMaxRenderBytes, &truncated);
    std::string fragment = MarkdownRenderer::toHtmlFragment(markdown);

    if (truncated)
    {
        fragment = std::string("<div class=\"notice\">Document is larger than 2 MB; only the beginning is previewed.</div>") + fragment;
    }

    const std::string document = PreviewAssets::browserDocument(
        fragment, wideToUtf8(nppCurrentFileBaseUrl()), nppIsDarkMode());

    const std::wstring file = dir + L"\\browser-preview.html";
    if (!writeTextFile(file, document))
        return;

    ::ShellExecuteW(nullptr, L"open", pathToFileUrl(file).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}
