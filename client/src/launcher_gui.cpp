#include "pch.hpp"
#include "../Embed.hpp"
#include "../resource_ids.h"
#include "launcher_gui.hpp"
#include "launcher.hpp"
#include "overlay/overlay.hpp"
#include "tray.hpp"

#include <objidl.h>
#include <gdiplus.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <algorithm>
#include <vector>
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "gdiplus.lib")

#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif
#ifndef DWMSBT_MAINWINDOW
#define DWMSBT_MAINWINDOW 2
#endif

namespace
{
	constexpr UINT WM_INIT_READY   = WM_APP + 1;
	constexpr UINT WM_INIT_FAILED  = WM_APP + 2;
	constexpr UINT WM_RADAR_OK     = WM_APP + 3;
	constexpr UINT WM_RADAR_FAIL   = WM_APP + 4;
	constexpr UINT WM_REQUEST_EXIT = WM_APP + 5;
	constexpr UINT WM_OVERLAY_OK   = WM_APP + 6;
	constexpr UINT WM_OVERLAY_FAIL = WM_APP + 7;

	constexpr int CLIENT_W = 420;
	constexpr int CLIENT_H = 448;

	constexpr int IDC_STATUS    = 100;
	constexpr int IDC_SUBTITLE  = 101;
	constexpr int IDC_SHAREINFO = 102;
	constexpr int BTN_START     = 201;
	constexpr int BTN_SHARE     = 202;
	constexpr int BTN_OVERLAY   = 203;

	// AimSync brand blue
	constexpr COLORREF ACCENT       = RGB(0, 132, 255);
	constexpr COLORREF ACCENT_HOVER = RGB(40, 155, 255);
	constexpr COLORREF ACCENT_PRESS = RGB(0, 108, 220);
	constexpr COLORREF SURFACE      = RGB(44, 44, 44);
	constexpr COLORREF STROKE       = RGB(61, 61, 61);
	constexpr COLORREF TEXT_PRIMARY = RGB(255, 255, 255);
	constexpr COLORREF TEXT_SECOND  = RGB(200, 200, 200);
	constexpr COLORREF TEXT_MUTED   = RGB(150, 150, 150);
	constexpr COLORREF TEXT_OK      = RGB(108, 203, 95);
	constexpr COLORREF TEXT_ERR     = RGB(255, 153, 164);

	HWND g_hwnd = nullptr;
	HWND g_status = nullptr;
	HWND g_subtitle = nullptr;
	HWND g_shareinfo = nullptr;
	HWND g_btn_start = nullptr;
	HWND g_btn_share = nullptr;
	HWND g_btn_overlay = nullptr;
	HBRUSH g_bg_brush = nullptr;
	HBRUSH g_surface_brush = nullptr;
	HFONT g_font_display = nullptr;
	HFONT g_font_body = nullptr;
	HFONT g_font_btn = nullptr;
	HINSTANCE g_instance = nullptr;
	launcher_gui::start_radar_fn_t g_start_radar;
	launcher_gui::stop_radar_fn_t g_stop_radar;

	bool g_init_ready = false;
	bool g_radar_started = false;
	bool g_overlay_running = false;
	bool g_start_pending = false;
	bool g_overlay_pending = false;
	bool g_share_flash = false;
	bool g_status_ok = false;
	bool g_status_err = false;
	std::wstring g_status_text = L"Initializing...";
	std::string g_start_error;
	std::string g_overlay_error;
	std::string g_public_ip;

	ULONG_PTR g_gdiplus_token = 0;
	std::unique_ptr<Gdiplus::Bitmap> g_logo;
	HICON g_app_icon = nullptr;
	HICON g_app_icon_sm = nullptr;

	struct BtnState
	{
		bool hovered = false;
		bool pressed = false;
	};

	BtnState g_start_state{};
	BtnState g_share_state{};
	BtnState g_overlay_state{};
	WNDPROC g_btn_orig_proc = nullptr;

	BtnState& btn_state_for(int id)
	{
		if (id == BTN_START)
			return g_start_state;
		if (id == BTN_OVERLAY)
			return g_overlay_state;
		return g_share_state;
	}

