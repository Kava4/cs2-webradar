#include "pch.hpp"
#include "overlay.hpp"
#include "utils/appdata.hpp"

#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <windowsx.h>
#include <cmath>
#include <filesystem>
#include <future>
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "windowscodecs.lib")

namespace
{
	float g_radar_x = 24.f;
	float g_radar_y = 24.f;
	float g_radar_size = 240.f;
	float g_list_x = 24.f;
	float g_list_w = 268.f;
	float g_map_opacity = 0.82f;
	bool g_radar_full_map = false;
	constexpr float ROW_H = 30.f;
	constexpr float ACCENT_R = 0.f / 255.f;
	constexpr float ACCENT_G = 132.f / 255.f;
	constexpr float ACCENT_B = 255.f / 255.f;

	struct map_meta_t
	{
		float x = 0.f;
		float y = 0.f;
		float scale = 5.f;
	};

	struct map_cache_t
	{
		std::string name;
		map_meta_t meta{};
		ID2D1Bitmap* bitmap = nullptr;
	};

	template<typename T>
	void safe_release(T*& ptr)
	{
		if (ptr)
		{
			ptr->Release();
			ptr = nullptr;
		}
	}

	D2D1_COLOR_F rgba(float r, float g, float b, float a = 1.f)
	{
		return D2D1::ColorF(r, g, b, a);
	}

	std::atomic<bool> g_running{ false };
	std::atomic<bool> g_visible{ true };
	bool g_edit_mode = false;
	int g_edit_target = 0;
	bool g_f8_prev = false;
	bool g_f9_prev = false;
	bool g_tab_prev = false;
	bool g_left_prev = false;
	bool g_right_prev = false;
	bool g_up_prev = false;
	bool g_down_prev = false;
	bool g_plus_prev = false;
	bool g_minus_prev = false;
	bool g_pgup_prev = false;
	bool g_pgdn_prev = false;
	bool g_m_prev = false;
	bool g_comma_prev = false;
	bool g_period_prev = false;
	bool g_overlay_shown = false;

	HWND g_hwnd = nullptr;
	int g_win_x = 0;
	int g_win_y = 0;
	int g_win_w = 0;
	int g_win_h = 0;
	std::thread g_msg_thread;
	std::mutex g_render_mtx;
	std::mutex g_data_mtx;
	nlohmann::json g_frame_data = nlohmann::json::object();

	HDC g_mem_dc = nullptr;
	HBITMAP g_dib = nullptr;

	ID2D1Factory* g_d2d = nullptr;
	ID2D1DCRenderTarget* g_rt = nullptr;
	IDWriteFactory* g_dw = nullptr;
	IWICImagingFactory* g_wic = nullptr;
	IDWriteTextFormat* g_fnt_name = nullptr;
	IDWriteTextFormat* g_fnt_small = nullptr;
	ID2D1SolidColorBrush* g_brush_white = nullptr;
	ID2D1SolidColorBrush* g_brush_red = nullptr;
	ID2D1SolidColorBrush* g_brush_dim = nullptr;
	ID2D1SolidColorBrush* g_brush_panel = nullptr;
	ID2D1SolidColorBrush* g_brush_accent = nullptr;
	ID2D1SolidColorBrush* g_brush_ring = nullptr;

	map_cache_t g_map;
	std::unordered_map<std::string, ID2D1Bitmap*> g_avatars;

	float g_zoom = 1.f;
	float g_local_x = 0.f;
	float g_local_y = 0.f;
	bool g_local_found = false;

	size_t curl_write(void* contents, size_t size, size_t nmemb, std::vector<uint8_t>* out)
	{
		const auto total = size * nmemb;
		const auto* bytes = static_cast<uint8_t*>(contents);
		out->insert(out->end(), bytes, bytes + total);
		return total;
	}

	DWORD find_cs2_process_id()
	{
		const auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (snapshot == INVALID_HANDLE_VALUE)
			return 0;

		PROCESSENTRY32W entry{};
		entry.dwSize = sizeof(entry);
		DWORD pid = 0;

		if (Process32FirstW(snapshot, &entry))
		{
			do
			{
				if (_wcsicmp(entry.szExeFile, L"cs2.exe") == 0)
				{
					pid = entry.th32ProcessID;
					break;
				}
			} while (Process32NextW(snapshot, &entry));
		}

		CloseHandle(snapshot);
		return pid;
	}

	struct enum_cs2_ctx_t
	{
		DWORD pid = 0;
		HWND hwnd = nullptr;
		int best_area = 0;
	};

	BOOL CALLBACK enum_cs2_proc(HWND hwnd, LPARAM param)
	{
		auto* ctx = reinterpret_cast<enum_cs2_ctx_t*>(param);
		if (!IsWindowVisible(hwnd))
			return TRUE;

		DWORD window_pid = 0;
		GetWindowThreadProcessId(hwnd, &window_pid);
		if (window_pid != ctx->pid)
			return TRUE;

		RECT rc{};
		GetClientRect(hwnd, &rc);
		const int area = (rc.right - rc.left) * (rc.bottom - rc.top);
		if (area <= 0)
			return TRUE;

		if (!ctx->hwnd || area > ctx->best_area)
		{
			ctx->hwnd = hwnd;
			ctx->best_area = area;
		}
		return TRUE;
	}

