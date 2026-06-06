#pragma once

#include <functional>
#include <string>

namespace launcher_gui
{
	using start_radar_fn_t = std::function<bool(std::string& error_out)>;
	using stop_radar_fn_t = std::function<void()>;

	int run(HINSTANCE instance, start_radar_fn_t start_radar, stop_radar_fn_t stop_radar = {});
	void show();
	void request_shutdown();
}