	LRESULT CALLBACK btn_subclass_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
	{
		auto& st = btn_state_for(GetDlgCtrlID(hwnd));
		switch (msg)
		{
		case WM_MOUSEMOVE:
			if (!st.hovered)
			{
				st.hovered = true;
				InvalidateRect(hwnd, nullptr, FALSE);
			}
			{
				TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, hwnd, 0 };
				TrackMouseEvent(&tme);
			}
			break;
		case WM_MOUSELEAVE:
			st.hovered = false;
			st.pressed = false;
			InvalidateRect(hwnd, nullptr, FALSE);
			break;
		case WM_LBUTTONDOWN:
			st.pressed = true;
			InvalidateRect(hwnd, nullptr, FALSE);
			break;
		case WM_LBUTTONUP:
			st.pressed = false;
			InvalidateRect(hwnd, nullptr, FALSE);
			break;
		}
		return CallWindowProcW(g_btn_orig_proc, hwnd, msg, wparam, lparam);
	}

	static size_t curl_write_cb(char* ptr, size_t size, size_t nmemb, void* userdata)
	{
		auto* out = static_cast<std::string*>(userdata);
		out->append(ptr, size * nmemb);
		return size * nmemb;
	}

	std::string fetch_public_ip()
	{
		std::string response;
		const auto curl = curl_easy_init();
		if (!curl)
			return {};

		curl_easy_setopt(curl, CURLOPT_URL, "https://api.ipify.org?format=json");
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 8L);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

		if (curl_easy_perform(curl) != CURLE_OK)
		{
			curl_easy_cleanup(curl);
			return {};
		}
		curl_easy_cleanup(curl);

		try
		{
			const auto j = nlohmann::json::parse(response);
			return j.value("ip", "");
		}
		catch (...)
		{
			return {};
		}
	}

	bool load_logo_bitmap(HINSTANCE instance)
	{
		const HRSRC res = FindResourceW(instance, MAKEINTRESOURCEW(IDR_LOGO_PNG), MAKEINTRESOURCEW(10));
		if (!res)
			return false;

		const HGLOBAL loaded = LoadResource(instance, res);
		const void* data = LockResource(loaded);
		const DWORD size = SizeofResource(instance, res);
		if (!data || !size)
			return false;

		HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, size);
		if (!mem)
			return false;

		memcpy(GlobalLock(mem), data, size);
		GlobalUnlock(mem);

		IStream* stream = nullptr;
		if (CreateStreamOnHGlobal(mem, TRUE, &stream) != S_OK)
			return false;

		g_logo = std::make_unique<Gdiplus::Bitmap>(stream, FALSE);
		stream->Release();

		return g_logo && g_logo->GetLastStatus() == Gdiplus::Ok;
	}

	HICON create_icon_from_logo(int size)
	{
		if (!g_logo)
			return nullptr;

		Gdiplus::Bitmap thumb(size, size, PixelFormat32bppARGB);
		Gdiplus::Graphics g(&thumb);
		g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
		g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
		g.SetCompositingMode(Gdiplus::CompositingModeSourceOver);
		g.Clear(Gdiplus::Color(0, 0, 0, 0));
		g.DrawImage(g_logo.get(), 0, 0, size, size);

		Gdiplus::BitmapData data{};
		Gdiplus::Rect rect(0, 0, size, size);
		if (thumb.LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &data) != Gdiplus::Ok)
			return nullptr;

		const int mask_stride = ((size + 15) / 16) * 2;
		std::vector<BYTE> and_mask(static_cast<size_t>(mask_stride) * size, 0xFF);
		std::vector<BYTE> color_bits(static_cast<size_t>(size) * size * 4);

		for (int y = 0; y < size; ++y)
		{
			const auto* src = static_cast<const BYTE*>(data.Scan0) + y * data.Stride;
			auto* dst = color_bits.data() + y * size * 4;
			for (int x = 0; x < size; ++x)
			{
				dst[x * 4 + 0] = src[x * 4 + 0];
				dst[x * 4 + 1] = src[x * 4 + 1];
				dst[x * 4 + 2] = src[x * 4 + 2];
				dst[x * 4 + 3] = src[x * 4 + 3];
				if (src[x * 4 + 3] >= 128)
					and_mask[y * mask_stride + x / 8] &= static_cast<BYTE>(~(1 << (7 - (x % 8))));
			}
		}
		thumb.UnlockBits(&data);

		BITMAPINFO bmi{};
		bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmi.bmiHeader.biWidth = size;
		bmi.bmiHeader.biHeight = -size;
		bmi.bmiHeader.biPlanes = 1;
		bmi.bmiHeader.biBitCount = 32;
		bmi.bmiHeader.biCompression = BI_RGB;

		void* color_pixels = nullptr;
		const HDC hdc = GetDC(nullptr);
		const HBITMAP hbm_color = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &color_pixels, nullptr, 0);
		ReleaseDC(nullptr, hdc);
		if (!hbm_color || !color_pixels)
			return nullptr;

		memcpy(color_pixels, color_bits.data(), color_bits.size());

		const HBITMAP hbm_mask = CreateBitmap(size, size, 1, 1, and_mask.data());
		if (!hbm_mask)
		{
			DeleteObject(hbm_color);
			return nullptr;
		}

		ICONINFO info{};
		info.fIcon = TRUE;
		info.hbmColor = hbm_color;
		info.hbmMask = hbm_mask;
		const HICON icon = CreateIconIndirect(&info);
		DeleteObject(hbm_color);
		DeleteObject(hbm_mask);
		return icon;
	}

	void init_branding(HINSTANCE instance, HWND hwnd)
	{
		Gdiplus::GdiplusStartupInput input;
		if (Gdiplus::GdiplusStartup(&g_gdiplus_token, &input, nullptr) != Gdiplus::Ok)
			return;

		if (!load_logo_bitmap(instance))
			return;

		g_app_icon = create_icon_from_logo(32);
		g_app_icon_sm = create_icon_from_logo(16);
		if (g_app_icon)
			SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(g_app_icon));
		if (g_app_icon_sm)
			SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(g_app_icon_sm));
	}

	void shutdown_branding()
	{
		g_logo.reset();
		if (g_app_icon)
		{
			DestroyIcon(g_app_icon);
			g_app_icon = nullptr;
		}
		if (g_app_icon_sm)
		{
			DestroyIcon(g_app_icon_sm);
			g_app_icon_sm = nullptr;
		}
		if (g_gdiplus_token)
		{
			Gdiplus::GdiplusShutdown(g_gdiplus_token);
			g_gdiplus_token = 0;
		}
	}

	void draw_logo(HDC hdc, const RECT& client)
	{
		if (!g_logo)
			return;

		const int max_w = client.right - 64;
		const int max_h = 128;
		const int img_w = static_cast<int>(g_logo->GetWidth());
		const int img_h = static_cast<int>(g_logo->GetHeight());
		if (img_w <= 0 || img_h <= 0)
			return;

		const float scale = std::min(static_cast<float>(max_w) / img_w, static_cast<float>(max_h) / img_h);
		const int dw = static_cast<int>(img_w * scale);
		const int dh = static_cast<int>(img_h * scale);
		const int x = (client.right - dw) / 2;
		const int y = 14;

		Gdiplus::Graphics g(hdc);
		g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
		g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
		g.SetCompositingMode(Gdiplus::CompositingModeSourceOver);
		g.DrawImage(g_logo.get(), x, y, dw, dh);
	}

	void apply_modern_shell(HWND hwnd)
	{
		BOOL dark = TRUE;
		DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));

		const int corner = DWMWCP_ROUND;
		DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));

		const int backdrop = DWMSBT_MAINWINDOW;
		DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));

		const COLORREF caption = RGB(32, 32, 32);
		const COLORREF border = RGB(48, 48, 48);
		DwmSetWindowAttribute(hwnd, DWMWA_CAPTION_COLOR, &caption, sizeof(caption));
		DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &border, sizeof(border));
	}

	HFONT create_font(int height_px, int weight, const wchar_t* face)
	{
		return CreateFontW(
			-height_px, 0, 0, 0, weight, FALSE, FALSE, FALSE,
			DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
			CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, face);
	}

	void create_fonts()
	{
		const wchar_t* face = L"Segoe UI Variable";
		g_font_display = create_font(22, FW_SEMIBOLD, face);
		if (!g_font_display)
			g_font_display = create_font(22, FW_SEMIBOLD, L"Segoe UI");
		g_font_body = create_font(13, FW_NORMAL, face);
		if (!g_font_body)
			g_font_body = create_font(13, FW_NORMAL, L"Segoe UI");
		g_font_btn = create_font(14, FW_SEMIBOLD, face);
		if (!g_font_btn)
			g_font_btn = create_font(14, FW_SEMIBOLD, L"Segoe UI");
	}

	void destroy_fonts()
	{
		if (g_font_display) { DeleteObject(g_font_display); g_font_display = nullptr; }
		if (g_font_body) { DeleteObject(g_font_body); g_font_body = nullptr; }
		if (g_font_btn) { DeleteObject(g_font_btn); g_font_btn = nullptr; }
	}

	void set_status(const std::wstring& text, bool ok = false, bool err = false)
	{
		g_status_text = text;
		g_status_ok = ok;
		g_status_err = err;
		if (g_status)
			SetWindowTextW(g_status, text.c_str());
		InvalidateRect(g_hwnd, nullptr, FALSE);
	}

	void refresh_buttons()
	{
		const wchar_t* start_label = g_radar_started ? L"Open in browser" : L"Start radar";
		if (g_btn_start)
		{
			SetWindowTextW(g_btn_start, start_label);
			EnableWindow(g_btn_start, g_init_ready && !g_start_pending);
		}
		if (g_btn_share)
		{
			SetWindowTextW(g_btn_share, g_share_flash ? L"Copied to clipboard" : L"Share with friends");
			EnableWindow(g_btn_share, g_radar_started && !g_public_ip.empty());
		}
		if (g_btn_overlay)
		{
			const wchar_t* label = g_overlay_running ? L"Stop overlay (WIP)" : L"In-game overlay (WIP)";
			SetWindowTextW(g_btn_overlay, label);
			EnableWindow(g_btn_overlay, g_radar_started && !g_overlay_pending);
		}
		if (g_shareinfo)
		{
			if (g_radar_started && !g_public_ip.empty())
			{
				const std::wstring link = L"Share address: " + std::wstring(g_public_ip.begin(), g_public_ip.end()) + L":5173";
				SetWindowTextW(g_shareinfo, link.c_str());
				ShowWindow(g_shareinfo, SW_SHOW);
			}
			else
			{
				SetWindowTextW(g_shareinfo, L"");
				ShowWindow(g_shareinfo, SW_HIDE);
			}
		}
	}

	void copy_share_link()
	{
		if (g_public_ip.empty())
			return;

		const std::string text = g_public_ip + ":5173";
		const std::wstring wtext(text.begin(), text.end());

		if (!OpenClipboard(g_hwnd))
			return;

		EmptyClipboard();
		const size_t bytes = (wtext.size() + 1) * sizeof(wchar_t);
		if (HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes))
		{
			memcpy(GlobalLock(mem), wtext.c_str(), bytes);
			GlobalUnlock(mem);
			SetClipboardData(CF_UNICODETEXT, mem);
		}
		CloseClipboard();

		g_share_flash = true;
		refresh_buttons();
		SetTimer(g_hwnd, 1, 2000, nullptr);
	}

	void draw_fluent_button(const DRAWITEMSTRUCT& dis, bool primary, const BtnState& state, bool enabled)
	{
		const HDC hdc = dis.hDC;
		const RECT rc = dis.rcItem;

		COLORREF fill = primary ? ACCENT : SURFACE;
		COLORREF border = primary ? ACCENT : STROKE;
		COLORREF text = TEXT_PRIMARY;

		if (!enabled)
		{
			fill = RGB(50, 50, 50);
			border = RGB(58, 58, 58);
			text = TEXT_MUTED;
		}
		else if (primary)
		{
			if (state.pressed)
			{
				fill = ACCENT_PRESS;
				border = ACCENT_PRESS;
			}
			else if (state.hovered)
			{
				fill = ACCENT_HOVER;
				border = ACCENT_HOVER;
			}
		}
		else if (state.hovered)
		{
			fill = RGB(58, 58, 58);
			border = RGB(90, 90, 90);
		}

		const HBRUSH brush = CreateSolidBrush(fill);
		const HPEN pen = CreatePen(PS_SOLID, 1, border);
		const HGDIOBJ old_brush = SelectObject(hdc, brush);
		const HGDIOBJ old_pen = SelectObject(hdc, pen);
		RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 8, 8);
		SelectObject(hdc, old_brush);
		SelectObject(hdc, old_pen);
		DeleteObject(brush);
		DeleteObject(pen);

		wchar_t label[64]{};
		GetWindowTextW(dis.hwndItem, label, static_cast<int>(std::size(label)));

		SetBkMode(hdc, TRANSPARENT);
		SetTextColor(hdc, text);
		SelectObject(hdc, g_font_btn);
		DrawTextW(hdc, label, -1, const_cast<RECT*>(&rc), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
	}

	int logo_bottom(const RECT& client)
	{
		if (!g_logo)
			return 14;

		const int max_w = client.right - 64;
		const int max_h = 128;
		const int img_w = static_cast<int>(g_logo->GetWidth());
		const int img_h = static_cast<int>(g_logo->GetHeight());
		if (img_w <= 0 || img_h <= 0)
			return 14 + max_h;

		const float scale = std::min(static_cast<float>(max_w) / img_w, static_cast<float>(max_h) / img_h);
		return 14 + static_cast<int>(img_h * scale);
	}

	void paint_header(HDC hdc, const RECT& client)
	{
		draw_logo(hdc, client);

		const int card_top = logo_bottom(client) + 20;
		RECT card{ 24, card_top, client.right - 24, card_top + 60 };
		const HBRUSH card_brush = CreateSolidBrush(RGB(36, 36, 36));
		const HPEN card_pen = CreatePen(PS_SOLID, 1, STROKE);
		const HGDIOBJ ob = SelectObject(hdc, card_brush);
		const HGDIOBJ op = SelectObject(hdc, card_pen);
		RoundRect(hdc, card.left, card.top, card.right, card.bottom, 10, 10);
		SelectObject(hdc, ob);
		SelectObject(hdc, op);
		DeleteObject(card_brush);
		DeleteObject(card_pen);

		const int dot_x = card.left + 18;
		const int dot_y = (card.top + card.bottom) / 2;
		const COLORREF dot_col = g_status_err ? TEXT_ERR : (g_status_ok ? TEXT_OK : TEXT_MUTED);
		const HBRUSH dot = CreateSolidBrush(dot_col);
		const HGDIOBJ od = SelectObject(hdc, dot);
		const HGDIOBJ on = SelectObject(hdc, GetStockObject(NULL_PEN));
		Ellipse(hdc, dot_x - 5, dot_y - 5, dot_x + 5, dot_y + 5);
		SelectObject(hdc, od);
		SelectObject(hdc, on);
		DeleteObject(dot);

		SetBkMode(hdc, TRANSPARENT);
		SetTextColor(hdc, g_status_err ? TEXT_ERR : (g_status_ok ? TEXT_OK : TEXT_SECOND));
		SelectObject(hdc, g_font_body);
		RECT status_rc{ card.left + 34, card.top + 12, card.right - 16, card.bottom - 12 };
		DrawTextW(hdc, g_status_text.c_str(), -1, &status_rc, DT_LEFT | DT_WORDBREAK);
	}

	void paint(HWND hwnd)
	{
		PAINTSTRUCT ps{};
		const HDC hdc = BeginPaint(hwnd, &ps);
		if (!hdc)
			return;

		RECT client{};
		GetClientRect(hwnd, &client);

		const HDC mem = CreateCompatibleDC(hdc);
		const HBITMAP bmp = CreateCompatibleBitmap(hdc, client.right, client.bottom);
		const HGDIOBJ old = SelectObject(mem, bmp);

		if (g_bg_brush)
			FillRect(mem, &client, g_bg_brush);

		paint_header(mem, client);
		BitBlt(hdc, 0, 0, client.right, client.bottom, mem, 0, 0, SRCCOPY);

		SelectObject(mem, old);
		DeleteObject(bmp);
		DeleteDC(mem);
		EndPaint(hwnd, &ps);
	}

	void create_controls(HWND hwnd)
	{
		const int pad = 24;
		const int btn_w = CLIENT_W - pad * 2;
		const DWORD btn_style = WS_CHILD | WS_VISIBLE | BS_OWNERDRAW;

		g_subtitle = CreateWindowExW(0, L"STATIC", L"", WS_CHILD, 0, 0, 0, 0, hwnd, (HMENU)IDC_SUBTITLE, g_instance, nullptr);
		g_status = CreateWindowExW(0, L"STATIC", g_status_text.c_str(), WS_CHILD, 0, 0, 0, 0, hwnd, (HMENU)IDC_STATUS, g_instance, nullptr);
		ShowWindow(g_status, SW_HIDE);
		g_shareinfo = CreateWindowExW(0, L"STATIC", L"", WS_CHILD, pad, 230, btn_w, 20, hwnd, (HMENU)IDC_SHAREINFO, g_instance, nullptr);
		ShowWindow(g_shareinfo, SW_HIDE);

		g_btn_start = CreateWindowExW(0, L"BUTTON", L"Start radar", btn_style,
			pad, 258, btn_w, 40, hwnd, (HMENU)BTN_START, g_instance, nullptr);
		g_btn_share = CreateWindowExW(0, L"BUTTON", L"Share with friends", btn_style,
			pad, 310, btn_w, 36, hwnd, (HMENU)BTN_SHARE, g_instance, nullptr);
		g_btn_overlay = CreateWindowExW(0, L"BUTTON", L"In-game overlay (WIP)", btn_style,
			pad, 356, btn_w, 36, hwnd, (HMENU)BTN_OVERLAY, g_instance, nullptr);

		EnableWindow(g_btn_start, FALSE);
		EnableWindow(g_btn_share, FALSE);
		EnableWindow(g_btn_overlay, FALSE);

		for (HWND btn : { g_btn_start, g_btn_share, g_btn_overlay })
		{
			if (!btn)
				continue;
			if (!g_btn_orig_proc)
				g_btn_orig_proc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(btn, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(btn_subclass_proc)));
			else
				SetWindowLongPtrW(btn, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(btn_subclass_proc));
		}

		for (HWND ctrl : { g_shareinfo })
		{
			if (ctrl)
				SendMessageW(ctrl, WM_SETFONT, reinterpret_cast<WPARAM>(g_font_body), TRUE);
		}
	}

	void on_overlay_clicked()
	{
		if (!g_radar_started || g_overlay_pending)
			return;

		if (g_overlay_running || overlay::is_running())
		{
			overlay::stop();
			g_overlay_running = false;
			set_status(L"Overlay stopped", true);
			refresh_buttons();
			return;
		}

		g_overlay_pending = true;
		refresh_buttons();
		set_status(L"Starting in-game overlay...");

		std::thread([]()
			{
				std::string error;
				const bool ok = overlay::start(error);
				if (!ok && error.empty())
					error = "Failed to start overlay.";

				if (ok)
					PostMessageW(g_hwnd, WM_OVERLAY_OK, 0, 0);
				else
				{
					g_overlay_error = std::move(error);
					PostMessageW(g_hwnd, WM_OVERLAY_FAIL, 0, 0);
				}
			}).detach();
	}

	void on_start_clicked()
	{
		if (!g_init_ready || g_start_pending)
			return;

		if (g_radar_started)
		{
			launcher::open_browser_async();
			return;
		}

		g_start_pending = true;
		refresh_buttons();
		set_status(L"Starting radar - waiting for CS2...");

		std::thread([]()
			{
				std::string error;
				const bool ok = g_start_radar ? g_start_radar(error) : false;
				if (!ok && error.empty())
					error = "Failed to start radar.";

				if (ok)
					PostMessageW(g_hwnd, WM_RADAR_OK, 0, 0);
				else
				{
					g_start_error = std::move(error);
					PostMessageW(g_hwnd, WM_RADAR_FAIL, 0, 0);
				}
			}).detach();
	}

	void confirm_exit(HWND hwnd)
	{
		if (g_radar_started)
		{
			const int answer = MessageBoxW(hwnd,
				L"Stop AimSync WebRadar?",
				L"Exit",
				MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
			if (answer != IDYES)
				return;
		}
		launcher_gui::request_shutdown();
	}

	void run_init_thread()
	{
		std::thread([]()
			{
				bool ok = true;
				if (embed::has_embedded_server())
				{
					if (!embed::ensure_server_running())
						ok = false;
					else
						launcher::open_firewall_ports();
				}
				g_public_ip = fetch_public_ip();
				PostMessageW(g_hwnd, ok ? WM_INIT_READY : WM_INIT_FAILED, 0, 0);
			}).detach();
	}

	LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
	{
		switch (msg)
		{
		case WM_CREATE:
			create_fonts();
			g_bg_brush = CreateSolidBrush(RGB(32, 32, 32));
			g_surface_brush = CreateSolidBrush(SURFACE);
			create_controls(hwnd);
			init_branding(g_instance, hwnd);
			apply_modern_shell(hwnd);
			run_init_thread();
			return 0;

		case WM_INIT_READY:
			g_init_ready = true;
			set_status(L"Ready - launch CS2, then press Start radar");
			refresh_buttons();
			return 0;

		case WM_INIT_FAILED:
			set_status(L"Failed to start web server. Rebuild with npm run build:all", false, true);
			refresh_buttons();
			return 0;

		case WM_RADAR_OK:
			g_radar_started = true;
			g_start_pending = false;
			if (!tray::init(g_instance, g_app_icon_sm ? g_app_icon_sm : g_app_icon))
				LOG_WARNING("failed to create system tray icon");
			set_status(L"Radar active - press Open in browser when ready", true);
			refresh_buttons();
			return 0;

		case WM_OVERLAY_OK:
			g_overlay_pending = false;
			g_overlay_running = true;
			set_status(L"Overlay (WIP): F8 hide | F9 edit | M full map | , . opacity", true);
			refresh_buttons();
			return 0;

		case WM_OVERLAY_FAIL:
			g_overlay_pending = false;
			g_overlay_running = false;
			if (g_overlay_error.empty())
				set_status(L"Failed to start overlay", false, true);
			else
			{
				const int len = MultiByteToWideChar(CP_UTF8, 0, g_overlay_error.c_str(), -1, nullptr, 0);
				std::wstring wide(static_cast<size_t>(len - 1), L'\0');
				MultiByteToWideChar(CP_UTF8, 0, g_overlay_error.c_str(), -1, wide.data(), len);
				std::replace(wide.begin(), wide.end(), L'\n', L' ');
				set_status(wide, false, true);
			}
			refresh_buttons();
			return 0;

		case WM_RADAR_FAIL:
			g_start_pending = false;
			if (g_start_error.empty())
				set_status(L"Failed to start radar", false, true);
			else
			{
				const int len = MultiByteToWideChar(CP_UTF8, 0, g_start_error.c_str(), -1, nullptr, 0);
				std::wstring wide(static_cast<size_t>(len - 1), L'\0');
				MultiByteToWideChar(CP_UTF8, 0, g_start_error.c_str(), -1, wide.data(), len);
				std::replace(wide.begin(), wide.end(), L'\n', L' ');
				set_status(wide, false, true);
			}
			refresh_buttons();
			return 0;

		case WM_REQUEST_EXIT:
			PostQuitMessage(0);
			return 0;

		case WM_TIMER:
			if (wparam == 1)
			{
				KillTimer(hwnd, 1);
				g_share_flash = false;
				refresh_buttons();
			}
			return 0;

		case WM_COMMAND:
			switch (LOWORD(wparam))
			{
			case BTN_START: on_start_clicked(); break;
			case BTN_SHARE: copy_share_link(); break;
			case BTN_OVERLAY: on_overlay_clicked(); break;
			}
			return 0;

		case WM_DRAWITEM:
		{
			const auto* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lparam);
			if (!dis || (dis->CtlID != BTN_START && dis->CtlID != BTN_SHARE && dis->CtlID != BTN_OVERLAY))
				return FALSE;
			const bool primary = dis->CtlID == BTN_START;
			const bool enabled = IsWindowEnabled(dis->hwndItem) != FALSE;
			draw_fluent_button(*dis, primary, btn_state_for(static_cast<int>(dis->CtlID)), enabled);
			return TRUE;
		}

		case WM_CTLCOLORSTATIC:
		{
			const HDC hdc = reinterpret_cast<HDC>(wparam);
			const HWND ctrl = reinterpret_cast<HWND>(lparam);
			SetBkMode(hdc, TRANSPARENT);
			SetTextColor(hdc, TEXT_SECOND);
			if (ctrl == g_shareinfo && g_surface_brush)
				return reinterpret_cast<LRESULT>(g_surface_brush);
			return reinterpret_cast<LRESULT>(g_bg_brush);
		}

		case WM_ERASEBKGND:
			return 1;

		case WM_PAINT:
			paint(hwnd);
			return 0;

		case WM_CLOSE:
			confirm_exit(hwnd);
			return 0;

		case WM_DESTROY:
			if (g_bg_brush) { DeleteObject(g_bg_brush); g_bg_brush = nullptr; }
			if (g_surface_brush) { DeleteObject(g_surface_brush); g_surface_brush = nullptr; }
			destroy_fonts();
			shutdown_branding();
			PostQuitMessage(0);
			return 0;
		}

		return DefWindowProcW(hwnd, msg, wparam, lparam);
	}
}