	HWND find_cs2_window()
	{
		const DWORD pid = find_cs2_process_id();
		if (!pid)
			return nullptr;

		enum_cs2_ctx_t ctx{ pid };
		EnumWindows(enum_cs2_proc, reinterpret_cast<LPARAM>(&ctx));
		if (ctx.hwnd)
			return ctx.hwnd;

		return FindWindowW(nullptr, L"Counter-Strike 2");
	}

	DWORD cs2_process_id(HWND cs2)
	{
		if (!cs2)
			return 0;
		DWORD pid = 0;
		GetWindowThreadProcessId(cs2, &pid);
		return pid;
	}

	bool is_cs2_foreground(HWND cs2)
	{
		const DWORD cs2_pid = cs2_process_id(cs2);
		if (!cs2_pid)
			return false;

		const HWND fg = GetForegroundWindow();
		if (!fg)
			return true;

		DWORD fg_pid = 0;
		GetWindowThreadProcessId(fg, &fg_pid);
		return fg_pid == cs2_pid;
	}

	bool should_draw_overlay(HWND cs2)
	{
		if (!cs2 || !IsWindowVisible(cs2) || IsIconic(cs2))
			return false;
		return is_cs2_foreground(cs2);
	}

	void present_layered_frame()
	{
		if (!g_hwnd || !g_mem_dc || g_win_w <= 0 || g_win_h <= 0)
			return;

		POINT dst{ g_win_x, g_win_y };
		SIZE sz{ g_win_w, g_win_h };
		POINT src{ 0, 0 };
		BLENDFUNCTION bf{ AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
		const HDC ddc = GetDC(nullptr);
		if (!UpdateLayeredWindow(g_hwnd, ddc, &dst, &sz, g_mem_dc, &src, 0, &bf, ULW_ALPHA))
			LOG_WARNING("UpdateLayeredWindow failed (%lu)", GetLastError());
		ReleaseDC(nullptr, ddc);
	}

	std::wstring utf8_to_wide(const std::string& text)
	{
		if (text.empty())
			return {};
		const int len = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
		if (len <= 0)
			return {};
		std::wstring wide(static_cast<size_t>(len - 1), L'\0');
		MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wide.data(), len);
		return wide;
	}

	void load_overlay_settings()
	{
		const auto path = appdata::overlay_path();
		std::ifstream file(path);
		if (!file.is_open())
			return;

		try
		{
			const auto j = nlohmann::json::parse(file);
			g_zoom = j.value("zoom", g_zoom);
			g_radar_x = j.value("radar_x", g_radar_x);
			g_radar_y = j.value("radar_y", g_radar_y);
			g_radar_size = j.value("radar_size", g_radar_size);
			g_list_x = j.value("list_x", g_list_x);
			g_list_w = j.value("list_w", g_list_w);
			g_map_opacity = j.value("map_opacity", g_map_opacity);
			g_radar_full_map = j.value("radar_full_map", g_radar_full_map);
		}
		catch (...)
		{
		}
	}

	void save_overlay_settings()
	{
		appdata::ensure();
		const auto path = appdata::overlay_path();
		std::error_code ec;
		std::filesystem::create_directories(path.parent_path(), ec);

		const nlohmann::json j{
			{ "zoom", g_zoom },
			{ "radar_x", g_radar_x },
			{ "radar_y", g_radar_y },
			{ "radar_size", g_radar_size },
			{ "list_x", g_list_x },
			{ "list_w", g_list_w },
			{ "map_opacity", g_map_opacity },
			{ "radar_full_map", g_radar_full_map },
		};

		std::ofstream file(path);
		if (!file.is_open())
			return;
		file << j.dump(4);
	}

	bool read_map_meta(const std::filesystem::path& json_path, map_meta_t& meta)
	{
		std::ifstream file(json_path);
		if (!file.is_open())
			return false;
		try
		{
			const auto j = nlohmann::json::parse(file);
			meta.x = j.value("x", 0.f);
			meta.y = j.value("y", 0.f);
			meta.scale = j.value("scale", 5.f);
			return true;
		}
		catch (...)
		{
			return false;
		}
	}

	std::filesystem::path resolve_map_image(const std::string& raw_map_name)
	{
		const auto map_name = appdata::normalize_map_name(raw_map_name);
		appdata::ensure_map_assets(map_name);

		const auto dir = appdata::map_dir(map_name);
		const char* names[] = { "radar.png", "background.png" };
		for (const char* name : names)
		{
			const auto path = dir / name;
			if (std::filesystem::exists(path) && std::filesystem::file_size(path) > 0)
				return path;
		}
		return {};
	}

	std::filesystem::path resolve_map_json(const std::string& raw_map_name)
	{
		const auto map_name = appdata::normalize_map_name(raw_map_name);
		appdata::ensure_map_assets(map_name);

		const auto dest = appdata::map_dir(map_name) / "data.json";
		if (std::filesystem::exists(dest))
			return dest;
		return {};
	}

	ID2D1Bitmap* load_bitmap_from_file(const std::filesystem::path& path)
	{
		if (!g_rt || !g_wic || path.empty())
			return nullptr;

		IWICBitmapDecoder* decoder = nullptr;
		IWICBitmapFrameDecode* frame = nullptr;
		IWICFormatConverter* converter = nullptr;
		ID2D1Bitmap* bitmap = nullptr;

		if (FAILED(g_wic->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
			WICDecodeMetadataCacheOnLoad, &decoder)))
			return nullptr;

		if (FAILED(decoder->GetFrame(0, &frame)))
		{
			safe_release(decoder);
			return nullptr;
		}

