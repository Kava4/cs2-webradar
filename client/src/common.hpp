#pragma once

/* game modules */
#define CLIENT_DLL "client.dll"
#define ENGINE2_DLL "engine2.dll"
#define SCHEMASYSTEM_DLL "schemasystem.dll"

/* game signatures */
#define GET_SCHEMA_SYSTEM "48 89 05 ? ? ? ? 4c 8d 0d ? ? ? ? 33 c0"
#define GET_ENTITY_LIST "48 8b 1d ? ? ? ? 48 89 1d ? ? ? ? 4c 63 b3"
#define GET_GLOBAL_VARS "48 89 15 ? ? ? ? 48 89 42"
#define GET_LOCAL_PLAYER_CONTROLLER "48 8b 05 ? ? ? ? 41 89 be"

/* custom defines */
#define LOG_INFO(str, ...) printf(" [info] " str "\n", __VA_ARGS__)
#define LOG_WARNING(str, ...) printf(" [warning] " str "\n", __VA_ARGS__)
#define LOG_ERROR(str, ...) \
    do { \
        const auto filename = std::filesystem::path(__FILE__).filename().string(); \
        printf(" [error] [%s:%d] " str "\n", filename.c_str(), __LINE__, __VA_ARGS__); \
    } while (0)

#define INIT_STEP(name, expr) \
    if (!(expr)) \
    { \
        std::this_thread::sleep_for(std::chrono::seconds(5)); \
        return {}; \
    } \
    LOG_INFO(name " initialization completed")
