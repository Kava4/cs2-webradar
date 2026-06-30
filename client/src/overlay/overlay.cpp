#include "pch.hpp"
#include "overlay.hpp"
#include "overlay_imgui.hpp"

bool overlay::is_running()
{
	return overlay_imgui::is_running();
}

bool overlay::start(std::string& error_out)
{
	return overlay_imgui::start(error_out);
}

void overlay::stop()
{
	overlay_imgui::stop();
}

void overlay::render(const nlohmann::json& data)
{
	overlay_imgui::render(data);
}

void overlay::shutdown()
{
	overlay_imgui::shutdown();
}
