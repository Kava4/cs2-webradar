#include "pch.hpp"
#include "appdata.hpp"

#include <shlobj.h>
#include <algorithm>
#include <vector>
#pragma comment(lib, "shell32.lib")

namespace
{
	std::filesystem::path canonical_path(const std::filesystem::path& path)
	{
		std::error_code ec;
		const auto resolved = std::filesystem::weakly_canonical(path, ec);
		return ec ? path : resolved;
	}

	bool has_map_image(const std::filesystem::path& map_dir)
	{
		return std::filesystem::exists(map_dir / "radar.png")
			|| std::filesystem::exists(map_dir / "background.png");
	}

	std::vector<std::filesystem::path> bundled_map_roots()
	{
		const auto base = appdata::exe_dir();
		const std::filesystem::path candidates[] = {
			base / "data",
			base / ".." / "data",
			base / ".." / ".." / "data",
		};

		std::vector<std::filesystem::path> roots;
		for (const auto& candidate : candidates)
		{
			const auto resolved = canonical_path(candidate);
			if (!std::filesystem::exists(resolved))
				continue;
			if (std::find(roots.begin(), roots.end(), resolved) == roots.end())
				roots.push_back(resolved);
		}
		return roots;
	}

	bool copy_map_files(const std::filesystem::path& src, const std::filesystem::path& dst)
	{
		if (!std::filesystem::exists(src))
			return false;

		std::error_code ec;
		std::filesystem::create_directories(dst, ec);

		bool copied = false;
		for (const auto& entry : std::filesystem::directory_iterator(src, ec))
		{
			if (!entry.is_regular_file())
				continue;

			const auto dest_file = dst / entry.path().filename();
			const bool missing = !std::filesystem::exists(dest_file);
			const bool newer = !missing
				&& std::filesystem::last_write_time(entry.path()) > std::filesystem::last_write_time(dest_file);

			if (missing || newer)
			{
				std::filesystem::copy_file(entry.path(), dest_file,
					std::filesystem::copy_options::overwrite_existing, ec);
				copied = true;
			}
		}

		return copied || has_map_image(dst) || std::filesystem::exists(dst / "data.json");
	}

	bool find_bundled_map(const std::string& map_name, std::filesystem::path& out_src)
	{
		for (const auto& root : bundled_map_roots())
		{
			const auto candidate = root / map_name;
			if (std::filesystem::exists(candidate / "data.json") || has_map_image(candidate))
			{
				out_src = candidate;
				return true;
			}
		}
		return false;
	}

	size_t curl_write_file(void* contents, size_t size, size_t nmemb, FILE* file)
	{
		return fwrite(contents, size, nmemb, file);
	}

	bool download_url_to_file(const std::string& url, const std::filesystem::path& dest)
	{
		std::error_code ec;
		std::filesystem::create_directories(dest.parent_path(), ec);

		FILE* file = nullptr;
		_wfopen_s(&file, dest.c_str(), L"wb");
		if (!file)
			return false;

		const auto curl = curl_easy_init();
		if (!curl)
		{
			fclose(file);
			return false;
		}

		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_file);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 12L);
		curl_easy_setopt(curl, CURLOPT_USERAGENT, "AimSyncWebRadar/1.0");

		const bool ok = curl_easy_perform(curl) == CURLE_OK;
		curl_easy_cleanup(curl);
		fclose(file);

		if (!ok || !std::filesystem::exists(dest) || std::filesystem::file_size(dest) == 0)
		{
			std::filesystem::remove(dest);
			return false;
		}
		return true;
	}

	bool download_map_file(const std::string& map_name, const char* filename)
	{
		const auto dest = appdata::map_dir(map_name) / filename;
		if (std::filesystem::exists(dest) && std::filesystem::file_size(dest) > 0)
			return true;

		const std::string urls[] = {
			std::format("http://127.0.0.1:5173/data/{}/{}", map_name, filename),
			std::format("http://localhost:5173/data/{}/{}", map_name, filename),
		};

		for (const auto& url : urls)
		{
			if (download_url_to_file(url, dest))
			{
				LOG_INFO("downloaded map asset %s/%s", map_name.c_str(), filename);
				return true;
			}
		}
		return false;
	}

	void migrate_legacy_config()
	{
		const auto dest = appdata::config_path();
		if (std::filesystem::exists(dest))
			return;

		const std::filesystem::path legacy_paths[] = {
			appdata::exe_dir() / "config.json",
			std::filesystem::current_path() / "config.json",
		};

		for (const auto& legacy : legacy_paths)
		{
			if (!std::filesystem::exists(legacy))
				continue;

			std::error_code ec;
			std::filesystem::create_directories(dest.parent_path(), ec);
			std::filesystem::copy_file(legacy, dest, std::filesystem::copy_options::overwrite_existing, ec);
			if (!ec)
				LOG_INFO("migrated config.json to %s", dest.string().c_str());
			return;
		}
	}
}

