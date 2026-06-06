#pragma once

#include <filesystem>
#include <string>

namespace appdata
{
	std::filesystem::path root();
	std::filesystem::path config_path();
	std::filesystem::path overlay_path();
	std::filesystem::path maps_dir();
	std::filesystem::path map_dir(const std::string& map_name);
	std::filesystem::path exe_dir();

	bool ensure();
	bool sync_maps();
	bool sync_map(const std::string& map_name);
	bool ensure_map_assets(const std::string& map_name);
	std::string normalize_map_name(const std::string& map_name);
}
