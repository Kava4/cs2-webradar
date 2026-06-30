#include "pch.hpp"
#include "overlay_imgui.hpp"
#include "utils/appdata.hpp"

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>

#include <d3d11.h>
#include <dxgi.h>
#include <dwmapi.h>
#include <windowsx.h>
#include <wincodec.h>
#include <curl/curl.h>

#include <cmath>
#include <cctype>
#include <cfloat>
#include <algorithm>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <unordered_set>

#define NANOSVG_IMPLEMENTATION
#define NANOSVGRAST_IMPLEMENTATION
#include "third_party/nanosvg/nanosvg.h"
#include "third_party/nanosvg/nanosvgrast.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "windowscodecs.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace
{
	constexpr float k_pi = 3.14159265358979323846f;

	constexpr float ACCENT_R = 220.f / 255.f;
	constexpr float ACCENT_G = 190.f / 255.f;
	constexpr float ACCENT_B = 80.f / 255.f;

	constexpr ImU32 COL_RADAR_BG = IM_COL32(16, 16, 16, 175);
	constexpr ImU32 COL_RADAR_BORDER = IM_COL32(220, 190, 80, 255);
	constexpr ImU32 COL_RADAR_ARROW = IM_COL32(255, 255, 255, 240);
	constexpr ImU32 COL_LOCAL_PLAYER = IM_COL32(234, 221, 64, 255);

	float g_radar_x = 24.f;
	float g_radar_y = 24.f;
	float g_radar_size = 240.f;
	float g_list_x = 24.f;
	float g_list_w = 360.f;
	float g_list_y = -1.f;
	float g_map_opacity = 0.92f;
	bool g_radar_full_map = false;
	bool g_show_enemy_list = true;
	bool g_show_view_cones = true;
	bool g_show_grenades = true;
	bool g_show_bomb_marker = true;
	bool g_radar_rotate = true;
	float g_zoom = 2.8f;

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
		ID3D11ShaderResourceView* srv = nullptr;
		int width = 0;
		int height = 0;
	};

	std::atomic<bool> g_running{ false };
	std::atomic<bool> g_visible{ true };
	bool g_settings_open = false;
	bool g_f8_prev = false;
	bool g_f9_prev = false;
	bool g_del_prev = false;
	bool g_m_prev = false;
	bool g_overlay_shown = false;
	bool g_dwm_alpha = true;

	HWND g_hwnd = nullptr;
	HWND g_cs2_hwnd = nullptr;
	int g_win_x = 0;
	int g_win_y = 0;
	int g_win_w = 0;
	int g_win_h = 0;
	std::thread g_msg_thread;
	std::mutex g_data_mtx;
	nlohmann::json g_frame_data = nlohmann::json::object();

	ID3D11Device* g_device = nullptr;
	ID3D11DeviceContext* g_ctx = nullptr;
	IDXGISwapChain* g_swap = nullptr;
	ID3D11RenderTargetView* g_rtv = nullptr;
	ID3D11BlendState* g_bs_default = nullptr;
	ID3D11BlendState* g_bs_erase = nullptr;

	map_cache_t g_map;
	std::unordered_map<std::string, ID3D11ShaderResourceView*> g_avatars;
	std::mutex g_avatar_mtx;
	std::unordered_set<std::string> g_avatar_loading;
	std::unordered_set<std::string> g_avatar_failed;

	struct ui_icon_t
	{
		ID3D11ShaderResourceView* srv = nullptr;
		int width = 0;
		int height = 0;
	};

	std::unordered_map<std::string, ui_icon_t> g_ui_icons;
	std::mutex g_ui_icon_mtx;
	std::unordered_set<std::string> g_ui_icon_failed;
	std::deque<std::string> g_avatar_queue;
	std::unordered_map<std::string, std::vector<uint8_t>> g_avatar_pending;
	std::thread g_avatar_thread;
	std::mutex g_avatar_queue_mtx;
	std::condition_variable g_avatar_cv;
	std::atomic<bool> g_avatar_worker_running{ false };

	float g_local_x = 0.5f;
	float g_local_y = 0.5f;
	float g_smooth_local_x = 0.5f;
	float g_smooth_local_y = 0.5f;
	float g_local_eye_angle = 0.f;
	float g_local_wx = 0.f;
	float g_local_wy = 0.f;
	float g_smooth_local_wx = 0.f;
	float g_smooth_local_wy = 0.f;
	float g_smooth_local_eye = 0.f;
	bool g_local_found = false;

	struct hit_rect_t
	{
		float x0 = 0.f;
		float y0 = 0.f;
		float x1 = 0.f;
		float y1 = 0.f;
		bool valid = false;
	};

	hit_rect_t g_radar_hit;
	hit_rect_t g_list_hit;
	hit_rect_t g_settings_hit;

	void set_hit_rect(hit_rect_t& r, const ImVec2& mn, const ImVec2& mx)
	{
		r.x0 = mn.x;
		r.y0 = mn.y;
		r.x1 = mx.x;
		r.y1 = mx.y;
		r.valid = true;
	}

	bool point_in_hit_rect(const hit_rect_t& r, float x, float y)
	{
		return r.valid && x >= r.x0 && x <= r.x1 && y >= r.y0 && y <= r.y1;
	}

	bool settings_hit_test_client(float x, float y)
	{
		return point_in_hit_rect(g_settings_hit, x, y)
			|| point_in_hit_rect(g_radar_hit, x, y)
			|| point_in_hit_rect(g_list_hit, x, y);
	}

	void focus_cs2_window()
	{
		if (!g_cs2_hwnd || !IsWindow(g_cs2_hwnd))
			return;

		if (GetForegroundWindow() == g_cs2_hwnd)
			return;

		const HWND fg = GetForegroundWindow();
		DWORD fg_tid = 0;
		DWORD cs2_tid = GetWindowThreadProcessId(g_cs2_hwnd, nullptr);
		if (fg)
			fg_tid = GetWindowThreadProcessId(fg, nullptr);

		if (fg_tid && fg_tid != cs2_tid)
			AttachThreadInput(fg_tid, cs2_tid, TRUE);

		SetForegroundWindow(g_cs2_hwnd);

		if (fg_tid && fg_tid != cs2_tid)
			AttachThreadInput(fg_tid, cs2_tid, FALSE);
	}

	void set_mouse_passthrough(bool enabled)
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

	void apply_passive_overlay_window(HWND hwnd)
	{
		const MARGINS margins{ 0, 0, 0, 0 };
		DwmExtendFrameIntoClientArea(hwnd, &margins);

		LONG_PTR ex = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
		ex |= WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE;
		SetWindowLongPtrW(hwnd, GWL_EXSTYLE, ex);
		SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);
		g_dwm_alpha = false;
	}

	void apply_colorkey_window(HWND hwnd)
	{
		apply_passive_overlay_window(hwnd);
	}

	void apply_interactive_settings_window(HWND hwnd)
	{
		const MARGINS margins{ -1, -1, -1, -1 };
		DwmExtendFrameIntoClientArea(hwnd, &margins);

		LONG_PTR ex = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
		ex &= ~(WS_EX_TRANSPARENT | WS_EX_NOACTIVATE);
		ex |= WS_EX_LAYERED;
		SetWindowLongPtrW(hwnd, GWL_EXSTYLE, ex);
		SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
		g_dwm_alpha = true;

		ClipCursor(nullptr);

		const HWND fg = GetForegroundWindow();
		DWORD fg_tid = 0;
		DWORD our_tid = GetCurrentThreadId();
		if (fg)
			fg_tid = GetWindowThreadProcessId(fg, nullptr);

		if (fg_tid && fg_tid != our_tid)
			AttachThreadInput(fg_tid, our_tid, TRUE);

		SetForegroundWindow(hwnd);
		SetFocus(hwnd);
		SetActiveWindow(hwnd);

		if (fg_tid && fg_tid != our_tid)
			AttachThreadInput(fg_tid, our_tid, FALSE);
	}

	template<typename T>
	void safe_release(T*& ptr)
	{
		if (ptr)
		{
			ptr->Release();
			ptr = nullptr;
		}
	}

	ImU32 im_col(float r, float g, float b, float a = 1.f)
	{
		return IM_COL32(
			static_cast<int>(r * 255.f),
			static_cast<int>(g * 255.f),
			static_cast<int>(b * 255.f),
			static_cast<int>(a * 255.f));
	}

	ImU32 ally_color(int color_idx, float alpha = 1.f)
	{
		static const ImU32 colors[] = {
			im_col(0x84 / 255.f, 0xc8 / 255.f, 0xed / 255.f, 1.f),
			im_col(0x00 / 255.f, 0x9a / 255.f, 0x7d / 255.f, 1.f),
			im_col(0xea / 255.f, 0xdd / 255.f, 0x40 / 255.f, 1.f),
			im_col(0xdf / 255.f, 0x7d / 255.f, 0x29 / 255.f, 1.f),
			im_col(0xb7 / 255.f, 0x2b / 255.f, 0x92 / 255.f, 1.f),
			im_col(1.f, 1.f, 1.f, 1.f),
		};
		if (color_idx < 0 || color_idx >= static_cast<int>(std::size(colors)))
			return colors[0];
		const ImU32 c = colors[color_idx];
		if (alpha >= 1.f)
			return c;
		return IM_COL32(
			(c >> IM_COL32_R_SHIFT) & 0xFF,
			(c >> IM_COL32_G_SHIFT) & 0xFF,
			(c >> IM_COL32_B_SHIFT) & 0xFF,
			static_cast<int>(alpha * 255.f));
	}

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
		if (g_settings_open)
			return true;
		return is_cs2_foreground(cs2);
	}

	float enemy_list_top()
	{
		if (g_list_y >= 0.f)
			return g_list_y;
		return g_radar_y + g_radar_size + 12.f;
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
			if (g_list_w < 340.f)
				g_list_w = 340.f;
			g_map_opacity = j.value("map_opacity", g_map_opacity);
			g_radar_full_map = j.value("radar_full_map", g_radar_full_map);
			g_list_y = j.value("list_y", g_list_y);
			g_show_enemy_list = j.value("show_enemy_list", g_show_enemy_list);
			g_show_view_cones = j.value("show_view_cones", g_show_view_cones);
			g_show_grenades = j.value("show_grenades", g_show_grenades);
			g_show_bomb_marker = j.value("show_bomb_marker", g_show_bomb_marker);
			g_radar_rotate = j.value("radar_rotate", true);
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
			{ "list_y", g_list_y },
			{ "show_enemy_list", g_show_enemy_list },
			{ "show_view_cones", g_show_view_cones },
			{ "show_grenades", g_show_grenades },
			{ "show_bomb_marker", g_show_bomb_marker },
			{ "radar_rotate", g_radar_rotate },
		};

		std::ofstream file(path);
		if (!file.is_open())
			return;
		file << j.dump(4);
	}

	bool key_edge(int vk, bool& prev)
	{
		const bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
		const bool edge = down && !prev;
		prev = down;
		return edge;
	}

	void set_settings_mode(bool open)
	{
		if (g_settings_open == open)
			return;

		if (!open)
		{
			save_overlay_settings();
			if (g_hwnd)
				apply_passive_overlay_window(g_hwnd);
			focus_cs2_window();
		}
		else if (g_hwnd)
		{
			apply_interactive_settings_window(g_hwnd);
		}

		g_settings_open = open;
	}

	void poll_hotkeys()
	{
		const bool f8 = (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
		if (f8 && !g_f8_prev)
		{
			g_visible = !g_visible;
			if (!g_visible && g_settings_open)
				set_settings_mode(false);
		}
		g_f8_prev = f8;

		const bool del = (GetAsyncKeyState(VK_DELETE) & 0x8000) != 0;
		if (del && !g_del_prev)
			set_settings_mode(!g_settings_open);
		g_del_prev = del;

		const bool f9 = (GetAsyncKeyState(VK_F9) & 0x8000) != 0;
		if (f9 && !g_f9_prev)
			set_settings_mode(!g_settings_open);
		g_f9_prev = f9;

		if (g_settings_open && key_edge('M', g_m_prev))
			g_radar_full_map = !g_radar_full_map;
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

	bool create_texture_from_rgba(const std::vector<uint8_t>& rgba, int w, int h, ID3D11ShaderResourceView** out_srv)
	{
		if (!g_device || rgba.empty() || w <= 0 || h <= 0 || !out_srv)
			return false;

		D3D11_TEXTURE2D_DESC desc{};
		desc.Width = static_cast<UINT>(w);
		desc.Height = static_cast<UINT>(h);
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

		D3D11_SUBRESOURCE_DATA sub{};
		sub.pSysMem = rgba.data();
		sub.SysMemPitch = static_cast<UINT>(w * 4);

		ID3D11Texture2D* tex = nullptr;
		if (FAILED(g_device->CreateTexture2D(&desc, &sub, &tex)))
			return false;

		D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
		srv_desc.Format = desc.Format;
		srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srv_desc.Texture2D.MipLevels = 1;

		ID3D11ShaderResourceView* srv = nullptr;
		const HRESULT hr = g_device->CreateShaderResourceView(tex, &srv_desc, &srv);
		tex->Release();
		if (FAILED(hr))
			return false;

		*out_srv = srv;
		return true;
	}

	bool load_image_rgba(const std::filesystem::path& path, std::vector<uint8_t>& rgba, int& w, int& h)
	{
		if (path.empty())
			return false;

		IWICImagingFactory* wic = nullptr;
		if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&wic))))
			return false;

		IWICBitmapDecoder* decoder = nullptr;
		IWICBitmapFrameDecode* frame = nullptr;
		IWICFormatConverter* converter = nullptr;
		bool ok = false;

		if (SUCCEEDED(wic->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
			WICDecodeMetadataCacheOnLoad, &decoder)) &&
			SUCCEEDED(decoder->GetFrame(0, &frame)) &&
			SUCCEEDED(wic->CreateFormatConverter(&converter)) &&
			SUCCEEDED(converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA,
				WICBitmapDitherTypeNone, nullptr, 0.f, WICBitmapPaletteTypeCustom)))
		{
			UINT width = 0;
			UINT height = 0;
			if (SUCCEEDED(converter->GetSize(&width, &height)) && width > 0 && height > 0)
			{
				rgba.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
				if (SUCCEEDED(converter->CopyPixels(nullptr, width * 4,
					static_cast<UINT>(rgba.size()), rgba.data())))
				{
					w = static_cast<int>(width);
					h = static_cast<int>(height);
					ok = true;
				}
			}
		}

		safe_release(converter);
		safe_release(frame);
		safe_release(decoder);
		safe_release(wic);
		return ok;
	}

	bool load_texture_from_file(const std::filesystem::path& path, ID3D11ShaderResourceView** out_srv, int& w, int& h)
	{
		std::vector<uint8_t> rgba;
		if (!load_image_rgba(path, rgba, w, h))
			return false;
		return create_texture_from_rgba(rgba, w, h, out_srv);
	}

	std::filesystem::path resolve_bundled_asset(const std::filesystem::path& relative)
	{
		const auto base = appdata::exe_dir();
		const std::filesystem::path candidates[] = {
			base / relative,
			base / ".." / ".." / "radar" / "public" / relative,
			base / ".." / "radar" / "public" / relative,
		};

		for (const auto& candidate : candidates)
		{
			std::error_code ec;
			const auto resolved = std::filesystem::weakly_canonical(candidate, ec);
			const auto path = ec ? candidate : resolved;
			if (std::filesystem::exists(path))
				return path;
		}
		return {};
	}

	bool rasterize_svg_white(const std::filesystem::path& path, int target_h, std::vector<uint8_t>& rgba, int& out_w, int& out_h)
	{
		if (path.empty() || target_h <= 0)
			return false;

		std::ifstream file(path, std::ios::binary);
		if (!file.is_open())
			return false;

		std::string svg_data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		if (svg_data.empty())
			return false;

		std::vector<char> mutable_svg(svg_data.begin(), svg_data.end());
		mutable_svg.push_back('\0');

		NSVGimage* image = nsvgParse(mutable_svg.data(), "px", 96.f);
		if (!image || image->height <= 0.f)
		{
			if (image)
				nsvgDelete(image);
			return false;
		}

		const float scale = static_cast<float>(target_h) / image->height;
		out_w = std::max(1, static_cast<int>(image->width * scale));
		out_h = std::max(1, static_cast<int>(image->height * scale));
		rgba.assign(static_cast<size_t>(out_w) * static_cast<size_t>(out_h) * 4, 0);

		NSVGrasterizer* rasterizer = nsvgCreateRasterizer();
		if (!rasterizer)
		{
			nsvgDelete(image);
			return false;
		}

		nsvgRasterize(rasterizer, image, 0.f, 0.f, scale, rgba.data(), out_w, out_h, out_w * 4);
		nsvgDeleteRasterizer(rasterizer);
		nsvgDelete(image);

		for (int i = 0; i < out_w * out_h; ++i)
		{
			const uint8_t a = rgba[static_cast<size_t>(i) * 4 + 3];
			if (a < 8)
				continue;

			const uint8_t lum = static_cast<uint8_t>(std::max<int>({
				rgba[static_cast<size_t>(i) * 4 + 0],
				rgba[static_cast<size_t>(i) * 4 + 1],
				rgba[static_cast<size_t>(i) * 4 + 2],
				a
			}));

			rgba[static_cast<size_t>(i) * 4 + 0] = 255;
			rgba[static_cast<size_t>(i) * 4 + 1] = 255;
			rgba[static_cast<size_t>(i) * 4 + 2] = 255;
			rgba[static_cast<size_t>(i) * 4 + 3] = lum;
		}

		return true;
	}

	bool load_ui_icon(const std::string& icon_name, int target_h = 32)
	{
		if (icon_name.empty())
			return false;

		{
			std::lock_guard lock(g_ui_icon_mtx);
			if (g_ui_icons.contains(icon_name))
				return g_ui_icons[icon_name].srv != nullptr;
			if (g_ui_icon_failed.contains(icon_name))
				return false;
		}

		const auto svg_path = resolve_bundled_asset(
			std::filesystem::path("assets") / "icons" / (icon_name + ".svg"));
		if (svg_path.empty())
		{
			std::lock_guard lock(g_ui_icon_mtx);
			g_ui_icon_failed.insert(icon_name);
			return false;
		}

		std::vector<uint8_t> rgba;
		int w = 0;
		int h = 0;
		if (!rasterize_svg_white(svg_path, target_h, rgba, w, h))
		{
			std::lock_guard lock(g_ui_icon_mtx);
			g_ui_icon_failed.insert(icon_name);
			return false;
		}

		ID3D11ShaderResourceView* srv = nullptr;
		if (!create_texture_from_rgba(rgba, w, h, &srv))
		{
			std::lock_guard lock(g_ui_icon_mtx);
			g_ui_icon_failed.insert(icon_name);
			return false;
		}

		std::lock_guard lock(g_ui_icon_mtx);
		g_ui_icons[icon_name] = { srv, w, h };
		return true;
	}

	ui_icon_t* get_ui_icon(const std::string& icon_name)
	{
		if (icon_name.empty())
			return nullptr;

		{
			std::lock_guard lock(g_ui_icon_mtx);
			const auto it = g_ui_icons.find(icon_name);
			if (it != g_ui_icons.end())
				return &it->second;
		}

		if (!load_ui_icon(icon_name))
			return nullptr;

		std::lock_guard lock(g_ui_icon_mtx);
		const auto it = g_ui_icons.find(icon_name);
		return it != g_ui_icons.end() ? &it->second : nullptr;
	}

	bool load_character_icon(const std::string& model, int target_h = 64)
	{
		if (model.empty())
			return false;

		const std::string key = "character:" + model;
		{
			std::lock_guard lock(g_ui_icon_mtx);
			if (g_ui_icons.contains(key))
				return g_ui_icons[key].srv != nullptr;
			if (g_ui_icon_failed.contains(key))
				return false;
		}

		const auto png_path = resolve_bundled_asset(
			std::filesystem::path("assets") / "characters" / (model + ".png"));
		if (png_path.empty())
		{
			std::lock_guard lock(g_ui_icon_mtx);
			g_ui_icon_failed.insert(key);
			return false;
		}

		ID3D11ShaderResourceView* srv = nullptr;
		int w = 0;
		int h = 0;
		if (!load_texture_from_file(png_path, &srv, w, h))
		{
			std::lock_guard lock(g_ui_icon_mtx);
			g_ui_icon_failed.insert(key);
			return false;
		}

		std::lock_guard lock(g_ui_icon_mtx);
		g_ui_icons[key] = { srv, w, h };
		return true;
	}

	ui_icon_t* get_character_icon(const std::string& model)
	{
		if (model.empty())
			return nullptr;

		const std::string key = "character:" + model;
		{
			std::lock_guard lock(g_ui_icon_mtx);
			const auto it = g_ui_icons.find(key);
			if (it != g_ui_icons.end())
				return &it->second;
		}

		if (!load_character_icon(model))
			return nullptr;

		std::lock_guard lock(g_ui_icon_mtx);
		const auto it = g_ui_icons.find(key);
		return it != g_ui_icons.end() ? &it->second : nullptr;
	}

	bool draw_tinted_icon(ImDrawList* dl, ui_icon_t* icon, const ImVec2& pos, float height, ImU32 tint)
	{
		if (!icon || !icon->srv || icon->height <= 0)
			return false;

		const float scale = height / static_cast<float>(icon->height);
		const float width = static_cast<float>(icon->width) * scale;
		dl->AddImage(reinterpret_cast<ImTextureID>(icon->srv), pos,
			ImVec2(pos.x + width, pos.y + height), ImVec2(0.f, 0.f), ImVec2(1.f, 1.f), tint);
		return true;
	}

	void clear_ui_icon_cache()
	{
		std::lock_guard lock(g_ui_icon_mtx);
		for (auto& [_, icon] : g_ui_icons)
			safe_release(icon.srv);
		g_ui_icons.clear();
		g_ui_icon_failed.clear();
	}

	void clear_map_cache()
	{
		safe_release(g_map.srv);
		g_map.name.clear();
		g_map.width = 0;
		g_map.height = 0;
	}

	bool load_map(const std::string& raw_map_name)
	{
		const auto map_name = appdata::normalize_map_name(raw_map_name);
		if (map_name.empty() || map_name == "invalid")
			return false;
		if (g_map.name == map_name && g_map.srv)
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

		if (!load_texture_from_file(png_path, &g_map.srv, g_map.width, g_map.height))
			return false;

		return true;
	}

	void queue_avatar_load(const std::string& url)
	{
		if (url.empty())
			return;

		{
			std::lock_guard lock(g_avatar_mtx);
			if (g_avatars.contains(url) || g_avatar_loading.contains(url))
				return;
			g_avatar_failed.erase(url);
		}

		{
			std::lock_guard lock(g_avatar_queue_mtx);
			g_avatar_queue.push_back(url);
		}
		g_avatar_cv.notify_one();
	}

	void avatar_worker()
	{
		while (g_avatar_worker_running.load())
		{
			std::string url;
			{
				std::unique_lock lock(g_avatar_queue_mtx);
				g_avatar_cv.wait(lock, []
				{
					return !g_avatar_worker_running.load() || !g_avatar_queue.empty();
				});
				if (!g_avatar_worker_running.load())
					break;
				url = std::move(g_avatar_queue.front());
				g_avatar_queue.pop_front();
			}

			{
				std::lock_guard lock(g_avatar_mtx);
				g_avatar_loading.insert(url);
			}

			std::vector<uint8_t> bytes;
			CURL* curl = curl_easy_init();
			if (curl)
			{
				curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
				curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write);
				curl_easy_setopt(curl, CURLOPT_WRITEDATA, &bytes);
				curl_easy_setopt(curl, CURLOPT_TIMEOUT, 8L);
				curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
				curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
				curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
				curl_easy_setopt(curl, CURLOPT_USERAGENT, "AimSync-WebRadar/1.0");
				if (curl_easy_perform(curl) != CURLE_OK)
					bytes.clear();
				curl_easy_cleanup(curl);
			}

			if (!bytes.empty())
			{
				std::lock_guard lock(g_avatar_queue_mtx);
				g_avatar_pending[url] = std::move(bytes);
			}
			else
			{
				std::lock_guard lock(g_avatar_mtx);
				g_avatar_failed.insert(url);
			}

			{
				std::lock_guard lock(g_avatar_mtx);
				g_avatar_loading.erase(url);
			}
		}
	}

	void start_avatar_worker()
	{
		if (g_avatar_worker_running.exchange(true))
			return;
		if (g_avatar_thread.joinable())
			g_avatar_thread.join();
		g_avatar_thread = std::thread(avatar_worker);
	}

	void stop_avatar_worker()
	{
		if (!g_avatar_worker_running.exchange(false))
			return;
		g_avatar_cv.notify_all();
		if (g_avatar_thread.joinable())
			g_avatar_thread.join();
	}

	void clear_avatar_cache()
	{
		std::lock_guard lock(g_avatar_mtx);
		for (auto& [_, srv] : g_avatars)
			safe_release(srv);
		g_avatars.clear();
		g_avatar_loading.clear();
		g_avatar_failed.clear();
		g_avatar_pending.clear();
	}

	void process_avatar_pending()
	{
		std::unordered_map<std::string, std::vector<uint8_t>> pending;
		{
			std::lock_guard lock(g_avatar_queue_mtx);
			if (g_avatar_pending.empty())
				return;
			pending.swap(g_avatar_pending);
		}

		for (auto& [url, bytes] : pending)
		{
			IWICImagingFactory* wic = nullptr;
			if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
				IID_PPV_ARGS(&wic))))
				continue;

			IWICStream* stream = nullptr;
			IWICBitmapDecoder* decoder = nullptr;
			IWICBitmapFrameDecode* frame = nullptr;
			IWICFormatConverter* converter = nullptr;

			if (FAILED(wic->CreateStream(&stream)) ||
				FAILED(stream->InitializeFromMemory(bytes.data(), static_cast<DWORD>(bytes.size()))) ||
				FAILED(wic->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnLoad, &decoder)) ||
				FAILED(decoder->GetFrame(0, &frame)) ||
				FAILED(wic->CreateFormatConverter(&converter)) ||
				FAILED(converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA,
					WICBitmapDitherTypeNone, nullptr, 0.f, WICBitmapPaletteTypeCustom)))
			{
				safe_release(converter);
				safe_release(frame);
				safe_release(decoder);
				safe_release(stream);
				safe_release(wic);
				std::lock_guard lock(g_avatar_mtx);
				g_avatar_failed.insert(url);
				continue;
			}

			UINT w = 0;
			UINT h = 0;
			converter->GetSize(&w, &h);
			std::vector<uint8_t> rgba(static_cast<size_t>(w) * static_cast<size_t>(h) * 4);
			if (SUCCEEDED(converter->CopyPixels(nullptr, w * 4, static_cast<UINT>(rgba.size()), rgba.data())))
			{
				ID3D11ShaderResourceView* srv = nullptr;
				if (create_texture_from_rgba(rgba, static_cast<int>(w), static_cast<int>(h), &srv))
				{
					std::lock_guard lock(g_avatar_mtx);
					g_avatars[url] = srv;
				}
				else
				{
					std::lock_guard lock(g_avatar_mtx);
					g_avatar_failed.insert(url);
				}
			}

			safe_release(converter);
			safe_release(frame);
			safe_release(decoder);
			safe_release(stream);
			safe_release(wic);
		}
	}

	ID3D11ShaderResourceView* get_avatar_srv(const std::string& url)
	{
		if (url.empty())
			return nullptr;

		{
			std::lock_guard lock(g_avatar_mtx);
			const auto it = g_avatars.find(url);
			if (it != g_avatars.end())
				return it->second;
		}

		queue_avatar_load(url);
		return nullptr;
	}

	void world_to_norm(const map_meta_t& meta, float wx, float wy, float& nx, float& ny)
	{
		nx = (wx - meta.x) / meta.scale / 1024.f;
		ny = ((wy - meta.y) / meta.scale * -1.f) / 1024.f;
	}

	void norm_to_world(const map_meta_t& meta, float nx, float ny, float& wx, float& wy)
	{
		wx = meta.x + nx * meta.scale * 1024.f;
		wy = meta.y - ny * meta.scale * 1024.f;
	}

	bool use_cs2_rotate_mode()
	{
		return g_radar_rotate && g_local_found && !g_radar_full_map;
	}

	// cl_radar_rotate 1 — TKazer/CS2_External polar radar (proven CS2 cheat impl).
	void world_to_screen_polar(float wx, float wy, float local_wx, float local_wy, float local_yaw_deg,
		float meta_scale, float units, const ImVec2& center, float& px, float& py)
	{
		const float dx = wx - local_wx;
		const float dy = wy - local_wy;
		const float dist_world = std::sqrt(dx * dx + dy * dy);
		if (dist_world < 0.001f)
		{
			px = center.x;
			py = center.y;
			return;
		}

		const float bearing_deg = std::atan2(dy, dx) * 180.f / k_pi;
		const float rel_rad = (local_yaw_deg - bearing_deg) * k_pi / 180.f;
		const float screen_dist = dist_world / (meta_scale * 1024.f) * units;
		px = center.x + std::sin(rel_rad) * screen_dist;
		py = center.y - std::cos(rel_rad) * screen_dist;
	}

	// cl_radar_rotate 0 — same math as radar/src/utilities.js + player.jsx (web radar).
	void norm_to_screen_fixed(float nx, float ny, float origin_x, float origin_y, float units,
		const ImVec2& center, float& px, float& py)
	{
		px = center.x + (nx - origin_x) * units;
		py = center.y + (ny - origin_y) * units;
	}

	float view_cone_cw(float entity_eye, bool is_local)
	{
		if (use_cs2_rotate_mode())
		{
			if (is_local)
				return 0.f;
			return g_smooth_local_eye - entity_eye;
		}
		return 270.f - entity_eye;
	}

	float smooth_exp(float current, float target, float speed)
	{
		const float dt = std::max(ImGui::GetIO().DeltaTime, 0.001f);
		const float blend = 1.f - std::exp(-speed * dt);
		return current + (target - current) * blend;
	}

	float smooth_angle_exp(float current, float target, float speed)
	{
		const float delta = std::fmod(target - current + 540.f, 360.f) - 180.f;
		const float dt = std::max(ImGui::GetIO().DeltaTime, 0.001f);
		const float blend = 1.f - std::exp(-speed * dt);
		return current + delta * blend;
	}

	ImU32 player_dot_border_color(const nlohmann::json& player, int local_team, bool dead)
	{
		const int team = player.value("m_team", 0);
		if (player.value("m_is_local", false) && !dead)
			return COL_LOCAL_PLAYER;
		if (team == local_team && team != 0)
			return ally_color(player.value("m_color", 0), dead ? 0.45f : 1.f);
		return dead ? IM_COL32(255, 68, 68, 110) : IM_COL32(255, 68, 68, 255);
	}

	ImU32 player_dot_fill_color(bool is_local, bool dead)
	{
		if (dead)
			return IM_COL32(10, 14, 26, 90);
		if (is_local)
			return IM_COL32(234, 221, 64, 255);
		return IM_COL32(10, 14, 26, 235);
	}

	ImU32 hp_color_u32(int hp)
	{
		if (hp > 60)
			return IM_COL32(61, 214, 140, 255);
		if (hp > 30)
			return IM_COL32(240, 180, 41, 255);
		return IM_COL32(224, 82, 82, 255);
	}

	float smooth_enemy_hp(int idx, float target, bool dead)
	{
		static std::unordered_map<int, float> smooth;
		if (dead)
		{
			smooth[idx] = 0.f;
			return 0.f;
		}

		float& value = smooth[idx];
		if (value <= 0.f && target > 0.f)
			value = target;
		value += (target - value) * 0.18f;
		return value;
	}

	ImU32 scale_alpha(ImU32 col, float mul)
	{
		const int a = static_cast<int>(((col >> IM_COL32_A_SHIFT) & 0xFF) * std::clamp(mul, 0.f, 1.f));
		return IM_COL32(
			(col >> IM_COL32_R_SHIFT) & 0xFF,
			(col >> IM_COL32_G_SHIFT) & 0xFF,
			(col >> IM_COL32_B_SHIFT) & 0xFF,
			a);
	}

	ImVec2 smooth_screen_pos(uint64_t key, float tx, float ty, float blend)
	{
		struct entry_t
		{
			float x = 0.f;
			float y = 0.f;
			bool init = false;
		};

		static std::unordered_map<uint64_t, entry_t> cache;
		auto& e = cache[key];
		if (!e.init || blend >= 0.99f)
		{
			e.x = tx;
			e.y = ty;
			e.init = true;
			return ImVec2(tx, ty);
		}

		e.x += (tx - e.x) * blend;
		e.y += (ty - e.y) * blend;
		return ImVec2(e.x, e.y);
	}

	float draw_utility_pill(ImDrawList* dl, const ImVec2& pos, const std::string& util_name,
		const std::string& active_weapon, float alpha_mul)
	{
		if (util_name.empty())
			return 0.f;

		const bool is_active = !active_weapon.empty() && active_weapon == util_name;
		const float icon_h = 11.f;
		constexpr float pad_x = 4.f;
		constexpr float pad_y = 2.f;
		const float pill_h = icon_h + pad_y * 2.f;

		ui_icon_t* icon = get_ui_icon(util_name);
		float icon_w = icon_h;
		if (icon && icon->height > 0)
			icon_w = icon_h * static_cast<float>(icon->width) / static_cast<float>(icon->height);

		const float pill_w = pad_x * 2.f + icon_w;
		const ImVec2 mn = pos;
		const ImVec2 mx(pos.x + pill_w, pos.y + pill_h);

		const ImU32 bg = is_active
			? IM_COL32(245, 166, 35, static_cast<int>(31 * alpha_mul))
			: IM_COL32(255, 255, 255, static_cast<int>(8 * alpha_mul));
		const ImU32 border = is_active
			? IM_COL32(245, 166, 35, static_cast<int>(90 * alpha_mul))
			: IM_COL32(255, 255, 255, static_cast<int>(15 * alpha_mul));
		const ImU32 tint = is_active
			? IM_COL32(245, 166, 35, static_cast<int>(255 * alpha_mul))
			: IM_COL32(255, 255, 255, static_cast<int>(115 * alpha_mul));

		dl->AddRectFilled(mn, mx, bg, 3.f);
		dl->AddRect(mn, mx, border, 3.f, 0, 1.f);
		draw_tinted_icon(dl, icon, ImVec2(mn.x + pad_x, mn.y + pad_y), icon_h, tint);
		return pill_w;
	}

	void draw_c4_carrier_badge(ImDrawList* dl, const ImVec2& pos, float alpha_mul)
	{
		const float t = static_cast<float>(ImGui::GetTime());
		const float pulse = 0.5f + 0.5f * std::sin(t * 5.65f);

		constexpr float badge_h = 16.f;
		constexpr float icon_h = 11.f;
		constexpr float pad_x = 5.f;
		constexpr const char* label = "C4";

		const ImVec2 text_sz = ImGui::CalcTextSize(label);
		const float badge_w = pad_x * 2.f + icon_h + 4.f + text_sz.x;
		const ImVec2 mn = pos;
		const ImVec2 mx(pos.x + badge_w, pos.y + badge_h);

		if (pulse > 0.55f)
		{
			dl->AddRect(
				ImVec2(mn.x - 1.5f, mn.y - 1.5f),
				ImVec2(mx.x + 1.5f, mx.y + 1.5f),
				IM_COL32(239, 68, 68, static_cast<int>(55 * pulse * alpha_mul)),
				4.f, 0, 1.6f);
		}

		dl->AddRectFilled(mn, mx, IM_COL32(239, 68, 68, static_cast<int>(46 * alpha_mul)), 3.f);
		dl->AddRect(mn, mx,
			IM_COL32(239, 68, 68, static_cast<int>((140 + 115 * pulse) * alpha_mul)),
			3.f, 0, 1.5f);

		const ImU32 icon_col = IM_COL32(239, 68, 68, static_cast<int>((200 + 55 * pulse) * alpha_mul));
		draw_tinted_icon(dl, get_ui_icon("c4"),
			ImVec2(mn.x + pad_x, mn.y + (badge_h - icon_h) * 0.5f), icon_h, icon_col);

		dl->AddText(
			ImVec2(mn.x + pad_x + icon_h + 4.f, mn.y + (badge_h - text_sz.y) * 0.5f),
			IM_COL32(239, 68, 68, static_cast<int>(255 * alpha_mul)), label);
	}

	std::string truncate_text(const char* text, float max_w)
	{
		if (!text || !*text)
			return {};

		ImFont* font = ImGui::GetFont();
		const float font_size = ImGui::GetFontSize();
		if (font->CalcTextSizeA(font_size, FLT_MAX, 0.f, text).x <= max_w)
			return text;

		std::string out(text);
		while (!out.empty())
		{
			out.pop_back();
			const std::string ellipsed = out + "...";
			if (font->CalcTextSizeA(font_size, FLT_MAX, 0.f, ellipsed.c_str()).x <= max_w)
				return ellipsed;
		}
		return "...";
	}

	enum class weapon_glyph_t
	{
		none,
		rifle,
		sniper,
		smg,
		shotgun,
		pistol,
		heavy,
	};

	weapon_glyph_t classify_weapon(const std::string& weapon)
	{
		if (weapon.empty())
			return weapon_glyph_t::none;

		static const std::unordered_set<std::string> snipers = { "awp", "ssg08", "g3sg1", "scar20" };
		static const std::unordered_set<std::string> smgs = { "mp9", "mac10", "ump45", "p90", "bizon", "mp7", "mp5sd" };
		static const std::unordered_set<std::string> shotguns = { "nova", "xm1014", "mag7", "sawedoff" };
		static const std::unordered_set<std::string> pistols = {
			"deagle", "glock", "usp_silencer", "hkp2000", "p250", "tec9", "cz75a",
			"fiveseven", "elite", "revolver"
		};
		static const std::unordered_set<std::string> heavies = { "negev", "m249" };

		if (snipers.count(weapon))
			return weapon_glyph_t::sniper;
		if (smgs.count(weapon))
			return weapon_glyph_t::smg;
		if (shotguns.count(weapon))
			return weapon_glyph_t::shotgun;
		if (pistols.count(weapon))
			return weapon_glyph_t::pistol;
		if (heavies.count(weapon))
			return weapon_glyph_t::heavy;
		return weapon_glyph_t::rifle;
	}

	void draw_weapon_glyph(ImDrawList* dl, const ImVec2& pos, float w, float h, weapon_glyph_t type, ImU32 col)
	{
		if (type == weapon_glyph_t::none)
			return;

		const float y = pos.y + h * 0.42f;
		float body_h = h * 0.22f;
		float body_w = w * 0.82f;
		float barrel_w = w * 0.18f;

		switch (type)
		{
		case weapon_glyph_t::pistol:
			body_w = w * 0.55f;
			barrel_w = w * 0.12f;
			break;
		case weapon_glyph_t::sniper:
			body_w = w * 0.62f;
			barrel_w = w * 0.36f;
			break;
		case weapon_glyph_t::smg:
			body_w = w * 0.68f;
			barrel_w = w * 0.22f;
			break;
		case weapon_glyph_t::shotgun:
			body_w = w * 0.58f;
			barrel_w = w * 0.24f;
			body_h = h * 0.28f;
			break;
		case weapon_glyph_t::heavy:
			body_w = w * 0.78f;
			barrel_w = w * 0.14f;
			break;
		default:
			break;
		}

		dl->AddRectFilled(ImVec2(pos.x, y - body_h * 0.5f),
			ImVec2(pos.x + body_w, y + body_h * 0.5f), col, 1.f);
		dl->AddRectFilled(ImVec2(pos.x + body_w - 1.f, y - body_h * 0.35f),
			ImVec2(pos.x + body_w + barrel_w, y + body_h * 0.35f), col, 1.f);
		dl->AddRectFilled(ImVec2(pos.x + w * 0.18f, y + body_h * 0.35f),
			ImVec2(pos.x + w * 0.34f, y + h * 0.82f), col, 1.f);
	}

	void draw_health_cross(ImDrawList* dl, const ImVec2& center, float size, ImU32 col)
	{
		const float arm = size * 0.42f;
		const float thick = std::max(1.2f, size * 0.18f);
		dl->AddRectFilled(ImVec2(center.x - arm, center.y - thick * 0.5f),
			ImVec2(center.x + arm, center.y + thick * 0.5f), col);
		dl->AddRectFilled(ImVec2(center.x - thick * 0.5f, center.y - arm),
			ImVec2(center.x + thick * 0.5f, center.y + arm), col);
	}

	void draw_vest_icon(ImDrawList* dl, const ImVec2& center, float size, ImU32 col)
	{
		const ImVec2 pts[4] = {
			{ center.x - size * 0.42f, center.y - size * 0.34f },
			{ center.x + size * 0.42f, center.y - size * 0.34f },
			{ center.x + size * 0.30f, center.y + size * 0.40f },
			{ center.x - size * 0.30f, center.y + size * 0.40f },
		};
		dl->AddConvexPolyFilled(pts, 4, col);
	}

	void draw_helmet_icon(ImDrawList* dl, const ImVec2& center, float size, ImU32 col)
	{
		dl->PathArcTo(ImVec2(center.x, center.y + size * 0.08f), size * 0.40f, k_pi * 1.08f, k_pi * 1.92f, 10);
		dl->PathStroke(col, 0, 1.5f);
		dl->AddLine(ImVec2(center.x - size * 0.40f, center.y + size * 0.10f),
			ImVec2(center.x + size * 0.40f, center.y + size * 0.10f), col, 1.5f);
	}

	void draw_avatar_tile(ImDrawList* dl, const ImVec2& pos, float size, const nlohmann::json& player, bool dead)
	{
		const std::string avatar_url = player.value("m_avatar_url", "");
		const std::string model = player.value("m_model_name", "");
		const ImVec2 max(pos.x + size, pos.y + size);
		const float radius = size * 0.5f;
		const ImU32 tint = dead ? IM_COL32(140, 140, 140, 200) : IM_COL32(255, 255, 255, 255);

		if (ID3D11ShaderResourceView* av = get_avatar_srv(avatar_url))
		{
			dl->AddImageRounded(reinterpret_cast<ImTextureID>(av), pos, max,
				ImVec2(0, 0), ImVec2(1, 1), tint, radius);
			if (dead)
				dl->AddRectFilled(pos, max, IM_COL32(0, 0, 0, 130), radius);
			return;
		}

		if (ui_icon_t* character = get_character_icon(model))
		{
			draw_tinted_icon(dl, character, pos, size, tint);
			if (dead)
				dl->AddRectFilled(pos, max, IM_COL32(0, 0, 0, 130), radius);
			return;
		}

		dl->AddCircleFilled(ImVec2(pos.x + radius, pos.y + radius), radius, IM_COL32(14, 16, 20, 255));
		const std::string name = player.value("m_name", "?");
		const char letter[2] = { name.empty() ? '?' : static_cast<char>(std::toupper(name[0])), '\0' };
		const ImVec2 ts = ImGui::CalcTextSize(letter);
		dl->AddText(ImVec2(pos.x + (size - ts.x) * 0.5f, pos.y + (size - ts.y) * 0.5f),
			IM_COL32(255, 255, 255, dead ? 70 : 130), letter);
	}

	void draw_enemy_row(ImDrawList* dl, const ImVec2& row_min, float row_w, const nlohmann::json& player)
	{
		constexpr float row_h = 44.f;
		constexpr float money_w = 58.f;
		constexpr float money_gap = 6.f;
		constexpr float status_w = 72.f;
		constexpr float bar_h = 3.f;
		const bool dead = player.value("m_is_dead", false);
		const int hp = player.value("m_health", 0);
		const int money = player.value("m_money", 0);
		const int idx = player.value("m_idx", 0);
		const float smooth_hp = smooth_enemy_hp(idx, static_cast<float>(hp), dead);
		const std::string name = player.value("m_name", "?");
		const std::string active_weapon = player.value("/m_weapons/m_active"_json_pointer, "");
		const std::string primary_weapon = player.value("/m_weapons/m_primary"_json_pointer, "");
		const std::string display_weapon = !primary_weapon.empty() ? primary_weapon : active_weapon;
		const int armor = player.value("m_armor", 0);
		const bool has_helmet = player.value("m_has_helmet", false);
		const bool has_bomb = !dead && player.value("m_has_bomb", false);

		const float main_w = row_w - money_w - money_gap;
		const ImVec2 main_min = row_min;
		const ImVec2 main_max(row_min.x + main_w, row_min.y + row_h);
		const ImVec2 money_min(main_max.x + money_gap, row_min.y);
		const ImVec2 money_max(row_min.x + row_w, row_min.y + row_h);

		const float alpha = dead ? 0.52f : 1.f;
		const ImU32 icon_col = IM_COL32(255, 255, 255, static_cast<int>(235 * alpha));
		const ImU32 text_col = IM_COL32(245, 247, 250, static_cast<int>(255 * alpha));
		const ImU32 dim_col = IM_COL32(245, 247, 250, static_cast<int>(120 * alpha));

		const float avatar_sz = 28.f;
		const float pad = 5.f;
		const float content_h = row_h - bar_h - 5.f;
		const ImVec2 avatar_pos(main_min.x + pad, main_min.y + (content_h - avatar_sz) * 0.5f);
		draw_avatar_tile(dl, avatar_pos, avatar_sz, player, dead);

		const float status_left = main_max.x - status_w;
		const float name_x = avatar_pos.x + avatar_sz + 6.f;
		const float weapon_center = (name_x + 78.f + status_left) * 0.5f;
		const float name_max_w = std::max(54.f, weapon_center - 28.f - name_x);
		const std::string clipped_name = truncate_text(name.c_str(), name_max_w);
		dl->AddText(ImVec2(name_x, main_min.y + 5.f), dead ? dim_col : text_col, clipped_name.c_str());

		float weapon_w = 0.f;
		if (!display_weapon.empty())
		{
			ui_icon_t* wpn_icon = get_ui_icon(display_weapon);
			const float weapon_h = dead ? 16.f : 24.f;
			weapon_w = 40.f;
			if (wpn_icon && wpn_icon->height > 0)
				weapon_w = weapon_h * static_cast<float>(wpn_icon->width) / static_cast<float>(wpn_icon->height);
			const ImVec2 weapon_pos(weapon_center - weapon_w * 0.5f, main_min.y + (content_h - weapon_h) * 0.5f - 2.f);
			if (!draw_tinted_icon(dl, wpn_icon, weapon_pos, weapon_h, dead ? dim_col : icon_col))
				draw_weapon_glyph(dl, weapon_pos, weapon_w, weapon_h, classify_weapon(display_weapon), dead ? dim_col : icon_col);
		}

		if (has_bomb)
		{
			const float c4_x = weapon_center + weapon_w * 0.5f + 8.f;
			draw_c4_carrier_badge(dl, ImVec2(c4_x, main_min.y + (content_h - 16.f) * 0.5f - 1.f), alpha);
		}

		if (!dead && player.contains("/m_weapons/m_utilities"_json_pointer))
		{
			const auto& utils = player["/m_weapons/m_utilities"_json_pointer];
			if (utils.is_array() && !utils.empty())
			{
				float ux = name_x;
				const float uy = main_min.y + 20.f;
				constexpr float util_gap = 3.f;
				for (const auto& util : utils)
				{
					if (!util.is_string())
						continue;
					const float pill_w = draw_utility_pill(dl, ImVec2(ux, uy), util.get<std::string>(), active_weapon, alpha);
					if (pill_w > 0.f)
						ux += pill_w + util_gap;
				}
			}
		}

		float stat_x = status_left + 2.f;
		if (armor > 0 && has_helmet)
		{
			if (!draw_tinted_icon(dl, get_ui_icon("kevlar_helmet"), ImVec2(stat_x, main_min.y + 12.f), 14.f, icon_col))
				draw_helmet_icon(dl, ImVec2(stat_x + 7.f, main_min.y + 19.f), 12.f, icon_col);
			stat_x += 16.f;
		}

		if (armor > 0)
		{
			if (!draw_tinted_icon(dl, get_ui_icon("kevlar"), ImVec2(stat_x, main_min.y + 12.f), 14.f, icon_col))
				draw_vest_icon(dl, ImVec2(stat_x + 7.f, main_min.y + 19.f), 12.f, icon_col);
			stat_x += 16.f;
		}

		if (!draw_tinted_icon(dl, get_ui_icon("health"), ImVec2(stat_x, main_min.y + 12.f), 14.f, icon_col))
			draw_health_cross(dl, ImVec2(stat_x + 7.f, main_min.y + 19.f), 10.f, icon_col);
		stat_x += 14.f;

		char hp_txt[8]{};
		snprintf(hp_txt, sizeof(hp_txt), "%d", dead ? 0 : hp);
		dl->AddText(ImVec2(stat_x, main_min.y + 10.f), dead ? dim_col : text_col, hp_txt);

		char money_txt[16]{};
		snprintf(money_txt, sizeof(money_txt), "$%d", money);
		const ImVec2 money_ts = ImGui::CalcTextSize(money_txt);
		dl->AddRectFilled(money_min, money_max, IM_COL32(8, 28, 16, 255), 2.f);
		dl->AddText(
			ImVec2(money_min.x + (money_w - money_ts.x) * 0.5f, money_min.y + (row_h - money_ts.y) * 0.5f),
			IM_COL32(58, 210, 112, static_cast<int>(255 * alpha)), money_txt);

		const float bar_y = main_max.y - bar_h - 2.f;
		const ImVec2 bar_min(main_min.x + 2.f, bar_y);
		const ImVec2 bar_max(main_max.x - 2.f, bar_y + bar_h);
		dl->AddRect(bar_min, bar_max, IM_COL32(120, 125, 135, static_cast<int>(100 * alpha)), 1.f, 0, 1.f);
		if (!dead && smooth_hp > 0.5f)
		{
			const float pct = std::clamp(smooth_hp / 100.f, 0.f, 1.f);
			const ImU32 hp_col = scale_alpha(hp_color_u32(static_cast<int>(smooth_hp + 0.5f)), alpha);
			dl->AddRectFilled(bar_min,
				ImVec2(bar_min.x + (bar_max.x - bar_min.x) * pct, bar_max.y),
				hp_col, 1.f);
		}
	}

	void create_render_target()
	{
		safe_release(g_rtv);
		ID3D11Texture2D* back = nullptr;
		if (g_swap && SUCCEEDED(g_swap->GetBuffer(0, IID_PPV_ARGS(&back))))
		{
			g_device->CreateRenderTargetView(back, nullptr, &g_rtv);
			back->Release();
		}
	}

	void create_blend_states()
	{
		if (!g_device)
			return;

		D3D11_BLEND_DESC desc{};
		desc.RenderTarget[0].BlendEnable = TRUE;
		desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		g_device->CreateBlendState(&desc, &g_bs_default);

		D3D11_BLEND_DESC erase{};
		erase.RenderTarget[0].BlendEnable = TRUE;
		erase.RenderTarget[0].SrcBlend = D3D11_BLEND_ZERO;
		erase.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		erase.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		erase.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
		erase.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		erase.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		erase.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		g_device->CreateBlendState(&erase, &g_bs_erase);
	}

	void cleanup_d3d()
	{
		safe_release(g_bs_default);
		safe_release(g_bs_erase);
		clear_avatar_cache();
		clear_ui_icon_cache();
		clear_map_cache();
		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		if (ImGui::GetCurrentContext())
			ImGui::DestroyContext();

		safe_release(g_rtv);
		safe_release(g_swap);
		safe_release(g_ctx);
		safe_release(g_device);
	}

	std::string init_d3d(HWND hwnd)
	{
		auto setup_imgui = [&]() -> std::string
		{
			IMGUI_CHECKVERSION();
			ImGui::CreateContext();
			ImGuiIO& io = ImGui::GetIO();
			io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
			io.IniFilename = nullptr;

			ImGui::StyleColorsDark();
			ImGuiStyle& style = ImGui::GetStyle();
			style.WindowRounding = 8.f;
			style.FrameRounding = 5.f;
			style.WindowBorderSize = 1.f;
			style.Colors[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.06f, 0.09f, 0.88f);
			style.Colors[ImGuiCol_Border] = ImVec4(ACCENT_R, ACCENT_G, ACCENT_B, 0.65f);
			style.Colors[ImGuiCol_FrameBg] = ImVec4(0.08f, 0.09f, 0.12f, 0.95f);
			style.Colors[ImGuiCol_SliderGrab] = ImVec4(ACCENT_R, ACCENT_G, ACCENT_B, 1.f);
			style.Colors[ImGuiCol_CheckMark] = ImVec4(ACCENT_R, ACCENT_G, ACCENT_B, 1.f);
			style.Colors[ImGuiCol_Header] = ImVec4(0.08f, 0.20f, 0.36f, 0.95f);

			if (!ImGui_ImplWin32_Init(hwnd))
				return "ImGui Win32 init failed.";
			if (!ImGui_ImplDX11_Init(g_device, g_ctx))
				return "ImGui DX11 init failed.";
			return {};
		};

		DXGI_SWAP_CHAIN_DESC sd{};
		sd.BufferCount = 2;
		sd.BufferDesc.Width = 0;
		sd.BufferDesc.Height = 0;
		sd.BufferDesc.RefreshRate.Numerator = 60;
		sd.BufferDesc.RefreshRate.Denominator = 1;
		sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.OutputWindow = hwnd;
		sd.SampleDesc.Count = 1;
		sd.Windowed = TRUE;
		sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

		D3D_FEATURE_LEVEL feature_level;
		const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };

		g_dwm_alpha = false;
		sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
			0, levels, 2, D3D11_SDK_VERSION, &sd, &g_swap, &g_device, &feature_level, &g_ctx)))
			return "D3D11 device creation failed.";

		apply_colorkey_window(hwnd);

		create_render_target();
		create_blend_states();
		return setup_imgui();
	}

	// deg_cw: clockwise degrees from screen-up (same convention as web/CSS rotate).
	void draw_cs2_direction_wedge(ImDrawList* dl, float px, float py, float deg_cw, float dot_r, ImU32 col)
	{
		const float rad = deg_cw * k_pi / 180.f;
		const float tri_h = dot_r * 0.5f;
		const ImVec2 tip(px + std::sin(rad) * (dot_r + tri_h * 0.9f),
			py - std::cos(rad) * (dot_r + tri_h * 0.9f));
		const ImVec2 left(px + std::sin(rad - 0.58f) * dot_r * 0.58f,
			py - std::cos(rad - 0.58f) * dot_r * 0.58f);
		const ImVec2 right(px + std::sin(rad + 0.58f) * dot_r * 0.58f,
			py - std::cos(rad + 0.58f) * dot_r * 0.58f);
		dl->AddTriangleFilled(tip, left, right, col);
	}

	void draw_cs2_view_arrow(ImDrawList* dl, float px, float py, float deg_cw, float dot_r, ImU32 col)
	{
		const float rad = deg_cw * k_pi / 180.f;
		const float arrow_len = dot_r * 1.8f;
		const float half_w = 0.42f;
		const float base_r = dot_r * 0.9f;
		const ImVec2 tip(px + std::sin(rad) * (dot_r + arrow_len),
			py - std::cos(rad) * (dot_r + arrow_len));
		const ImVec2 left(px + std::sin(rad - half_w) * base_r,
			py - std::cos(rad - half_w) * base_r);
		const ImVec2 right(px + std::sin(rad + half_w) * base_r,
			py - std::cos(rad + half_w) * base_r);
		dl->AddTriangleFilled(tip, left, right, scale_alpha(col, 0.55f));
	}

	void draw_player_c4_badge(ImDrawList* dl, float px, float py, float dot_r)
	{
		load_ui_icon("c4", 16);
		const float badge_r = std::max(4.5f, dot_r * 0.52f);
		const float bx = px + dot_r * 0.68f;
		const float by = py - dot_r * 0.68f;
		dl->AddCircleFilled(ImVec2(bx, by), badge_r, IM_COL32(239, 68, 68, 255), 16);
		dl->AddCircle(ImVec2(bx, by), badge_r, IM_COL32(160, 25, 25, 255), 16, 1.1f);
		const float icon_h = badge_r * 1.15f;
		ui_icon_t* c4 = get_ui_icon("c4");
		float icon_w = icon_h;
		if (c4 && c4->height > 0)
			icon_w = icon_h * static_cast<float>(c4->width) / static_cast<float>(c4->height);
		draw_tinted_icon(dl, c4,
			ImVec2(bx - icon_w * 0.5f, by - icon_h * 0.5f), icon_h, IM_COL32(255, 255, 255, 255));
	}

	void draw_cs2_player_dot(ImDrawList* dl, float px, float py, float dot_r, ImU32 border_col,
		ImU32 fill_col, bool is_local, bool dead, float eye_deg_cw, bool has_bomb)
	{
		const float border_w = std::max(1.2f, dot_r * 0.16f);
		dl->AddCircleFilled(ImVec2(px, py), dot_r, fill_col, 24);
		dl->AddCircle(ImVec2(px, py), dot_r, border_col, 24, border_w);

		if (is_local && !dead)
			dl->AddCircle(ImVec2(px, py), dot_r + 1.6f, IM_COL32(255, 255, 255, 230), 24, 1.4f);

		if (!dead)
			draw_cs2_direction_wedge(dl, px, py, eye_deg_cw, dot_r, border_col);

		if (has_bomb && !dead)
			draw_player_c4_badge(dl, px, py, dot_r);
	}

	void draw_cs2_smoke_cloud(ImDrawList* dl, float px, float py, float diam)
	{
		if (diam <= 4.f)
			return;

		const float t = static_cast<float>(ImGui::GetTime());
		const float r = diam * 0.5f;
		const float rot = t * 0.12f;
		const float cos_r = std::cos(rot);
		const float sin_r = std::sin(rot);

		struct blob_t
		{
			float ox;
			float oy;
			float scale;
			int alpha;
		};

		const blob_t blobs[] = {
			{ 0.f, 0.f, 1.02f, 150 },
			{ 0.14f, -0.1f, 0.78f, 125 },
			{ -0.16f, 0.12f, 0.74f, 118 },
			{ 0.1f, 0.18f, 0.68f, 108 },
			{ -0.12f, -0.14f, 0.64f, 100 },
			{ 0.2f, 0.04f, 0.56f, 92 },
			{ -0.06f, 0.22f, 0.5f, 85 },
			{ 0.05f, -0.2f, 0.48f, 80 },
		};

		for (const auto& blob : blobs)
		{
			const float pulse = 0.9f + 0.1f * std::sin(t * 2.2f + blob.ox * 8.f);
			const float bx = blob.ox * cos_r - blob.oy * sin_r;
			const float by = blob.ox * sin_r + blob.oy * cos_r;
			const float br = r * blob.scale * pulse;
			dl->AddCircleFilled(
				ImVec2(px + bx * r, py + by * r), br,
				IM_COL32(198, 208, 218, static_cast<int>(blob.alpha * pulse)), 36);
		}

		const float core_pulse = 0.88f + 0.12f * std::sin(t * 1.8f);
		dl->AddCircleFilled(ImVec2(px, py), r * 0.42f * core_pulse,
			IM_COL32(225, 232, 238, static_cast<int>(165 * core_pulse)), 32);
		dl->AddCircleFilled(ImVec2(px - r * 0.08f, py - r * 0.1f), r * 0.28f,
			IM_COL32(235, 242, 248, 110), 24);
	}

	void draw_outside_circle_transparent_mask(ImDrawList* dl, const ImVec2& center, float radius, ImU32 col)
	{
		struct corner_patch_t
		{
			ImVec2 corner;
			float arc_start;
			float arc_end;
		};

		const float cx = center.x;
		const float cy = center.y;
		const corner_patch_t patches[4] = {
			{ { cx - radius, cy - radius }, k_pi, k_pi * 1.5f },
			{ { cx + radius, cy - radius }, k_pi * 1.5f, k_pi * 2.f },
			{ { cx + radius, cy + radius }, 0.f, k_pi * 0.5f },
			{ { cx - radius, cy + radius }, k_pi * 0.5f, k_pi },
		};

		for (const auto& patch : patches)
		{
			dl->PathClear();
			dl->PathLineTo(patch.corner);
			dl->PathArcTo(center, radius, patch.arc_start, patch.arc_end, 16);
			dl->PathFillConvex(col);
		}
	}

	void imgui_callback_set_erase(const ImDrawList*, const ImDrawCmd*)
	{
		if (!g_ctx || !g_bs_erase)
			return;
		const float factor[4] = { 0.f, 0.f, 0.f, 0.f };
		g_ctx->OMSetBlendState(g_bs_erase, factor, 0xffffffff);
	}

	void imgui_callback_set_default(const ImDrawList*, const ImDrawCmd*)
	{
		if (!g_ctx || !g_bs_default)
			return;
		const float factor[4] = { 0.f, 0.f, 0.f, 0.f };
		g_ctx->OMSetBlendState(g_bs_default, factor, 0xffffffff);
	}

	void erase_outside_circle(ImDrawList* dl, const ImVec2& center, float radius)
	{
		dl->AddCallback(imgui_callback_set_erase, nullptr);
		draw_outside_circle_transparent_mask(dl, center, radius, IM_COL32(255, 255, 255, 255));
		dl->AddCallback(imgui_callback_set_default, nullptr);
	}

	void entity_to_screen(float wx, float wy, float nx, float ny, bool rotate_mode,
		float origin_x, float origin_y, float units, const ImVec2& center, float& px, float& py)
	{
		if (rotate_mode)
		{
			world_to_screen_polar(wx, wy, g_smooth_local_wx, g_smooth_local_wy, g_smooth_local_eye,
				g_map.meta.scale, units, center, px, py);
		}
		else
		{
			norm_to_screen_fixed(nx, ny, origin_x, origin_y, units, center, px, py);
		}
	}

	void draw_cs2_bomb_marker(ImDrawList* dl, float px, float py, float size, bool planted)
	{
		load_ui_icon("c4", 20);
		ui_icon_t* c4 = get_ui_icon("c4");
		const float half = size * 0.5f;
		const ImU32 fill = planted ? IM_COL32(201, 11, 11, 255) : IM_COL32(201, 144, 11, 255);

		dl->AddCircleFilled(ImVec2(px, py), half, fill, 20);
		dl->AddCircle(ImVec2(px, py), half, IM_COL32(0, 0, 0, 180), 20, 1.1f);

		if (c4 && c4->srv)
		{
			const float icon_h = size * 0.68f;
			const float icon_w = c4->height > 0
				? icon_h * static_cast<float>(c4->width) / static_cast<float>(c4->height)
				: icon_h;
			draw_tinted_icon(dl, c4,
				ImVec2(px - icon_w * 0.5f, py - icon_h * 0.5f), icon_h, IM_COL32(255, 255, 255, 255));
		}
	}

	float grenade_aura_px(float game_units, float map_scale, float display_units)
	{
		if (map_scale <= 0.f || game_units <= 0.f)
			return 0.f;
		return (game_units / map_scale / 1024.f) * display_units * 2.f;
	}

	bool radar_point_visible(float px, float py, const ImVec2& center, float radius, bool cs2_circle)
	{
		if (!cs2_circle)
			return true;
		return std::hypot(px - center.x, py - center.y) <= radius - 2.f;
	}

	void draw_grenade_aura(ImDrawList* dl, float px, float py, float diam, ImU32 fill, ImU32 border, bool pulse = false)
	{
		if (diam <= 2.f)
			return;

		if (pulse)
		{
			const float t = static_cast<float>(ImGui::GetTime());
			const float wave = 0.86f + 0.14f * std::sin(t * 3.1f);
			fill = scale_alpha(fill, wave);
			border = scale_alpha(border, 0.82f + 0.18f * std::sin(t * 3.1f + 0.6f));
		}

		const float r = diam * 0.5f;
		dl->AddCircleFilled(ImVec2(px, py), r, fill, 48);
		dl->AddCircle(ImVec2(px, py), r, border, 48, 1.4f);
	}

	void draw_grenade_dot(ImDrawList* dl, float px, float py, float r, ImU32 fill, ImU32 border)
	{
		dl->AddCircleFilled(ImVec2(px, py), r, fill, 20);
		dl->AddCircle(ImVec2(px, py), r, border, 20, 1.2f);
	}

	void draw_radar_grenades(ImDrawList* dl, const nlohmann::json& data, bool rotate_mode,
		float origin_x, float origin_y, float units, const ImVec2& center, float radius, bool cs2_circle)
	{
		if (!g_show_grenades || !data.contains("m_grenades") || !data["m_grenades"].is_array())
			return;

		const float map_scale = g_map.meta.scale;
		const float dot_r = std::max(4.f, units * 0.007f);

		for (const auto& grenade : data["m_grenades"])
		{
			const std::string type = grenade.value("type", "");
			const float gx = grenade.value("x", 0.f);
			const float gy = grenade.value("y", 0.f);
			if (type.empty() || (gx == 0.f && gy == 0.f))
				continue;

			float nx = 0.f, ny = 0.f;
			world_to_norm(g_map.meta, gx, gy, nx, ny);
			float px = 0.f, py = 0.f;
			entity_to_screen(gx, gy, nx, ny, rotate_mode, origin_x, origin_y, units, center, px, py);

			if (!radar_point_visible(px, py, center, radius, cs2_circle))
				continue;

			const bool smoke_live = type == "smoke" && grenade.value("m_did_smoke_effect", false);
			if (smoke_live)
			{
				const float diam = grenade_aura_px(144.f, map_scale, units);
				draw_cs2_smoke_cloud(dl, px, py, diam);
				continue;
			}

			if (type == "inferno")
			{
				const float diam = grenade_aura_px(150.f, map_scale, units);
				draw_grenade_aura(dl, px, py, diam,
					IM_COL32(235, 95, 30, 70), IM_COL32(255, 120, 45, 160), true);
				continue;
			}

			ImU32 fill = IM_COL32(148, 163, 184, 255);
			ImU32 border = IM_COL32(190, 200, 210, 255);
			if (type == "flashbang")
			{
				fill = IM_COL32(254, 240, 138, 255);
				border = IM_COL32(255, 250, 200, 255);
			}
			else if (type == "he")
			{
				fill = IM_COL32(251, 146, 60, 255);
				border = IM_COL32(255, 180, 100, 255);
			}
			else if (type == "molotov" || type == "incendiary")
			{
				fill = IM_COL32(239, 68, 68, 255);
				border = IM_COL32(255, 110, 90, 255);
			}
			else if (type == "decoy")
			{
				fill = IM_COL32(167, 139, 250, 255);
				border = IM_COL32(200, 175, 255, 255);
				if (grenade.value("m_is_active", false))
					dl->AddCircle(ImVec2(px, py), dot_r * 2.2f, border, 16, 1.2f);
			}

			draw_grenade_dot(dl, px, py, dot_r, fill, border);
		}
	}

	void draw_radar_window(const nlohmann::json& data, int local_team)
	{
		const float radius = g_radar_size * 0.5f;
		const bool cs2_circle = !g_radar_full_map;
		ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing;

		if (cs2_circle)
			flags |= ImGuiWindowFlags_NoBackground;

		static bool radar_layout_init = false;

		if (!g_settings_open)
		{
			radar_layout_init = false;
			flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoInputs;
			ImGui::SetNextWindowPos(ImVec2(g_radar_x, g_radar_y), ImGuiCond_Always);
			ImGui::SetNextWindowSize(ImVec2(g_radar_size, g_radar_size), ImGuiCond_Always);
		}
		else if (!radar_layout_init)
		{
			ImGui::SetNextWindowPos(ImVec2(g_radar_x, g_radar_y), ImGuiCond_Always);
			ImGui::SetNextWindowSize(ImVec2(g_radar_size, g_radar_size), ImGuiCond_Always);
			radar_layout_init = true;
		}

		if (cs2_circle)
		{
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
		}

		if (ImGui::Begin("##radar", nullptr, flags))
		{
			const ImVec2 win_pos = ImGui::GetWindowPos();
			const ImVec2 win_sz = ImGui::GetWindowSize();
			if (g_settings_open)
				set_hit_rect(g_radar_hit, win_pos, ImVec2(win_pos.x + win_sz.x, win_pos.y + win_sz.y));

			if (g_settings_open)
			{
				g_radar_x = win_pos.x;
				g_radar_y = win_pos.y;
				g_radar_size = std::max(120.f, win_sz.x);
			}

			const ImVec2 center(win_pos.x + win_sz.x * 0.5f, win_pos.y + win_sz.y * 0.5f);
			ImDrawList* dl = ImGui::GetWindowDrawList();

			if (cs2_circle)
			{
				if (!g_dwm_alpha)
				{
					const ImU32 colorkey = IM_COL32(0, 0, 0, 255);
					const ImVec2 win_max(win_pos.x + win_sz.x, win_pos.y + win_sz.y);
					dl->AddRectFilled(win_pos, win_max, colorkey);
				}
				dl->AddCircleFilled(center, radius, COL_RADAR_BG, 64);
			}
			else
			{
				dl->AddRectFilled(win_pos, ImVec2(win_pos.x + win_sz.x, win_pos.y + win_sz.y),
					COL_RADAR_BG, 8.f);
				dl->AddRect(win_pos, ImVec2(win_pos.x + win_sz.x, win_pos.y + win_sz.y),
					COL_RADAR_BORDER, 8.f, 0, 1.5f);
			}

			const auto map_name = data.value("m_map", "");
			if (!map_name.empty())
				load_map(map_name);

			const float origin_x = g_radar_full_map ? 0.5f : (g_local_found ? g_smooth_local_x : 0.5f);
			const float origin_y = g_radar_full_map ? 0.5f : (g_local_found ? g_smooth_local_y : 0.5f);
			const float units = g_radar_full_map ? g_radar_size : (g_radar_size * g_zoom);
			const bool rotate_mode = use_cs2_rotate_mode();

			if (g_map.srv)
			{
				const ImU32 map_col = IM_COL32(255, 255, 255, static_cast<int>(g_map_opacity * 255.f));

				if (rotate_mode)
				{
					const float corners_norm[4][2] = { {0.f, 0.f}, {1.f, 0.f}, {1.f, 1.f}, {0.f, 1.f} };
					ImVec2 corners[4]{};
					for (int i = 0; i < 4; ++i)
					{
						float wx = 0.f, wy = 0.f;
						norm_to_world(g_map.meta, corners_norm[i][0], corners_norm[i][1], wx, wy);
						float sx = 0.f, sy = 0.f;
						world_to_screen_polar(wx, wy, g_smooth_local_wx, g_smooth_local_wy, g_smooth_local_eye,
							g_map.meta.scale, units, center, sx, sy);
						corners[i] = ImVec2(sx, sy);
					}
					dl->AddImageQuad(reinterpret_cast<ImTextureID>(g_map.srv),
						corners[0], corners[1], corners[2], corners[3],
						ImVec2(0, 0), ImVec2(1, 0), ImVec2(1, 1), ImVec2(0, 1), map_col);
				}
				else
				{
					const float map_x = center.x - origin_x * units;
					const float map_y = center.y - origin_y * units;
					dl->AddImage(reinterpret_cast<ImTextureID>(g_map.srv),
						ImVec2(map_x, map_y), ImVec2(map_x + units, map_y + units),
						ImVec2(0, 0), ImVec2(1, 1), map_col);
				}
			}

			if (cs2_circle)
			{
				if (g_dwm_alpha)
					erase_outside_circle(dl, center, radius);
				else
					draw_outside_circle_transparent_mask(dl, center, radius, IM_COL32(0, 0, 0, 255));
			}

			if (g_show_bomb_marker && data.contains("m_bomb") && data["m_bomb"].is_object())
			{
				load_ui_icon("c4", 20);
				const auto& bomb = data["m_bomb"];
				const float bx = bomb.value("x", 0.f);
				const float by = bomb.value("y", 0.f);
				if (bx != 0.f || by != 0.f)
				{
					float nx = 0.f, ny = 0.f;
					world_to_norm(g_map.meta, bx, by, nx, ny);
					float px = 0.f, py = 0.f;
					entity_to_screen(bx, by, nx, ny, rotate_mode, origin_x, origin_y, units, center, px, py);

					if (g_radar_full_map)
					{
						const float bomb_size = std::max(10.f, units * 0.018f);
						const bool planted = bomb.contains("m_blow_time");
						draw_cs2_bomb_marker(dl, px, py, bomb_size, planted);
					}
					else
					{
						const float dist = std::sqrt((px - center.x) * (px - center.x) + (py - center.y) * (py - center.y));
						if (dist <= radius - 4.f)
						{
							const float bomb_size = std::max(9.f, units * 0.016f);
							const bool planted = bomb.contains("m_blow_time");
							draw_cs2_bomb_marker(dl, px, py, bomb_size, planted);
						}
					}
				}
			}

			draw_radar_grenades(dl, data, rotate_mode, origin_x, origin_y, units, center, radius, cs2_circle);

			if (data.contains("m_players") && data["m_players"].is_array())
			{
				for (const auto& p : data["m_players"])
				{
					const int team = p.value("m_team", 0);
					if (team == 0)
						continue;

					const float wx = p.value("/m_position/x"_json_pointer, 0.f);
					const float wy = p.value("/m_position/y"_json_pointer, 0.f);
					float nx = 0.f, ny = 0.f;
					world_to_norm(g_map.meta, wx, wy, nx, ny);

					const bool dead = p.value("m_is_dead", false);
					const bool is_local = p.value("m_is_local", false);
					float px = 0.f, py = 0.f;
					entity_to_screen(wx, wy, nx, ny, rotate_mode, origin_x, origin_y, units, center, px, py);

					if (cs2_circle)
					{
						const float dist = std::sqrt((px - center.x) * (px - center.x) + (py - center.y) * (py - center.y));
						if (dist > radius - 3.f)
							continue;
					}

					const float dot_base = std::max(4.5f, units * 0.013f);
					const float dot_r = is_local ? dot_base * 1.05f : (dead ? dot_base * 0.7f : dot_base);
					const ImU32 border_col = player_dot_border_color(p, local_team, dead);
					const ImU32 fill_col = player_dot_fill_color(is_local, dead);
					const bool has_bomb = !dead && p.value("m_has_bomb", false);
					const float eye = p.value("m_eye_angle", 0.f);
					const float eye_cw = view_cone_cw(eye, is_local);

					draw_cs2_player_dot(dl, px, py, dot_r, border_col, fill_col, is_local, dead, eye_cw, has_bomb);

					if (g_show_view_cones && !dead)
						draw_cs2_view_arrow(dl, px, py, eye_cw, dot_r, border_col);
				}
			}

			if (cs2_circle)
				dl->AddCircle(center, radius, COL_RADAR_BORDER, 64, 1.8f);

			if (g_settings_open)
			{
				char hint[96]{};
				if (!g_radar_full_map)
					snprintf(hint, sizeof(hint), "ZOOM %.1fx  opacity %.0f%%", g_zoom, g_map_opacity * 100.f);
				else
					snprintf(hint, sizeof(hint), "FULL MAP  opacity %.0f%%", g_map_opacity * 100.f);
				dl->AddText(ImVec2(win_pos.x + 8.f, win_pos.y + win_sz.y - 18.f),
					IM_COL32(220, 190, 80, 180), hint);
			}
		}
		ImGui::End();

		if (cs2_circle)
		{
			ImGui::PopStyleVar(2);
		}
	}

	void draw_enemy_list_window(const nlohmann::json& data, int local_team)
	{
		(void)local_team;
		if (!g_show_enemy_list)
			return;
		if (!data.contains("m_players") || !data["m_players"].is_array())
			return;

		std::vector<nlohmann::json> enemies;
		enemies.reserve(5);
		for (const auto& p : data["m_players"])
		{
			const int team = p.value("m_team", 0);
			if (team == local_team || team == 0)
				continue;
			enemies.push_back(p);
			const std::string avatar_url = p.value("m_avatar_url", "");
			if (!avatar_url.empty())
				queue_avatar_load(avatar_url);
			else
				queue_avatar_load("https://avatars.fastly.steamstatic.com/fef49e7fa7e1997310d705b2a6158ff8dc1cdfeb_full.jpg");
		}

		if (enemies.empty())
			return;

		constexpr float row_h = 44.f;
		constexpr float row_gap = 4.f;
		constexpr float header_h = 22.f;
		constexpr float pad = 0.f;
		const float panel_w = std::max(340.f, g_list_w);
		const float list_h = header_h + 4.f + enemies.size() * row_h + (enemies.size() > 1 ? (enemies.size() - 1) * row_gap : 0.f) + 4.f;
		const float list_top = enemy_list_top();

		ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
			ImGuiWindowFlags_NoBackground;

		static bool list_layout_init = false;

		if (!g_settings_open)
		{
			list_layout_init = false;
			flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoInputs;
			ImGui::SetNextWindowPos(ImVec2(g_list_x, list_top), ImGuiCond_Always);
			ImGui::SetNextWindowSize(ImVec2(panel_w, list_h), ImGuiCond_Always);
		}
		else if (!list_layout_init)
		{
			ImGui::SetNextWindowPos(ImVec2(g_list_x, list_top), ImGuiCond_Always);
			ImGui::SetNextWindowSize(ImVec2(panel_w, list_h), ImGuiCond_Always);
			list_layout_init = true;
		}

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));

		if (ImGui::Begin("##enemies", nullptr, flags))
		{
			const ImVec2 win_pos = ImGui::GetWindowPos();
			const ImVec2 win_sz = ImGui::GetWindowSize();
			if (g_settings_open)
			{
				set_hit_rect(g_list_hit, win_pos, ImVec2(win_pos.x + win_sz.x, win_pos.y + win_sz.y));
				g_list_x = win_pos.x;
				g_list_y = win_pos.y;
				g_list_w = std::max(340.f, win_sz.x);
			}

			ImDrawList* dl = ImGui::GetWindowDrawList();

			load_ui_icon("health", 24);
			load_ui_icon("kevlar", 24);
			load_ui_icon("kevlar_helmet", 24);
			load_ui_icon("c4", 16);
			load_ui_icon("flashbang", 16);
			load_ui_icon("smokegrenade", 16);
			load_ui_icon("hegrenade", 16);
			load_ui_icon("incgrenade", 16);
			load_ui_icon("molotov", 16);
			load_ui_icon("decoy", 16);

			dl->AddText(ImVec2(win_pos.x, win_pos.y + 2.f),
				IM_COL32(220, 190, 80, 220), "ENEMIES");

			float row_y = win_pos.y + header_h + 4.f;
			for (size_t i = 0; i < enemies.size(); ++i)
			{
				const auto& enemy = enemies[i];
				const std::string weapon = enemy.value("/m_weapons/m_active"_json_pointer, "");
				const std::string primary = enemy.value("/m_weapons/m_primary"_json_pointer, "");
				const std::string display_weapon = !primary.empty() ? primary : weapon;
				if (!display_weapon.empty())
					load_ui_icon(display_weapon, 32);

				if (enemy.value("m_has_bomb", false))
					load_ui_icon("c4", 16);

				if (enemy.contains("/m_weapons/m_utilities"_json_pointer)
					&& enemy["/m_weapons/m_utilities"_json_pointer].is_array())
				{
					for (const auto& util : enemy["/m_weapons/m_utilities"_json_pointer])
					{
						if (util.is_string())
							load_ui_icon(util.get<std::string>(), 16);
					}
				}

				const std::string model = enemy.value("m_model_name", "");
				if (!model.empty())
					load_character_icon(model, 64);

				draw_enemy_row(dl, ImVec2(win_pos.x, row_y), win_sz.x, enemy);
				row_y += row_h + row_gap;
			}
		}
		ImGui::End();
		ImGui::PopStyleVar();
	}

	void draw_settings_scrim()
	{
		if (!g_settings_open)
			return;

		ImGui::SetNextWindowPos(ImVec2(0.f, 0.f), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(static_cast<float>(g_win_w), static_cast<float>(g_win_h)), ImGuiCond_Always);

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.02f, 0.03f, 0.06f, 0.58f));

		const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav;

		if (ImGui::Begin("##settings_scrim", nullptr, flags))
		{
			const ImVec2 win_pos = ImGui::GetWindowPos();
			const ImVec2 win_sz = ImGui::GetWindowSize();
			ImGui::GetWindowDrawList()->AddRectFilled(
				win_pos,
				ImVec2(win_pos.x + win_sz.x, win_pos.y + win_sz.y),
				IM_COL32(4, 6, 12, 145));

			ImGui::SetCursorPos(ImVec2(16.f, 16.f));
			ImGui::TextDisabled("EDIT MODE  |  DEL / F9 close  |  drag radar / enemies / settings");
		}
		ImGui::End();

		ImGui::PopStyleColor();
		ImGui::PopStyleVar(2);
	}

	void draw_settings_window()
	{
		static bool settings_layout_init = false;
		if (!g_settings_open)
		{
			settings_layout_init = false;
			return;
		}

		const float settings_x = std::min(g_radar_x + g_radar_size + 14.f,
			static_cast<float>(g_win_w) - 250.f);
		const float settings_y = std::max(8.f, g_radar_y);

		if (!settings_layout_init)
		{
			ImGui::SetNextWindowPos(ImVec2(settings_x, settings_y), ImGuiCond_Always);
			ImGui::SetNextWindowSize(ImVec2(236.f, 320.f), ImGuiCond_Always);
			settings_layout_init = true;
		}

		const ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;

		if (ImGui::Begin("Overlay Settings", nullptr, flags))
		{
			const ImVec2 win_pos = ImGui::GetWindowPos();
			const ImVec2 win_sz = ImGui::GetWindowSize();
			set_hit_rect(g_settings_hit, win_pos, ImVec2(win_pos.x + win_sz.x, win_pos.y + win_sz.y));

			ImGui::TextDisabled("DEL or F9 to close");
			ImGui::Separator();

			ImGui::SliderFloat("Map opacity", &g_map_opacity, 0.15f, 1.f, "%.0f%%");
			if (!g_radar_full_map)
				ImGui::SliderFloat("Radar zoom", &g_zoom, 1.f, 8.f, "%.1fx");

			ImGui::Checkbox("Enemy list", &g_show_enemy_list);
			ImGui::Checkbox("Full map box", &g_radar_full_map);
			ImGui::Checkbox("Rotate map (cl_radar_rotate 1)", &g_radar_rotate);
			ImGui::Checkbox("View arrows", &g_show_view_cones);
			ImGui::Checkbox("Grenades", &g_show_grenades);
			ImGui::Checkbox("Bomb marker", &g_show_bomb_marker);

			ImGui::Separator();
			ImGui::TextDisabled("ON  = cl_radar_rotate 1 (default, polar)");
			ImGui::TextDisabled("OFF = cl_radar_rotate 0 (north-up/web)");
			ImGui::TextDisabled("Drag radar / enemy list with mouse");
			ImGui::TextDisabled("M toggles full-map mode");
		}
		ImGui::End();
	}

	void update_local_player(const nlohmann::json& data)
	{
		g_local_found = false;
		g_local_x = 0.5f;
		g_local_y = 0.5f;

		if (!data.contains("m_players") || !data["m_players"].is_array())
			return;

		for (const auto& p : data["m_players"])
		{
			if (!p.value("m_is_local", false))
				continue;

			world_to_norm(g_map.meta,
				p.value("/m_position/x"_json_pointer, 0.f),
				p.value("/m_position/y"_json_pointer, 0.f),
				g_local_x, g_local_y);
			g_local_wx = p.value("/m_position/x"_json_pointer, 0.f);
			g_local_wy = p.value("/m_position/y"_json_pointer, 0.f);
			g_local_eye_angle = p.value("m_eye_angle", 0.f);
			g_local_found = true;
		}

		static bool smooth_init = false;
		if (g_local_found)
		{
			if (!smooth_init)
			{
				g_smooth_local_x = g_local_x;
				g_smooth_local_y = g_local_y;
				g_smooth_local_wx = g_local_wx;
				g_smooth_local_wy = g_local_wy;
				g_smooth_local_eye = g_local_eye_angle;
				smooth_init = true;
			}
			else
			{
				constexpr float pos_speed = 10.f;
				constexpr float eye_speed = 13.f;
				g_smooth_local_x = smooth_exp(g_smooth_local_x, g_local_x, pos_speed);
				g_smooth_local_y = smooth_exp(g_smooth_local_y, g_local_y, pos_speed);
				g_smooth_local_wx = smooth_exp(g_smooth_local_wx, g_local_wx, pos_speed);
				g_smooth_local_wy = smooth_exp(g_smooth_local_wy, g_local_wy, pos_speed);
				g_smooth_local_eye = smooth_angle_exp(g_smooth_local_eye, g_local_eye_angle, eye_speed);
			}
		}
		else
		{
			smooth_init = false;
			g_smooth_local_x = 0.5f;
			g_smooth_local_y = 0.5f;
			g_local_eye_angle = 0.f;
			g_local_wx = 0.f;
			g_local_wy = 0.f;
			g_smooth_local_wx = 0.f;
			g_smooth_local_wy = 0.f;
			g_smooth_local_eye = 0.f;
		}
	}

	void render_frame(const nlohmann::json& data)
	{
		g_radar_hit.valid = false;
		g_list_hit.valid = false;
		g_settings_hit.valid = false;

		update_local_player(data);
		process_avatar_pending();

		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		ImGuiIO& io = ImGui::GetIO();
		io.MouseDrawCursor = g_settings_open;
		if (!g_settings_open)
			io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
		else
			io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;

		const int local_team = data.value("m_local_team", 0);
		if (g_settings_open)
			draw_settings_scrim();
		draw_radar_window(data, local_team);
		draw_enemy_list_window(data, local_team);
		draw_settings_window();

		ImGui::Render();
		const float clear_transparent[4] = { 0.f, 0.f, 0.f, 0.f };
		const float clear_colorkey[4] = { 0.f, 0.f, 0.f, 1.f };
		g_ctx->OMSetRenderTargets(1, &g_rtv, nullptr);
		g_ctx->ClearRenderTargetView(g_rtv, g_dwm_alpha ? clear_transparent : clear_colorkey);
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		if (g_bs_default)
		{
			const float factor[4] = { 0.f, 0.f, 0.f, 0.f };
			g_ctx->OMSetBlendState(g_bs_default, factor, 0xffffffff);
		}
		g_swap->Present(g_settings_open ? 0 : 1, 0);
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
			g_win_w = w;
			g_win_h = h;
			if (g_swap)
			{
				safe_release(g_rtv);
				g_swap->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, 0);
				create_render_target();
			}
			clear_map_cache();
		}

		g_win_x = tl.x;
		g_win_y = tl.y;

		UINT flags = SWP_NOACTIVATE | SWP_SHOWWINDOW;
		if (!moved)
			flags |= SWP_NOMOVE;
		if (!resized)
			flags |= SWP_NOSIZE;

		const bool was_shown = g_overlay_shown;
		SetWindowPos(g_hwnd, HWND_TOPMOST, tl.x, tl.y, w, h, flags);
		g_overlay_shown = true;

		if (!g_settings_open && !was_shown)
			focus_cs2_window();
	}

	LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
	{
		if (msg == WM_NCHITTEST)
		{
			if (!g_settings_open)
				return HTTRANSPARENT;

			return HTCLIENT;
		}

		if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp))
			return true;

		if (msg == WM_CLOSE)
		{
			overlay_imgui::stop();
			return 0;
		}

		if (msg == WM_SIZE && wp != SIZE_MINIMIZED && g_swap)
		{
			safe_release(g_rtv);
			g_swap->ResizeBuffers(0, LOWORD(lp), HIWORD(lp), DXGI_FORMAT_UNKNOWN, 0);
			create_render_target();
			return 0;
		}

		return DefWindowProcW(hwnd, msg, wp, lp);
	}

	std::string create_overlay_window()
	{
		const wchar_t* cls = L"AimSyncOverlayImGui";
		WNDCLASSEXW wc{};
		wc.cbSize = sizeof(wc);
		wc.style = CS_CLASSDC;
		wc.lpfnWndProc = wnd_proc;
		wc.hInstance = GetModuleHandleW(nullptr);
		wc.hCursor = nullptr;
		wc.lpszClassName = cls;
		RegisterClassExW(&wc);

		g_hwnd = CreateWindowExW(
			WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
			cls, L"AimSync Overlay",
			WS_POPUP,
			g_win_x, g_win_y, g_win_w, g_win_h,
			nullptr, nullptr, wc.hInstance, nullptr);

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

		if (const auto err = create_overlay_window(); !err.empty())
		{
			if (com_owned)
				CoUninitialize();
			ready.set_value(err);
			return;
		}

		if (const auto err = init_d3d(g_hwnd); !err.empty())
		{
			DestroyWindow(g_hwnd);
			g_hwnd = nullptr;
			if (com_owned)
				CoUninitialize();
			ready.set_value(err);
			return;
		}

		g_visible = true;
		g_settings_open = false;
		apply_passive_overlay_window(g_hwnd);
		start_avatar_worker();

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
			g_cs2_hwnd = game;
			const bool cs2_ready = game && IsWindowVisible(game) && !IsIconic(game);
			const bool active = g_visible && (g_settings_open ? cs2_ready : should_draw_overlay(game));
			sync_cs2_window(game, active);

			if (active)
			{
				nlohmann::json data = nlohmann::json::object();
				{
					std::lock_guard lock(g_data_mtx);
					data = g_frame_data;
				}
				render_frame(data);
			}
			else
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(16));
			}
		}

		save_overlay_settings();
		set_settings_mode(false);
		stop_avatar_worker();

		if (g_hwnd)
		{
			DestroyWindow(g_hwnd);
			g_hwnd = nullptr;
		}
		g_overlay_shown = false;

		cleanup_d3d();

		if (com_owned)
			CoUninitialize();
	}
}

bool overlay_imgui::is_running()
{
	return g_running.load();
}

bool overlay_imgui::start(std::string& error_out)
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

	LOG_INFO("imgui overlay started");
	return true;
}

void overlay_imgui::stop()
{
	if (!g_running.load())
		return;

	g_running = false;
	if (g_msg_thread.joinable())
		g_msg_thread.join();

	LOG_INFO("imgui overlay stopped");
}

void overlay_imgui::render(const nlohmann::json& data)
{
	if (!g_running.load())
		return;

	std::lock_guard lock(g_data_mtx);
	g_frame_data = data;
}

void overlay_imgui::shutdown()
{
	stop();
}
