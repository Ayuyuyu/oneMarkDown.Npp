#pragma once

#include <windows.h>

#include "PluginInterface.h"

extern NppData nppData;
extern HINSTANCE g_instance;

void pluginCleanUp();

void cmdTogglePreview();
void cmdCopyRichText();
void cmdCopyHtmlSource();
void cmdRefreshPreview();
void cmdOpenInBrowser();
void cmdAbout();
