#include <array>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string_view>

#include "json.hpp"

#include "another_toml.hpp"

using namespace std::string_view_literals;
namespace toml = another_toml;

void stream_to_json(std::ostream&, const toml::node&);

int main()
{
	try
	{
		auto toml_node = toml::parse(std::cin);
		stream_to_json(std::cout, toml_node);
		return EXIT_SUCCESS;
	}
	catch (...)
	{
		return EXIT_FAILURE;
	}
}

constexpr auto value_strings = std::array{
	"string"sv, "integer"sv, "float"sv, "boolean"sv,
	"datetime"sv, "datetime-local"sv, "date-local"sv,
	"time-local"sv, "unknown"sv, "bad"sv, "out-of-range"sv
};

constexpr std::string_view value_to_string(const toml::value_type v)
{
	assert(static_cast<std::size_t>(v) < size(value_strings));
	return value_strings[static_cast<std::size_t>(v)];
}

json::JSON stream_array(const toml::node&);
void stream_table(json::JSON&, const toml::node&);

json::JSON stream_value(const toml::node& n)
{
	if (n.array())
		return stream_array(n);
	else if (n.inline_table())
	{
		auto tab = json::Object();
		stream_table(tab, n);
		return tab;
	}

	auto val = json::Object();
	val["type"] = std::string{ value_to_string(n.type()) };
	val["value"] = n.as_string();
	return val;
}

json::JSON stream_array(const toml::node& n)
{
	auto arr = json::Array();

	for (const auto& node : n)
	{
		assert(node.good());
		auto elm = stream_value(node);
		arr.append(elm);
	}

	return arr;
}

void stream_table(json::JSON& json, const toml::node& n)
{
	for (const auto& node : n)
	{
		assert(node.good());
		if(node.table())
		{
			auto tab = json::Object();
			stream_table(tab, node);
			json[node.as_string()] = tab;
		}
		else if(node.key())
		{
			auto val = stream_value(node.get_child());
			json[node.as_string()] = val;
		}
		else
		{
			assert(node.array_table());
			json::JSON& arr = json[node.as_string()];
			for (const auto& arr_tab : node)
			{
				auto tab = json::Object();
				stream_table(tab, arr_tab);
				arr.append(tab);
			}
		}
	}
}

void stream_to_json(std::ostream& strm, const toml::node& n)
{
	auto json = json::Object();
	stream_table(json, n);
	strm << json;
	return;
}
