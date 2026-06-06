#include "pch.hpp"

void utils::send_reload()
{
	std::thread([]() {
		const auto curl = curl_easy_init();
		if (!curl) return;

		curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:22006/reload");
		curl_easy_setopt(curl, CURLOPT_POST, 1L);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 0L);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2L);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
			+[](void*, size_t s, size_t n, void*) -> size_t { return s * n; });
		curl_easy_perform(curl);
		curl_easy_cleanup(curl);
		}).detach();
}