		if (FAILED(g_wic->CreateFormatConverter(&converter)))
		{
			safe_release(frame);
			safe_release(decoder);
			return nullptr;
		}

		if (FAILED(converter->Initialize(frame, GUID_WICPixelFormat32bppPBGRA,
			WICBitmapDitherTypeNone, nullptr, 0.f, WICBitmapPaletteTypeMedianCut)))
		{
			safe_release(converter);
			safe_release(frame);
			safe_release(decoder);
			return nullptr;
		}

		g_rt->CreateBitmapFromWicBitmap(converter, nullptr, &bitmap);

		safe_release(converter);
		safe_release(frame);
		safe_release(decoder);
		return bitmap;
	}

	ID2D1Bitmap* load_bitmap_from_url(const std::string& url)
	{
		if (!g_rt || url.empty())
			return nullptr;

		const auto it = g_avatars.find(url);
		if (it != g_avatars.end())
			return it->second;

		std::vector<uint8_t> bytes;
		const auto curl = curl_easy_init();
		if (!curl)
			return nullptr;

		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &bytes);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 4L);
		curl_easy_setopt(curl, CURLOPT_USERAGENT, "AimSyncWebRadar/1.0");

		if (curl_easy_perform(curl) != CURLE_OK || bytes.empty())
		{
			curl_easy_cleanup(curl);
			g_avatars[url] = nullptr;
			return nullptr;
		}
		curl_easy_cleanup(curl);

		HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes.size());
		if (!mem)
			return nullptr;
		memcpy(GlobalLock(mem), bytes.data(), bytes.size());
		GlobalUnlock(mem);

		IStream* stream = nullptr;
		if (CreateStreamOnHGlobal(mem, TRUE, &stream) != S_OK)
			return nullptr;

		IWICBitmapDecoder* decoder = nullptr;
		IWICBitmapFrameDecode* frame = nullptr;
		IWICFormatConverter* converter = nullptr;
		ID2D1Bitmap* bitmap = nullptr;

		if (SUCCEEDED(g_wic->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnLoad, &decoder))
			&& SUCCEEDED(decoder->GetFrame(0, &frame))
			&& SUCCEEDED(g_wic->CreateFormatConverter(&converter))
			&& SUCCEEDED(converter->Initialize(frame, GUID_WICPixelFormat32bppPBGRA,
				WICBitmapDitherTypeNone, nullptr, 0.f, WICBitmapPaletteTypeMedianCut)))
		{
			g_rt->CreateBitmapFromWicBitmap(converter, nullptr, &bitmap);
		}

		safe_release(converter);
		safe_release(frame);
		safe_release(decoder);
		stream->Release();

		g_avatars[url] = bitmap;
		return bitmap;
	}

	void clear_map_cache()
	{
		safe_release(g_map.bitmap);
		g_map.name.clear();
	}

	void clear_avatar_cache()
	{
		for (auto& [_, bmp] : g_avatars)
			safe_release(bmp);
		g_avatars.clear();
	}

	bool load_map(const std::string& raw_map_name)
	{
		const auto map_name = appdata::normalize_map_name(raw_map_name);
		if (map_name.empty() || map_name == "invalid")
			return false;
		if (g_map.name == map_name && g_map.bitmap)
			return true;

		clear_map_cache();
		g_map.name = map_name;

		const auto json_path = resolve_map_json(map_name);
		if (!json_path.empty())
			read_map_meta(json_path, g_map.meta);

		const auto png_path = resolve_map_image(map_name);
		if (png_path.empty())
		{
			LOG_WARNING("overlay map image missing for %s", map_name.c_str());
			return false;
		}

		g_map.bitmap = load_bitmap_from_file(png_path);
		return g_map.bitmap != nullptr;
	}

	void world_to_norm(const map_meta_t& meta, float wx, float wy, float& nx, float& ny)
	{
		nx = (wx - meta.x) / meta.scale / 1024.f;
		ny = ((wy - meta.y) / meta.scale * -1.f) / 1024.f;
	}

	bool create_dib(int w, int h)
	{
		if (!g_mem_dc)
			g_mem_dc = CreateCompatibleDC(nullptr);
		if (!g_mem_dc)
			return false;

		if (g_dib)
		{
			DeleteObject(g_dib);
			g_dib = nullptr;
		}

		BITMAPINFO bmi{};
		bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmi.bmiHeader.biWidth = w;
		bmi.bmiHeader.biHeight = -h;
		bmi.bmiHeader.biPlanes = 1;
		bmi.bmiHeader.biBitCount = 32;
		bmi.bmiHeader.biCompression = BI_RGB;

		void* bits = nullptr;
		g_dib = CreateDIBSection(g_mem_dc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
		if (!g_dib)
			return false;

		SelectObject(g_mem_dc, g_dib);
		if (bits)
			memset(bits, 0, static_cast<size_t>(w) * static_cast<size_t>(h) * 4);
		return true;
	}

	bool create_render_target()
	{
		if (!g_d2d)
			return false;

		safe_release(g_rt);

		D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
			D2D1_RENDER_TARGET_TYPE_DEFAULT,
			D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
			0, 0, D2D1_RENDER_TARGET_USAGE_NONE, D2D1_FEATURE_LEVEL_DEFAULT);

		if (FAILED(g_d2d->CreateDCRenderTarget(&props, &g_rt)))
			return false;

		g_rt->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
		g_rt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
		return true;
	}

	void create_brushes()
	{
		if (!g_rt)
			return;
		g_rt->CreateSolidColorBrush(rgba(1.f, 1.f, 1.f), &g_brush_white);
		g_rt->CreateSolidColorBrush(rgba(1.f, .25f, .25f), &g_brush_red);
		g_rt->CreateSolidColorBrush(rgba(.55f, .58f, .64f), &g_brush_dim);
		g_rt->CreateSolidColorBrush(rgba(.05f, .06f, .09f, .88f), &g_brush_panel);
		g_rt->CreateSolidColorBrush(rgba(ACCENT_R, ACCENT_G, ACCENT_B), &g_brush_accent);
		g_rt->CreateSolidColorBrush(rgba(.25f, .30f, .40f, .65f), &g_brush_ring);
	}

	void release_brushes()
	{
		safe_release(g_brush_ring);
		safe_release(g_brush_accent);
		safe_release(g_brush_panel);
		safe_release(g_brush_dim);
		safe_release(g_brush_red);
		safe_release(g_brush_white);
	}

	void draw_text(const wchar_t* text, IDWriteTextFormat* fmt, ID2D1Brush* brush,
		float x, float y, float w, float h)
	{
		if (!text || !fmt || !brush || !g_rt)
			return;
		const D2D1_RECT_F rc{ x, y, x + w, y + h };
		g_rt->DrawText(text, static_cast<UINT32>(wcslen(text)), fmt, rc, brush);
	}

	void draw_round_rect(float x, float y, float w, float h, float r, ID2D1Brush* fill, ID2D1Brush* stroke = nullptr)
	{
		const D2D1_ROUNDED_RECT rr{
			D2D1::RectF(x, y, x + w, y + h),
			r, r
		};
		if (fill)
			g_rt->FillRoundedRectangle(rr, fill);
		if (stroke)
			g_rt->DrawRoundedRectangle(rr, stroke, 1.f);
	}

	void draw_round_rect_color(float x, float y, float w, float h, float r, D2D1_COLOR_F fill)
	{
		ID2D1SolidColorBrush* brush = nullptr;
		if (FAILED(g_rt->CreateSolidColorBrush(fill, &brush)))
			return;
		draw_round_rect(x, y, w, h, r, brush);
		safe_release(brush);
	}

	void fill_rect_color(float x, float y, float w, float h, D2D1_COLOR_F fill)
	{
		ID2D1SolidColorBrush* brush = nullptr;
		if (FAILED(g_rt->CreateSolidColorBrush(fill, &brush)))
			return;
		g_rt->FillRectangle(D2D1::RectF(x, y, x + w, y + h), brush);
		safe_release(brush);
	}

	void fill_ellipse_color(float x, float y, float radius, D2D1_COLOR_F fill)
	{
		ID2D1SolidColorBrush* brush = nullptr;
		if (FAILED(g_rt->CreateSolidColorBrush(fill, &brush)))
			return;
		g_rt->FillEllipse(D2D1::Ellipse({ x, y }, radius, radius), brush);
		safe_release(brush);
	}

	void stroke_ellipse_color(float x, float y, float radius, D2D1_COLOR_F fill, float width = 1.5f)
	{
		ID2D1SolidColorBrush* brush = nullptr;
		if (FAILED(g_rt->CreateSolidColorBrush(fill, &brush)))
			return;
		g_rt->DrawEllipse(D2D1::Ellipse({ x, y }, radius, radius), brush, width);
		safe_release(brush);
	}

	D2D1_COLOR_F ally_color(int color_idx)
	{
		static const D2D1_COLOR_F colors[] = {
			rgba(0x84 / 255.f, 0xc8 / 255.f, 0xed / 255.f, 1.f),
			rgba(0x00 / 255.f, 0x9a / 255.f, 0x7d / 255.f, 1.f),
			rgba(0xea / 255.f, 0xdd / 255.f, 0x40 / 255.f, 1.f),
			rgba(0xdf / 255.f, 0x7d / 255.f, 0x29 / 255.f, 1.f),
			rgba(0xb7 / 255.f, 0x2b / 255.f, 0x92 / 255.f, 1.f),
			rgba(1.f, 1.f, 1.f, 1.f),
		};
		if (color_idx < 0 || color_idx >= static_cast<int>(std::size(colors)))
			return colors[0];
		return colors[color_idx];
	}

	D2D1_COLOR_F player_dot_color(const nlohmann::json& player, int local_team)
	{
		const int team = player.value("m_team", 0);
		if (team == local_team && team != 0)
			return ally_color(player.value("m_color", 0));
		return rgba(1.f, 0x44 / 255.f, 0x44 / 255.f, 1.f);
	}

	bool key_edge(int vk, bool& prev)
	{
		const bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
		const bool edge = down && !prev;
		prev = down;
		return edge;
	}

	void poll_edit_keys()
	{
		if (!g_edit_mode)
			return;

		const bool fast = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
		const float step = fast ? 8.f : 2.f;

		if (key_edge(VK_TAB, g_tab_prev))
			g_edit_target = (g_edit_target + 1) % 2;

		const bool left = key_edge(VK_LEFT, g_left_prev);
		const bool right = key_edge(VK_RIGHT, g_right_prev);
		const bool up = key_edge(VK_UP, g_up_prev);
		const bool down = key_edge(VK_DOWN, g_down_prev);
		const bool plus = key_edge(VK_OEM_PLUS, g_plus_prev) || key_edge(VK_ADD, g_plus_prev);
		const bool minus = key_edge(VK_OEM_MINUS, g_minus_prev) || key_edge(VK_SUBTRACT, g_minus_prev);
		const bool pgup = key_edge(VK_PRIOR, g_pgup_prev);
		const bool pgdn = key_edge(VK_NEXT, g_pgdn_prev);

		if (g_edit_target == 0)
		{
			if (left)  g_radar_x -= step;
			if (right) g_radar_x += step;
			if (up)    g_radar_y -= step;
			if (down)  g_radar_y += step;
			if (plus)  g_radar_size += step;
			if (minus) g_radar_size = std::max(120.f, g_radar_size - step);
		}
		else
		{
			if (left)  g_list_x -= step;
			if (right) g_list_x += step;
			if (up)    g_list_w = std::max(180.f, g_list_w - step);
			if (down)  g_list_w += step;
		}

		if (!g_radar_full_map)
		{
			if (pgup) g_zoom = std::min(6.f, g_zoom + 0.1f);
			if (pgdn) g_zoom = std::max(1.f, g_zoom - 0.1f);
		}

		if (key_edge(VK_OEM_COMMA, g_comma_prev))
			g_map_opacity = std::max(0.15f, g_map_opacity - 0.05f);
		if (key_edge(VK_OEM_PERIOD, g_period_prev))
			g_map_opacity = std::min(1.f, g_map_opacity + 0.05f);
	}

	void draw_players_on_radar(const nlohmann::json& data, int local_team,
		float cx, float cy, float origin_x, float origin_y, float units_per_norm)
	{
		if (!data.contains("m_players") || !data["m_players"].is_array())
			return;

		for (const auto& p : data["m_players"])
		{
			const int team = p.value("m_team", 0);
			if (team == 0)
				continue;

			float nx = 0.f, ny = 0.f;
			world_to_norm(g_map.meta,
				p.value("/m_position/x"_json_pointer, 0.f),
				p.value("/m_position/y"_json_pointer, 0.f),
				nx, ny);

			const bool dead = p.value("m_is_dead", false);
			const bool is_local = p.value("m_is_local", false);
			const float px = cx + (nx - origin_x) * units_per_norm;
			const float py = cy + (ny - origin_y) * units_per_norm;

			auto dot_col = player_dot_color(p, local_team);
			if (dead)
				dot_col = rgba(dot_col.r * .35f, dot_col.g * .35f, dot_col.b * .35f, .55f);

			const float dot_base = std::max(4.f, units_per_norm * 0.014f);
			const float dot_r = is_local ? dot_base * 1.2f : (dead ? dot_base * 0.7f : dot_base);

			if (is_local && !dead)
				stroke_ellipse_color(px, py, dot_r + 2.f, rgba(1.f, 1.f, 1.f, .9f), 1.5f);
			fill_ellipse_color(px, py, dot_r, dot_col);
		}
	}

	void draw_radar_disc(const nlohmann::json& data, int local_team)
	{
		const float cx = g_radar_x + g_radar_size * 0.5f;
		const float cy = g_radar_y + g_radar_size * 0.5f;
		const float radius = g_radar_size * 0.5f;
		const float frame_r = g_radar_full_map ? 8.f : radius;

		draw_round_rect(g_radar_x, g_radar_y, g_radar_size, g_radar_size, frame_r, g_brush_panel, g_brush_ring);
		if (!g_radar_full_map)
			g_rt->DrawEllipse(D2D1::Ellipse({ cx, cy }, radius - 1.f, radius - 1.f), g_brush_ring, 1.5f);

		const auto map_name = data.value("m_map", "");
		if (!map_name.empty())
			load_map(map_name);

		const float origin_x = g_radar_full_map ? 0.5f : (g_local_found ? g_local_x : 0.5f);
		const float origin_y = g_radar_full_map ? 0.5f : (g_local_found ? g_local_y : 0.5f);
		const float units_per_norm = g_radar_full_map ? g_radar_size : (g_radar_size * g_zoom);

		ID2D1EllipseGeometry* clip_ellipse = nullptr;
		if (!g_radar_full_map && g_d2d)
			g_d2d->CreateEllipseGeometry(D2D1::Ellipse({ cx, cy }, radius - 2.f, radius - 2.f), &clip_ellipse);

		if (g_radar_full_map)
		{
			g_rt->PushAxisAlignedClip(
				D2D1::RectF(g_radar_x + 1.f, g_radar_y + 1.f, g_radar_x + g_radar_size - 1.f, g_radar_y + g_radar_size - 1.f),
				D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
		}
		else if (clip_ellipse)
		{
			g_rt->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(), clip_ellipse), nullptr);
		}

		if (g_map.bitmap)
		{
			const float map_px = units_per_norm;
			const float map_x = cx - origin_x * map_px;
			const float map_y = cy - origin_y * map_px;
			const D2D1_RECT_F dest{ map_x, map_y, map_x + map_px, map_y + map_px };
			g_rt->DrawBitmap(g_map.bitmap, dest, g_map_opacity, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
		}
		else
		{
			draw_round_rect_color(g_radar_x + 8.f, g_radar_y + 8.f, g_radar_size - 16.f, g_radar_size - 16.f,
				frame_r - 4.f, rgba(.08f, .10f, .14f, .9f));
		}

		draw_players_on_radar(data, local_team, cx, cy, origin_x, origin_y, units_per_norm);

		if (g_radar_full_map)
			g_rt->PopAxisAlignedClip();
		else if (clip_ellipse)
		{
			g_rt->PopLayer();
			safe_release(clip_ellipse);
		}

		if (g_edit_mode)
		{
			wchar_t hint[96]{};
			if (!g_radar_full_map)
				swprintf_s(hint, L"ZOOM %.1fx  opacity %.0f%%", g_zoom, g_map_opacity * 100.f);
			else
				swprintf_s(hint, L"FULL MAP  opacity %.0f%%", g_map_opacity * 100.f);
			draw_text(hint, g_fnt_small, g_brush_dim, g_radar_x + 8.f, g_radar_y + g_radar_size - 18.f, 220.f, 14.f);
		}
		else if (!g_radar_full_map)
		{
			draw_text(L"+  -", g_fnt_small, g_brush_dim, g_radar_x + g_radar_size - 44.f, g_radar_y + g_radar_size - 20.f, 40.f, 16.f);
		}
	}

	void draw_enemy_list(const nlohmann::json& data, int local_team)
	{
		if (!data.contains("m_players") || !data["m_players"].is_array())
			return;

		std::vector<nlohmann::json> enemies;
		for (const auto& p : data["m_players"])
		{
			const int team = p.value("m_team", 0);
			if (team == local_team || team == 0)
				continue;
			enemies.push_back(p);
		}

		const float list_y = g_radar_y + g_radar_size + 12.f;
		const float list_h = ROW_H * static_cast<float>(std::max<size_t>(enemies.size(), 1)) + 10.f;
		draw_round_rect(g_list_x, list_y, g_list_w, list_h, 8.f, g_brush_panel, g_brush_ring);

		draw_text(L"ENEMIES", g_fnt_small, g_brush_accent, g_list_x + 10.f, list_y + 4.f, 80.f, 14.f);

		float y = list_y + 18.f;
		for (const auto& p : enemies)
		{
			const bool dead = p.value("m_is_dead", false);
			const int hp = p.value("m_health", 0);
			const std::string name = p.value("m_name", "");
			const std::string weapon = p.value("/m_weapons/m_active"_json_pointer, "");
			const std::string avatar_url = p.value("m_avatar_url", "");

			const float row_x = g_list_x + 6.f;
			const float row_w = g_list_w - 12.f;
			draw_round_rect_color(row_x, y, row_w, ROW_H - 2.f, 6.f,
				dead ? rgba(.04f, .04f, .05f, .55f) : rgba(.08f, .09f, .12f, .75f));

			const float av_x = row_x + 6.f;
			const float av_y = y + 4.f;
			const float av_s = 22.f;

			if (ID2D1Bitmap* av = load_bitmap_from_url(avatar_url))
			{
				const D2D1_RECT_F av_rc{ av_x, av_y, av_x + av_s, av_y + av_s };
				g_rt->DrawBitmap(av, av_rc, dead ? .35f : 1.f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
			}
			else
			{
				g_rt->FillEllipse(D2D1::Ellipse({ av_x + av_s * .5f, av_y + av_s * .5f }, 10.f, 10.f),
					dead ? g_brush_dim : g_brush_red);
			}

			const auto wname = utf8_to_wide(name);
			draw_text(wname.c_str(), g_fnt_name, dead ? g_brush_dim : g_brush_white,
				row_x + 34.f, y + 4.f, 120.f, 16.f);

			const auto wweapon = utf8_to_wide(weapon);
			draw_text(wweapon.c_str(), g_fnt_small, g_brush_dim,
				row_x + 34.f, y + 16.f, 120.f, 12.f);

			wchar_t hp_buf[16]{};
			swprintf_s(hp_buf, L"+%d", hp);
			draw_text(hp_buf, g_fnt_name, dead ? g_brush_dim : g_brush_red,
				row_x + row_w - 42.f, y + 8.f, 36.f, 16.f);

			if (!dead && hp < 100)
			{
				const float bar_x = row_x + 34.f;
				const float bar_y = y + ROW_H - 8.f;
				const float bar_w = row_w - 78.f;
				fill_rect_color(bar_x, bar_y, bar_w, 3.f, rgba(.12f, .12f, .14f, 1.f));
				fill_rect_color(bar_x, bar_y, bar_w * (hp / 100.f), 3.f, rgba(1.f, .25f, .25f, 1.f));
			}

			y += ROW_H;
		}
	}

	void draw_edit_guides()
	{
		if (!g_edit_mode)
			return;

		draw_round_rect(g_radar_x - 2.f, g_radar_y - 38.f, 430.f, 34.f, 4.f, g_brush_accent);
		draw_text(L"EDIT: Arrows move | +/- size | Tab widget | M full-map box | , . opacity | PgUp/Dn zoom | F9 save",
			g_fnt_small, g_brush_white, g_radar_x + 6.f, g_radar_y - 36.f, 420.f, 12.f);

		if (g_edit_target == 0)
		{
			const float guide_r = g_radar_full_map ? 10.f : (g_radar_size * 0.5f + 3.f);
			draw_round_rect(g_radar_x - 3.f, g_radar_y - 3.f, g_radar_size + 6.f, g_radar_size + 6.f,
				guide_r, nullptr, g_brush_accent);
		}
		else
		{
			const float list_y = g_radar_y + g_radar_size + 12.f;
			draw_round_rect(g_list_x - 3.f, list_y - 3.f, g_list_w + 6.f, 80.f, 8.f, nullptr, g_brush_accent);
		}
	}

	void set_click_through(bool enabled)
	{
		if (!g_hwnd)
			return;
		LONG_PTR ex = GetWindowLongPtrW(g_hwnd, GWL_EXSTYLE);
		if (enabled)
			ex |= WS_EX_TRANSPARENT;
		else
			ex &= ~static_cast<LONG_PTR>(WS_EX_TRANSPARENT);
		SetWindowLongPtrW(g_hwnd, GWL_EXSTYLE, ex);
	}

	void poll_hotkeys()
	{
		const bool f8 = (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
		if (f8 && !g_f8_prev)
			g_visible = !g_visible;
		g_f8_prev = f8;

		const bool f9 = (GetAsyncKeyState(VK_F9) & 0x8000) != 0;
		if (f9 && !g_f9_prev)
		{
			if (g_edit_mode)
				save_overlay_settings();
			g_edit_mode = !g_edit_mode;
		}
		g_f9_prev = f9;

		if (key_edge('M', g_m_prev))
			g_radar_full_map = !g_radar_full_map;

		poll_edit_keys();
	}

	void sync_cs2_window(HWND cs2, bool active)
	{
		if (!g_hwnd)
			return;

		if (!cs2 || !active)
		{
			if (g_overlay_shown)
			{
				ShowWindow(g_hwnd, SW_HIDE);
				g_overlay_shown = false;
			}
			return;
		}

		RECT rc{};
		GetClientRect(cs2, &rc);
		POINT tl{ 0, 0 };
		ClientToScreen(cs2, &tl);

		const int w = rc.right - rc.left;
		const int h = rc.bottom - rc.top;
		if (w <= 0 || h <= 0)
			return;

		const bool moved = tl.x != g_win_x || tl.y != g_win_y;
		const bool resized = w != g_win_w || h != g_win_h;

		if (resized)
		{
			std::lock_guard lock(g_render_mtx);
			g_win_w = w;
			g_win_h = h;
			create_dib(w, h);
			safe_release(g_rt);
			create_render_target();
			release_brushes();
			create_brushes();
			clear_map_cache();
		}

		g_win_x = tl.x;
		g_win_y = tl.y;

		UINT flags = SWP_NOACTIVATE | SWP_SHOWWINDOW;
		if (!moved)
			flags |= SWP_NOMOVE;
		if (!resized)
			flags |= SWP_NOSIZE;

		// TOPMOST only while CS2 is focused — hidden otherwise, so it won't cover other apps.
		SetWindowPos(g_hwnd, HWND_TOPMOST, tl.x, tl.y, w, h, flags);
		g_overlay_shown = true;
	}

	LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
	{
		if (msg == WM_CLOSE)
		{
			overlay::stop();
			return 0;
		}
		return DefWindowProcW(hwnd, msg, wp, lp);
	}

	std::string init_d2d()
	{
		if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, &g_d2d)))
			return "Direct2D factory failed.";

		if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
			reinterpret_cast<IUnknown**>(&g_dw))))
		{
			return "DirectWrite factory failed.";
		}

		if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&g_wic))))
		{
			return "WIC imaging factory failed.";
		}

		g_dw->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
			DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.f, L"en-US", &g_fnt_name);
		g_dw->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
			DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 9.f, L"en-US", &g_fnt_small);

		if (!create_render_target())
			return "Overlay render target failed.";

		create_brushes();
		return {};
	}

	void teardown_d2d()
	{
		clear_avatar_cache();
		clear_map_cache();
		release_brushes();
		safe_release(g_fnt_small);
		safe_release(g_fnt_name);
		safe_release(g_rt);
		safe_release(g_wic);
		safe_release(g_dw);
		safe_release(g_d2d);
		if (g_dib)
		{
			DeleteObject(g_dib);
			g_dib = nullptr;
		}
		if (g_mem_dc)
		{
			DeleteDC(g_mem_dc);
			g_mem_dc = nullptr;
		}
	}

	void draw_clear_frame()
	{
		std::lock_guard lock(g_render_mtx);
		RECT bind{ 0, 0, g_win_w, g_win_h };
		if (FAILED(g_rt->BindDC(g_mem_dc, &bind)))
			return;

		g_rt->BeginDraw();
		g_rt->Clear(rgba(0, 0, 0, 0));
		const HRESULT hr = g_rt->EndDraw();
		if (FAILED(hr))
		{
			LOG_WARNING("overlay EndDraw failed (clear) hr=0x%08X", static_cast<unsigned>(hr));
			return;
		}
		present_layered_frame();
	}

	void draw_overlay_frame(const nlohmann::json& data)
	{
		const int local_team = data.value("m_local_team", 0);
		g_local_found = false;
		g_local_x = .5f;
		g_local_y = .5f;

		if (data.contains("m_players") && data["m_players"].is_array())
		{
			for (const auto& p : data["m_players"])
			{
				if (!p.value("m_is_local", false))
					continue;
				world_to_norm(g_map.meta,
					p.value("/m_position/x"_json_pointer, 0.f),
					p.value("/m_position/y"_json_pointer, 0.f),
					g_local_x, g_local_y);
				g_local_found = true;
				break;
			}
		}

		std::lock_guard lock(g_render_mtx);
		RECT bind{ 0, 0, g_win_w, g_win_h };
		if (FAILED(g_rt->BindDC(g_mem_dc, &bind)))
			return;

		g_rt->BeginDraw();
		g_rt->Clear(rgba(0, 0, 0, 0));

		draw_radar_disc(data, local_team);
		draw_edit_guides();
		draw_enemy_list(data, local_team);

		const HRESULT hr = g_rt->EndDraw();
		if (FAILED(hr))
		{
			if (hr == D2DERR_RECREATE_TARGET)
			{
				safe_release(g_rt);
				if (create_render_target())
				{
					release_brushes();
					create_brushes();
				}
			}
			else
			{
				LOG_WARNING("overlay EndDraw failed hr=0x%08X", static_cast<unsigned>(hr));
			}
			return;
		}

		present_layered_frame();
	}

	std::string create_overlay_window()
	{
		const HINSTANCE instance = GetModuleHandleW(nullptr);
		const wchar_t* cls = L"AimSyncWebRadarOverlay";

		WNDCLASSEXW wc{};
		wc.cbSize = sizeof(wc);
		wc.lpfnWndProc = wnd_proc;
		wc.hInstance = instance;
		wc.lpszClassName = cls;

		if (!RegisterClassExW(&wc))
		{
			const DWORD err = GetLastError();
			if (err != ERROR_CLASS_ALREADY_EXISTS)
				return "Overlay window class registration failed.";
		}

		g_hwnd = CreateWindowExW(
			WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT,
			cls, L"", WS_POPUP,
			g_win_x, g_win_y, g_win_w, g_win_h, nullptr, nullptr, instance, nullptr);

		if (!g_hwnd)
			return "Overlay window creation failed.";

		ShowWindow(g_hwnd, SW_SHOWNOACTIVATE);
		return {};
	}

	void overlay_thread_main(std::promise<std::string> ready)
	{
		const HRESULT com_hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
		const bool com_owned = (com_hr == S_OK);

		if (com_hr != S_OK && com_hr != S_FALSE && com_hr != RPC_E_CHANGED_MODE)
		{
			ready.set_value("COM initialization failed.");
			return;
		}

		if (const auto err = init_d2d(); !err.empty())
		{
			teardown_d2d();
			if (com_owned)
				CoUninitialize();
			ready.set_value(err);
			return;
		}

		if (!create_dib(g_win_w, g_win_h))
		{
			teardown_d2d();
			if (com_owned)
				CoUninitialize();
			ready.set_value("Overlay framebuffer failed.");
			return;
		}

		if (const auto err = create_overlay_window(); !err.empty())
		{
			teardown_d2d();
			if (com_owned)
				CoUninitialize();
			ready.set_value(err);
			return;
		}

		g_visible = true;
		g_edit_mode = false;
		set_click_through(true);

		ready.set_value({});

		while (g_running.load())
		{
			MSG msg{};
			while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessageW(&msg);
			}

			poll_hotkeys();

			const HWND game = find_cs2_window();
			const bool active = g_visible && should_draw_overlay(game);
			sync_cs2_window(game, active);

			if (active)
			{
				nlohmann::json data = nlohmann::json::object();
				{
					std::lock_guard lock(g_data_mtx);
					data = g_frame_data;
				}
				draw_overlay_frame(data);
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(16));
		}

		save_overlay_settings();

		if (g_hwnd)
		{
			DestroyWindow(g_hwnd);
			g_hwnd = nullptr;
		}
		g_overlay_shown = false;

		teardown_d2d();

		if (com_owned)
			CoUninitialize();
	}
}