std::filesystem::path appdata::exe_dir()
{
	wchar_t path[MAX_PATH]{};
	GetModuleFileNameW(nullptr, path, MAX_PATH);
	return std::filesystem::path(path).parent_path();
}

std::filesystem::path appdata::root()
{
	wchar_t appdata_path[MAX_PATH]{};
	if (FAILED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, appdata_path)))
		return exe_dir() / "AimSync";

	return std::filesystem::path(appdata_path) / "AimSync";
}

std::filesystem::path appdata::config_path()
{
	return root() / "config.json";
}

std::filesystem::path appdata::overlay_path()
{
	return root() / "overlay.json";
}

std::filesystem::path appdata::maps_dir()
{
	return root() / "data";
}

std::filesystem::path appdata::map_dir(const std::string& map_name)
{
	return maps_dir() / normalize_map_name(map_name);
}

std::string appdata::normalize_map_name(const std::string& map_name)
{
	std::string name = map_name;
	while (!name.empty() && (name.back() == '/' || name.back() == '\\'))
		name.pop_back();

	const auto pos = name.find_last_of("/\\");
	if (pos != std::string::npos)
		name = name.substr(pos + 1);

	return name;
}

bool appdata::ensure()
{
	std::error_code ec;
	std::filesystem::create_directories(maps_dir(), ec);
	if (ec)
	{
		LOG_WARNING("could not create AimSync app data folder: %s", root().string().c_str());
		return false;
	}

	migrate_legacy_config();
	return true;
}

bool appdata::sync_map(const std::string& raw_map_name)
{
	const auto map_name = normalize_map_name(raw_map_name);
	if (map_name.empty() || map_name == "invalid")
		return false;

	const auto dest = map_dir(map_name);
	if (has_map_image(dest) && std::filesystem::exists(dest / "data.json"))
		return true;

	std::filesystem::path src;
	if (find_bundled_map(map_name, src))
		copy_map_files(src, dest);

	return has_map_image(dest) || std::filesystem::exists(dest / "data.json");
}

bool appdata::ensure_map_assets(const std::string& raw_map_name)
{
	const auto map_name = normalize_map_name(raw_map_name);
	if (map_name.empty() || map_name == "invalid")
		return false;

	ensure();
	sync_map(map_name);

	const auto dest = map_dir(map_name);
	if (!std::filesystem::exists(dest / "data.json"))
	{
		std::filesystem::path src;
		if (find_bundled_map(map_name, src))
			copy_map_files(src, dest);
	}

	if (!has_map_image(dest))
	{
		std::filesystem::path src;
		if (find_bundled_map(map_name, src))
			copy_map_files(src, dest);
	}

	if (!std::filesystem::exists(dest / "data.json") || !has_map_image(dest))
	{
		download_map_file(map_name, "data.json");
		if (!has_map_image(dest))
		{
			download_map_file(map_name, "radar.png");
			if (!has_map_image(dest))
				download_map_file(map_name, "background.png");
		}
	}

	return has_map_image(dest);
}

bool appdata::sync_maps()
{
	std::error_code ec;
	std::filesystem::create_directories(maps_dir(), ec);

	bool synced = false;
	for (const auto& root : bundled_map_roots())
	{
		for (const auto& entry : std::filesystem::directory_iterator(root, ec))
		{
			if (!entry.is_directory())
				continue;
			if (!std::filesystem::exists(entry.path() / "data.json") && !has_map_image(entry.path()))
				continue;

			const auto map_name = entry.path().filename().string();
			if (copy_map_files(entry.path(), map_dir(map_name)))
				synced = true;
		}
	}
	return synced;
}
