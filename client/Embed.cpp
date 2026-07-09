#include "pch.hpp"
#include "Embed.hpp"
#include "resource_ids.h"

#include <tlhelp32.h>
#include <cwctype>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

namespace
{
	bool equals_ignore_case(const std::wstring_view a, const std::wstring_view b)
	{
		return a.size() == b.size()
			&& std::equal(a.begin(), a.end(), b.begin(),
				[](wchar_t x, wchar_t y)
				{
					return towlower(x) == towlower(y);
				});
	}

	bool http_probe(uint16_t port)
	{
		const auto session = WinHttpOpen(L"AimSyncWebRadar/1.0",
			WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
		if (!session)
			return false;

		const auto connect = WinHttpConnect(session, L"127.0.0.1", port, 0);
		if (!connect)
		{
			WinHttpCloseHandle(session);
			return false;
		}

		const auto request = WinHttpOpenRequest(connect, L"GET", L"/",
			nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
		if (!request)
		{
			WinHttpCloseHandle(connect);
			WinHttpCloseHandle(session);
			return false;
		}

		const bool ok = WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
			WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
			&& WinHttpReceiveResponse(request, nullptr);

		WinHttpCloseHandle(request);
		WinHttpCloseHandle(connect);
		WinHttpCloseHandle(session);
		return ok;
	}

	bool wait_for_server(const DWORD timeout_ms = 10000)
	{
		const auto deadline = GetTickCount64() + timeout_ms;
		while (GetTickCount64() < deadline)
		{
			if (embed::is_server_responding())
				return true;

			if (embed::g_server_process)
			{
				DWORD code = STILL_ACTIVE;
				GetExitCodeProcess(embed::g_server_process, &code);
				if (code != STILL_ACTIVE)
					return embed::is_server_responding();
			}

			Sleep(200);
		}

		return embed::is_server_responding();
	}
}

std::wstring embed::server_temp_path()
{
	wchar_t tmp[MAX_PATH]{};
	GetTempPathW(MAX_PATH, tmp);
	return std::wstring(tmp) + L"aimsync_webradar_server.exe";
}

bool embed::is_server_responding()
{
	return http_probe(5173) && http_probe(22006);
}

void embed::kill_orphan_servers()
{
	const auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snapshot == INVALID_HANDLE_VALUE)
		return;

	PROCESSENTRY32W entry{};
	entry.dwSize = sizeof(entry);

	if (Process32FirstW(snapshot, &entry))
	{
		do
		{
			if (!equals_ignore_case(entry.szExeFile, L"aimsync_webradar_server.exe"))
				continue;

			if (g_server_process)
			{
				const DWORD our_pid = GetProcessId(g_server_process);
				if (entry.th32ProcessID == our_pid)
					continue;
			}

			const auto process = OpenProcess(PROCESS_TERMINATE, FALSE, entry.th32ProcessID);
			if (!process)
				continue;

			TerminateProcess(process, 0);
			CloseHandle(process);
		} while (Process32NextW(snapshot, &entry));
	}

	CloseHandle(snapshot);
}

bool embed::extract_server()
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

	kill_orphan_servers();
	Sleep(300);

	HANDLE hf = CreateFileW(dest.c_str(), GENERIC_WRITE, 0, nullptr,
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (hf == INVALID_HANDLE_VALUE)
		return false;

	DWORD written = 0;
	const bool ok = WriteFile(hf, data, size, &written, nullptr) && written == size;
	CloseHandle(hf);
	return ok;
}

bool embed::ensure_server_running()
{
	if (g_server_process)
	{
		DWORD code = STILL_ACTIVE;
		GetExitCodeProcess(g_server_process, &code);
		if (code == STILL_ACTIVE && is_server_responding())
			return true;

		CloseHandle(g_server_process);
		g_server_process = nullptr;
	}

	if (is_server_responding())
		return true;

	kill_orphan_servers();
	Sleep(400);

	if (is_server_responding())
		return true;

	if (!extract_server())
	{
		LOG_ERROR("failed to extract embedded web server");
		return false;
	}

	const std::wstring srv = server_temp_path();
	std::wstring cmd = L"\"" + srv + L"\"";

	STARTUPINFOW si{};
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;

	PROCESS_INFORMATION pi{};
	if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr,
		FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
	{
		LOG_ERROR("CreateProcess failed for web server (error %lu)", GetLastError());

		if (is_server_responding())
			return true;

		return false;
	}

	g_server_process = pi.hProcess;
	CloseHandle(pi.hThread);

	if (!wait_for_server())
	{
		LOG_ERROR("web server did not start (ports 5173/22006 not responding)");
		kill_orphan_servers();
		g_server_process = nullptr;
		return is_server_responding();
	}

	return true;
}

void embed::kill_server()
{
	kill_orphan_servers();

	if (g_server_process)
	{
		TerminateProcess(g_server_process, 0);
		CloseHandle(g_server_process);
		g_server_process = nullptr;
	}
}

bool embed::has_embedded_server()
{
	const HMODULE self = GetModuleHandleW(nullptr);
	const HRSRC res = FindResourceW(self, MAKEINTRESOURCEW(IDR_SERVER_EXE), MAKEINTRESOURCEW(RT_RCDATA));
	return res != nullptr;
}