bool overlay::is_running()
{
	return g_running.load();
}

bool overlay::start(std::string& error_out)
{
	if (g_running.load())
		return true;

	appdata::ensure();
	appdata::sync_maps();
	load_overlay_settings();

	const HWND cs2 = find_cs2_window();
	if (!cs2)
	{
		error_out = "Counter-Strike 2 is not running.";
		return false;
	}

	RECT rc{};
	GetClientRect(cs2, &rc);
	POINT tl{ 0, 0 };
	ClientToScreen(cs2, &tl);
	g_win_x = tl.x;
	g_win_y = tl.y;
	g_win_w = rc.right - rc.left;
	g_win_h = rc.bottom - rc.top;
	if (g_win_w <= 0 || g_win_h <= 0)
	{
		error_out = "Could not read CS2 window size.";
		return false;
	}

	{
		std::lock_guard lock(g_data_mtx);
		g_frame_data = nlohmann::json::object();
	}

	if (g_msg_thread.joinable())
		g_msg_thread.join();

	std::promise<std::string> ready_promise;
	std::future<std::string> ready_future = ready_promise.get_future();

	g_running = true;
	g_msg_thread = std::thread(overlay_thread_main, std::move(ready_promise));

	if (ready_future.wait_for(std::chrono::seconds(10)) != std::future_status::ready)
	{
		g_running = false;
		if (g_msg_thread.joinable())
			g_msg_thread.join();
		error_out = "Overlay init timed out.";
		return false;
	}

	if (const std::string init_err = ready_future.get(); !init_err.empty())
	{
		g_running = false;
		if (g_msg_thread.joinable())
			g_msg_thread.join();
		error_out = init_err;
		return false;
	}

	LOG_INFO("overlay started");
	return true;
}

void overlay::stop()
{
	if (!g_running.load())
		return;

	save_overlay_settings();
	g_running = false;
	if (g_msg_thread.joinable())
		g_msg_thread.join();

	LOG_INFO("overlay stopped");
}

void overlay::render(const nlohmann::json& data)
{
	if (!g_running.load())
		return;

	std::lock_guard lock(g_data_mtx);
	g_frame_data = data;
}

void overlay::shutdown()
{
	stop();
}
