#pragma once

namespace tray
{
	bool init(HINSTANCE instance, HICON icon = nullptr);
	void shutdown();
	int run_message_loop();
}
