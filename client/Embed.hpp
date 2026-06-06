#pragma once

#include <windows.h>
#include <string>
#include <filesystem>
#include "resource_ids.h"

namespace embed
{
	inline HANDLE g_server_process = nullptr;

	inline std::wstring server_temp_path()
	{
		wchar_t tmp[MAX_PATH]{};
		GetTempPathW(MAX_PATH, tmp);
		return std::wstring(tmp) + L"aimsync_webradar_server.exe";
	}

	inline bool extract_server()
	{
		const HMODULE self = GetModuleHandleW(nullptr);
		const HRSRC res = FindResourceW(self, MAKEINTRESOURCEW(IDR_SERVER_EXE), MAKEINTRESOURCEW(RT_RCDATA));
		if (!res)
			return false;

		const HGLOBAL hg = LoadResource(self, res);
		if (!hg)
			return false;

		const void* data = LockResource(hg);
		const DWORD size = SizeofResource(self, res);
		if (!data || size == 0)
			return false;

		const std::wstring dest = server_temp_path();

		if (std::filesystem::exists(dest))
		{
			const auto existing_size = std::filesystem::file_size(dest);
			if (existing_size == size)
				return true;
		}

		HANDLE hf = CreateFileW(dest.c_str(), GENERIC_WRITE, 0, nullptr,
			CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (hf == INVALID_HANDLE_VALUE)
			return false;

		DWORD written = 0;
		const bool ok = WriteFile(hf, data, size, &written, nullptr) && written == size;
		CloseHandle(hf);
		return ok;
	}

	inline bool ensure_server_running()
	{
		if (g_server_process)
		{
			DWORD code = STILL_ACTIVE;
			GetExitCodeProcess(g_server_process, &code);
			if (code == STILL_ACTIVE)
				return true;
			CloseHandle(g_server_process);
			g_server_process = nullptr;
		}

		if (!extract_server())
			return false;

		const std::wstring srv = server_temp_path();
		std::wstring cmd = L"\"" + srv + L"\"";

		STARTUPINFOW si{};
		si.cb = sizeof(si);
		si.dwFlags = STARTF_USESHOWWINDOW;
		si.wShowWindow = SW_HIDE;

		PROCESS_INFORMATION pi{};
		if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr,
			FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
			return false;

		g_server_process = pi.hProcess;
		CloseHandle(pi.hThread);
		return true;
	}

	inline void kill_server()
	{
		if (g_server_process)
		{
			TerminateProcess(g_server_process, 0);
			CloseHandle(g_server_process);
			g_server_process = nullptr;
		}
	}

	inline bool has_embedded_server()
	{
		const HMODULE self = GetModuleHandleW(nullptr);
		const HRSRC res = FindResourceW(self, MAKEINTRESOURCEW(IDR_SERVER_EXE), MAKEINTRESOURCEW(RT_RCDATA));
		return res != nullptr;
	}
}
