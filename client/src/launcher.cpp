#include "pch.hpp"
#include "launcher.hpp"

#include <shellapi.h>
#pragma comment(lib, "shell32.lib")

void launcher::open_firewall_ports()
{
	const wchar_t* cmds[] = {
		LR"(netsh advfirewall firewall delete rule name="CS2 WebRadar - HTTP")",
		LR"(netsh advfirewall firewall delete rule name="CS2 WebRadar - WebSocket")",
		LR"(netsh advfirewall firewall delete rule name="AImSync WebRadar - HTTP")",
		LR"(netsh advfirewall firewall delete rule name="AImSync WebRadar - WebSocket")",
		LR"(netsh advfirewall firewall delete rule name="AimSync WebRadar - HTTP")",
		LR"(netsh advfirewall firewall delete rule name="AimSync WebRadar - WebSocket")",
		LR"(netsh advfirewall firewall add rule name="AimSync WebRadar - HTTP" dir=in action=allow protocol=TCP localport=5173)",
		LR"(netsh advfirewall firewall add rule name="AimSync WebRadar - WebSocket" dir=in action=allow protocol=TCP localport=22006)",
	};

	for (const auto* cmd : cmds)
	{
		STARTUPINFOW si{};
		si.cb = sizeof(si);
		si.dwFlags = STARTF_USESHOWWINDOW;
		si.wShowWindow = SW_HIDE;
		PROCESS_INFORMATION pi{};
		std::wstring c = L"cmd.exe /C " + std::wstring(cmd);
		if (CreateProcessW(nullptr, c.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
		{
			WaitForSingleObject(pi.hProcess, 3000);
			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
		}
	}
}

std::string launcher::get_lan_ip()
{
	std::string ip_str = "localhost";
	WSADATA wsa{};
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return ip_str;

	char hostname[256]{};
	if (gethostname(hostname, sizeof(hostname)) == 0)
	{
		const hostent* he = gethostbyname(hostname);
		if (he)
		{
			for (int i = 0; he->h_addr_list[i]; ++i)
			{
				const auto* addr = reinterpret_cast<in_addr*>(he->h_addr_list[i]);
				std::string cand = inet_ntoa(*addr);
				if (cand.rfind("192.168.", 0) == 0)
				{
					ip_str = cand;
					break;
				}
				if (cand != "127.0.0.1" && ip_str == "localhost")
					ip_str = cand;
			}
		}
	}
	WSACleanup();
	return ip_str;
}

void launcher::open_browser_async(const std::string& ip)
{
	const std::string host = ip.empty() ? get_lan_ip() : ip;
	const std::wstring url = std::wstring(L"http://") +
		std::wstring(host.begin(), host.end()) + L":5173";

	std::thread([url]() {
		std::this_thread::sleep_for(std::chrono::milliseconds(1200));
		ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
		}).detach();
}
