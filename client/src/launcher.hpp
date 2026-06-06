#pragma once

#include <string>

namespace launcher
{
	void open_firewall_ports();
	std::string get_lan_ip();
	void open_browser_async(const std::string& ip = "");
}
