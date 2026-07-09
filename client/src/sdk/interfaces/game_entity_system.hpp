#pragma once

class c_game_entity_system
{
public:
	template <typename T = c_base_entity*>
	T get(int32_t idx)
	{
		return reinterpret_cast<T>(this->get_entity_by_idx(idx));
	}

	template <typename T = c_base_entity*>
	T get(const c_base_handle handle)
	{
		if (!handle.is_valid())
			return nullptr;

		return reinterpret_cast<T>(this->get_entity_by_idx(handle.get_entry_idx()));
	}

private:
	void* get_entity_by_idx(const int32_t idx)
	{
		if (static_cast<uint32_t>(idx) >= 0x7ffe)
			return nullptr;

		if (static_cast<uint32_t>(idx >> 9) >= 0x3f)
			return nullptr;

		const auto entry_list = m_memory->read_t<uintptr_t>(this + 8i64 * (idx >> 9) + 16);
		if (!entry_list)
			return nullptr;

		const auto slot = entry_list + 112i64 * (idx & 0x1ff);
		const auto entity = m_memory->read_t<uintptr_t>(slot);
		if (!entity)
			return nullptr;

		return reinterpret_cast<void*>(entity);
	}
};