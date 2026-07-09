#pragma once

#include <windows.h>
#include <string>

namespace embed
{
	inline HANDLE g_server_process = nullptr;

	std::wstring server_temp_path();
	bool extract_server();
	bool is_server_responding();
	void kill_orphan_servers();
	bool ensure_server_running();
	void kill_server();
	bool has_embedded_server();
}
