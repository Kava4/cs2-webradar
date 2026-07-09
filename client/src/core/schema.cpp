#include "pch.hpp"

struct schema_data_t
{
	fnv1a_t m_hashed_field_name = 0;
	uint32_t m_offset = 0;
};
static std::vector<schema_data_t> m_schema_data = {};

static std::ptrdiff_t resolve_type_scope_hash_offset(const c_schema_system_type_scope* type_scope)
{
	constexpr std::ptrdiff_t candidates[] = { 0x540, 0x588, 0x5B8, 0x5C8, 0x638 };
	constexpr int32_t min_classes = 200;
	constexpr int32_t max_classes = 3000;

	for (const auto offset : candidates)
	{
		const auto blocks = m_memory->read_t<int32_t>(
			reinterpret_cast<uintptr_t>(type_scope) + offset + 0xC);

		if (blocks >= min_classes && blocks <= max_classes)
			return offset;
	}

	return 0x540;
}

bool schema::setup()
{
	const auto type_scope = i::m_schema_system->find_type_scope_for_module(CLIENT_DLL);
	if (!type_scope)
	{
		LOG_ERROR("failed to find client.dll type scope");
		return false;
	}

	schema::m_type_scope_hash_offset = resolve_type_scope_hash_offset(type_scope);

	const auto table_size = type_scope->m_hash_classes().size();
	if (!table_size || table_size > 4096)
	{
		LOG_ERROR("invalid schema class table size: %u (hash offset 0x%llX)",
			table_size, static_cast<unsigned long long>(schema::m_type_scope_hash_offset));
		return false;
	}

	std::unique_ptr<uintptr_t[]> elements = std::make_unique_for_overwrite<uintptr_t[]>(table_size);

	const auto elements_size = type_scope->m_hash_classes().get_elements(0, table_size, elements.get());
	LOG_INFO("found '%d' schema classes in module '%s' (hash offset 0x%llX)",
		elements_size, CLIENT_DLL, static_cast<unsigned long long>(schema::m_type_scope_hash_offset));

	for (uint32_t idx = 0; idx < elements_size; idx++)
	{
		const auto element = elements[idx];
		if (!element)
			continue;

		const auto class_binding = type_scope->m_hash_classes()[element];
		if (!class_binding)
			continue;

		auto [schema_field_size, schema_field] = class_binding->get_fields();
		if (schema_field_size > 512)
			continue;

		for (uint32_t f_idx = 0; f_idx < schema_field_size; f_idx++)
		{
			if (!schema_field)
				continue;

			auto buff = format("{}->{}", class_binding->m_binary_name(), schema_field->m_name());
			m_schema_data.emplace_back(fnv1a::hash(buff), schema_field->m_single_inheritance_offset());

			schema_field = reinterpret_cast<c_schema_class_field_data*>(
				reinterpret_cast<uintptr_t>(schema_field) + sizeof(c_schema_class_field_data));
		}
	}

	if (!m_schema_data.size())
	{
		LOG_ERROR("schema table is empty after parsing client.dll");
		return false;
	}

	return true;
}

uint32_t schema::get_offset(const fnv1a_t hashed_field_name)
{
	if (const auto it = std::ranges::find_if(m_schema_data, [hashed_field_name](const schema_data_t& data)
	{
		return data.m_hashed_field_name == hashed_field_name;
	});

	it != m_schema_data.end())
		return it->m_offset;

	LOG_ERROR("failed to find an offset for the field with the hash value '%d'", hashed_field_name);
	return {};
}
