#include "pch.hpp"
#include "../Embed.hpp"
#include "launcher.hpp"
#include "launcher_gui.hpp"
#include "overlay/overlay.hpp"
#include "tray.hpp"
#include "utils/appdata.hpp"

static HANDLE g_mutex = nullptr;
static std::atomic<bool> g_running{ false };

struct radar_runtime_t
{
	std::unique_ptr<ix::WebSocket> web_socket;
	std::thread thread;
};

static radar_runtime_t g_radar;

static void cleanup()
{
	overlay::shutdown();
	embed::kill_server();
	tray::shutdown();
	if (g_mutex)
	{
		CloseHandle(g_mutex);
		g_mutex = nullptr;
	}
}

static bool start_radar(HINSTANCE instance, std::string& error_out)
{
	config_data_t config_data = {};
	if (!cfg::setup(config_data))
	{
		error_out = "Failed to load configuration.";
		return false;
	}
	LOG_INFO("config system initialization completed");

	if (!m_memory->setup(error_out))
		return false;
	LOG_INFO("memory initialization completed");

	if (!i::setup())
	{
		error_out = "Failed to resolve game interfaces.\n\nThe game may have updated.";
		return false;
	}
	LOG_INFO("interfaces initialization completed");

	if (!schema::setup())
	{
		error_out = "Failed to build schema table.\n\nTry restarting CS2.";
		return false;
	}
	LOG_INFO("schema initialization completed");

	ix::initNetSystem();
	LOG_INFO("winsock initialization completed");

	const auto ws_host = config_data.m_ip.empty() ? "localhost" : config_data.m_ip;
	const auto formatted_address = std::format("ws://{}:22006/aimsync_webradar", ws_host);

	g_radar.web_socket = std::make_unique<ix::WebSocket>();
	auto& web_socket = *g_radar.web_socket;

	std::mutex handshake_mutex;
	std::condition_variable handshake_cv;
	bool connected = false;
	bool failed = false;

	web_socket.setUrl(formatted_address);
	web_socket.setOnMessageCallback([&](const ix::WebSocketMessagePtr& msg)
	{
		if (msg->type == ix::WebSocketMessageType::Open)
		{
			{
				std::lock_guard lock(handshake_mutex);
				connected = true;
			}
			handshake_cv.notify_one();
			LOG_INFO("connected to the web socket ('%s')", formatted_address.c_str());
		}
		else if (msg->type == ix::WebSocketMessageType::Message)
		{
			try
			{
				const auto j = nlohmann::json::parse(msg->str);
				if (j.value("cmd", "") == "shutdown")
				{
					LOG_INFO("shutdown requested from web UI");
					launcher_gui::request_shutdown();
				}
			}
			catch (...) {}
		}
		else if (msg->type == ix::WebSocketMessageType::Error)
		{
			{
				std::lock_guard lock(handshake_mutex);
				failed = true;
			}
			handshake_cv.notify_one();
			LOG_ERROR("failed to connect to the web socket ('%s')", formatted_address.c_str());
		}
	});
	web_socket.start();

	{
		std::unique_lock lock(handshake_mutex);
		handshake_cv.wait_for(lock, std::chrono::seconds(10), [&] { return connected || failed; });
	}

	if (!connected)
	{
		web_socket.stop();
		g_radar.web_socket.reset();
		error_out = "Could not connect to the web server.\n\nTry restarting AimSync WebRadar.";
		return false;
	}

	g_running = true;
	g_radar.thread = std::thread([&]() {
		while (g_running.load())
		{
			sdk::update();
			f::run();
			if (overlay::is_running())
				overlay::render(f::m_data);
			web_socket.send(f::m_data.dump());
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
		web_socket.stop();
		});

	LOG_INFO("radar loop running");
	return true;
}

static int run(HINSTANCE instance)
{
	g_mutex = CreateMutexW(nullptr, TRUE, L"Global\\AImSyncWebRadar");
	if (GetLastError() == ERROR_ALREADY_EXISTS)
	{
		MessageBoxA(nullptr, "AimSync WebRadar is already running.", "AimSync WebRadar", MB_OK | MB_ICONINFORMATION);
		return 1;
	}

	LOG_INFO("AimSync WebRadar starting...");

	appdata::ensure();
	appdata::sync_maps();
	LOG_INFO("app data folder: %s", appdata::root().string().c_str());

	const int exit_code = launcher_gui::run(instance,
		[&](std::string& error_out) { return start_radar(instance, error_out); },
		[] { g_running = false; });

	g_running = false;
	overlay::shutdown();
	if (g_radar.thread.joinable())
		g_radar.thread.join();
	g_radar.web_socket.reset();

	cleanup();
	return exit_code;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int)
{
	return run(instance);
}
