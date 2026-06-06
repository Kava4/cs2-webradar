#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace overlay
{
	bool is_running();
	bool start(std::string& error_out);
	void stop();
	void render(const nlohmann::json& data);
	void shutdown();
}
