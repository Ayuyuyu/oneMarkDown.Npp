#pragma once

#include <windows.h>
#include <string>

#include <wrl/client.h>
#include "WebView2.h"

// Markdown live-preview panel: a dockable window hosting WebView2.
class PreviewPanel
{
public:
    static PreviewPanel& get();

    bool create(HINSTANCE instance);
    void shutdown();

    HWND hwnd() const { return hwnd_; }
    bool isVisible() const;

    void toggle();
    void showPanel();
    void hidePanel();

    // Debounced refresh (a few hundred ms while typing).
    void scheduleRender(unsigned delayMs = 220);
    void renderNow();

    void syncTheme();

    // Puts the rendered document on the clipboard as rich text.
    void copyRichText();

    // Export a standalone HTML file and open it with the default browser.
    // Works without WebView2.
    void openInBrowser();

private:
    PreviewPanel() = default;
    ~PreviewPanel() = default;
    PreviewPanel(const PreviewPanel&) = delete;
    PreviewPanel& operator=(const PreviewPanel&) = delete;

    static LRESULT CALLBACK staticWndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT wndProc(HWND, UINT, WPARAM, LPARAM);

    void registerDockedWindow();
    void initWebView();
    void configureWebView();
    void applyBackgroundColor(bool dark);
    void updateBounds();
    void updateWebViewVisibility();
    void navigateShell();

    void pushTheme();
    void pushBase();
    void pushHtml(const std::wstring& html);
    void postWebMessage(const std::wstring& message);

    void showPanelMessage(const wchar_t* message);
    void enterFallbackMode(const wchar_t* message);

private:
    HWND hwnd_ = nullptr;
    HINSTANCE instance_ = nullptr;

    Microsoft::WRL::ComPtr<ICoreWebView2Environment> environment_;
    Microsoft::WRL::ComPtr<ICoreWebView2Controller> controller_;
    Microsoft::WRL::ComPtr<ICoreWebView2> webView_;

    bool webViewReady_ = false;
    bool webViewCreating_ = false;
    bool shuttingDown_ = false;
    bool fallback_ = false;

    HWND messageLabel_ = nullptr;
};
