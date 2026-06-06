#include "pch.hpp"
#include "appdata.hpp"

static bool write_default_config(const std::filesystem::path& path)
{
	std::error_code ec;
	std::filesystem::create_directories(path.parent_path(), ec);

	std::ofstream file(path);
	if (!file.is_open())
		return false;

	file << std::format("{}", R"({
    "m_ip": "localhost"
})");
	return true;
}

bool cfg::setup(config_data_t& config_data)
{
	config_data = config_data_t{};
	config_data.m_ip = "localhost";

	appdata::ensure();

	const auto path = appdata::config_path();
	std::ifstream file(path);
	if (!file.is_open())
	{
		LOG_WARNING("config.json not found — creating defaults at %s", path.string().c_str());
		if (!write_default_config(path))
			LOG_WARNING("could not write config.json — using built-in defaults");
		return true;
	}

	try
	{
		const auto parsed_data = nlohmann::json::parse(file);
		if (parsed_data.is_null() || parsed_data.empty())
			return true;

		config_data = parsed_data.get<config_data_t>();

		if (config_data.m_ip.empty())
			config_data.m_ip = "localhost";
	}
	catch (const std::exception& e)
	{
		LOG_WARNING("config parse error: %s — using defaults", e.what());
		config_data.m_ip = "localhost";
	}

	return true;
}

bool cfg::save(const config_data_t& config_data)
{
	try
	{
		appdata::ensure();
		const auto path = appdata::config_path();
		std::ofstream file(path);
		if (!file.is_open())
			return false;
		file << nlohmann::json(config_data).dump(4);
		return true;
	}
	catch (...)
	{
		return false;
	}
}
