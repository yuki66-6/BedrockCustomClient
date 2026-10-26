#pragma once

// Configuration ImGui propre au projet (injectée via IMGUI_USER_CONFIG).
void BccImGuiAssert(const char* expr, const char* file, int line);
#define IM_ASSERT(_EXPR) ((_EXPR) ? (void)0 : BccImGuiAssert(#_EXPR, __FILE__, __LINE__))
