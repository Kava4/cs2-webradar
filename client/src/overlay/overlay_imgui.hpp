#pragma once

#include <nlohmann/json.hpp>
#include <string>

// ImGui + D3D11 overlay backend (Hurracan-style input handling).
namespace overlay_imgui
{
	bool is_running();
	bool start(std::string& error_out);
	void stop();
	void render(const nlohmann::json& data);
	void shutdown();
}