void launcher_gui::show()
{
	if (!g_hwnd)
		return;
	ShowWindow(g_hwnd, SW_SHOW);
	SetForegroundWindow(g_hwnd);
}

void launcher_gui::request_shutdown()
{
	if (overlay::is_running())
		overlay::stop();
	if (g_stop_radar)
		g_stop_radar();
	if (g_hwnd)
		PostMessageW(g_hwnd, WM_REQUEST_EXIT, 0, 0);
}

int launcher_gui::run(HINSTANCE instance, start_radar_fn_t start_radar, stop_radar_fn_t stop_radar)
{
	g_instance = instance;
	g_start_radar = std::move(start_radar);
	g_stop_radar = std::move(stop_radar);

	const wchar_t* cls = L"AImSyncWebRadarLauncher";
	WNDCLASSEXW wc{};
	wc.cbSize = sizeof(wc);
	wc.lpfnWndProc = wnd_proc;
	wc.hInstance = instance;
	wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(IDC_ARROW));
	wc.hbrBackground = nullptr;
	wc.lpszClassName = cls;
	RegisterClassExW(&wc);

	RECT wr{ 0, 0, CLIENT_W, CLIENT_H };
	AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME, FALSE);

	const int win_w = wr.right - wr.left;
	const int win_h = wr.bottom - wr.top;
	const int x = (GetSystemMetrics(SM_CXSCREEN) - win_w) / 2;
	const int y = (GetSystemMetrics(SM_CYSCREEN) - win_h) / 2;

	g_hwnd = CreateWindowExW(
		0,
		cls,
		L"AimSync WebRadar",
		WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME,
		x, y, win_w, win_h,
		nullptr, nullptr, instance, nullptr);

	if (!g_hwnd)
		return 1;

	ShowWindow(g_hwnd, SW_SHOW);
	UpdateWindow(g_hwnd);

	MSG msg{};
	while (GetMessageW(&msg, nullptr, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}

	return static_cast<int>(msg.wParam);
}
