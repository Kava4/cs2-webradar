#include "pch.hpp"
#include "tray.hpp"
#include "launcher.hpp"
#include "launcher_gui.hpp"

#include <shellapi.h>
#pragma comment(lib, "shell32.lib")

#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_OPEN      1001
#define ID_TRAY_EXIT      1002
#define ID_TRAY_LAUNCHER  1003
#define TRAY_UID      1

namespace tray
{
	static HWND     g_hwnd       = nullptr;
	static HICON    g_icon       = nullptr;
	static bool     g_added      = false;
	static bool     g_owns_icon  = false;

	static void show_context_menu()
	{
		POINT cursor{};
		GetCursorPos(&cursor);

		HMENU menu = CreatePopupMenu();
		AppendMenuW(menu, MF_STRING, ID_TRAY_OPEN, L"Open Radar");
		AppendMenuW(menu, MF_STRING, ID_TRAY_LAUNCHER, L"Show Launcher");
		AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
		AppendMenuW(menu, MF_STRING, ID_TRAY_EXIT, L"Exit");

		SetForegroundWindow(g_hwnd);
		TrackPopupMenu(menu, TPM_RIGHTALIGN | TPM_BOTTOMALIGN, cursor.x, cursor.y, 0, g_hwnd, nullptr);
		DestroyMenu(menu);
	}

	static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
	{
		switch (msg)
		{
		case WM_TRAYICON:
			if (lparam == WM_RBUTTONUP)
				show_context_menu();
			else if (lparam == WM_LBUTTONDBLCLK)
				launcher::open_browser_async();
			break;

		case WM_COMMAND:
			switch (LOWORD(wparam))
			{
			case ID_TRAY_OPEN:
				launcher::open_browser_async();
				break;
			case ID_TRAY_LAUNCHER:
				launcher_gui::show();
				break;
			case ID_TRAY_EXIT:
				launcher_gui::request_shutdown();
				break;
			}
			break;

		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		}

		return DefWindowProcW(hwnd, msg, wparam, lparam);
	}

	bool init(HINSTANCE instance, HICON icon)
	{
		const wchar_t* cls = L"AImSyncWebRadarTray";

		WNDCLASSEXW wc{};
		wc.cbSize = sizeof(wc);
		wc.lpfnWndProc = wnd_proc;
		wc.hInstance = instance;
		wc.lpszClassName = cls;
		RegisterClassExW(&wc);

		g_hwnd = CreateWindowExW(0, cls, L"AimSync WebRadar", 0,
			0, 0, 0, 0, HWND_MESSAGE, nullptr, instance, nullptr);
		if (!g_hwnd)
			return false;

		if (icon)
		{
			g_icon = CopyIcon(icon);
			g_owns_icon = g_icon != nullptr;
		}
		else
		{
			g_icon = LoadIconW(nullptr, MAKEINTRESOURCEW(IDI_APPLICATION));
			g_owns_icon = false;
		}

		NOTIFYICONDATAW nid{};
		nid.cbSize = sizeof(nid);
		nid.hWnd = g_hwnd;
		nid.uID = TRAY_UID;
		nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
		nid.uCallbackMessage = WM_TRAYICON;
		nid.hIcon = g_icon;
		wcscpy_s(nid.szTip, std::size(nid.szTip), L"AimSync WebRadar - right-click to exit");

		g_added = Shell_NotifyIconW(NIM_ADD, &nid) != FALSE;
		return g_added;
	}

	void shutdown()
	{
		if (g_owns_icon && g_icon)
		{
			DestroyIcon(g_icon);
			g_icon = nullptr;
			g_owns_icon = false;
		}

		if (g_added)
		{
			NOTIFYICONDATAW nid{};
			nid.cbSize = sizeof(nid);
			nid.hWnd = g_hwnd;
			nid.uID = TRAY_UID;
			Shell_NotifyIconW(NIM_DELETE, &nid);
			g_added = false;
		}

		if (g_hwnd)
		{
			DestroyWindow(g_hwnd);
			g_hwnd = nullptr;
		}
	}

	int run_message_loop()
	{
		MSG msg{};
		while (GetMessageW(&msg, nullptr, 0, 0))
		{
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
		return static_cast<int>(msg.wParam);
	}
}
