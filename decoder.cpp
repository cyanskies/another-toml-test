#include <array>
#include <iostream>
#include <string_view>

#include "json.hpp"

#include "another_toml/parser.hpp"
#include "another_toml/string_util.hpp"

using namespace std::string_view_literals;
namespace toml = another_toml;

void stream_to_json(std::ostream&, const toml::root_node&);

constexpr auto str = u8R"([a.b.c]
answer = 42
[a]
better = 43
)"sv;

void read_toml(const toml::root_node& r)
{
	auto name = std::string{ "name" };
	auto test = r["a"]["b.c"];
	auto test2 = r[name]["first"];
	auto test3 = r["arr"]["t"]["a"]["b"];
}

int main(int argc, char** args)
{
	try
	{
	#if 1
		std::ios_base::sync_with_stdio(false);
		auto toml_node = toml::parse(std::cin);
	#elif 0
		// nothrow
		auto toml_node = toml::parse(std::cin, toml::no_throw);
		if (!toml_node.good())
			return EXIT_FAILURE;
	#elif 1
		// use the string defined above as input
		auto toml_node = toml::parse(str, toml::no_throw);
		//read_toml(toml_node);
	#elif 1
		const auto file = std::filesystem::path{ "large.toml" };
		auto toml_node = toml::parse(file);
	#else
		// use the string defined above as input
		auto toml_node = toml::parse(str);
		//read_toml(toml_node);
	#endif
		stream_to_json(std::cout, toml_node);
		return EXIT_SUCCESS;
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what();
		return EXIT_FAILURE;
	}
}

constexpr auto value_strings = std::array{
	"string"sv, "integer"sv, "float"sv, "bool"sv,
	"datetime"sv, "datetime-local"sv, "date-local"sv,
	"time-local"sv, "unknown"sv, "bad"sv, "out-of-range"sv
};

constexpr std::string_view value_to_string(const toml::value_type v)
{
	assert(static_cast<std::size_t>(v) < size(value_strings));
	return value_strings[static_cast<std::size_t>(v)];
}

json::JSON stream_array(const toml::node&);

template<bool R>
void stream_table(json::JSON&, const toml::basic_node<R>&);

using toml::to_escaped_string;

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
	val["type"] = to_escaped_string(std::string{ value_to_string(n.type()) });
	if (n.type() == toml::value_type::string)
		val["value"] = to_escaped_string(n.as_string());
	else if (n.type() == toml::value_type::integer)
		val["value"] = n.as_string(toml::int_base::dec);
	else if (n.type() == toml::value_type::floating_point)
		val["value"] = n.as_string(toml::float_rep::default, 19);
	else
		val["value"] = n.as_string();

	return val;
}

json::JSON stream_array(const toml::node& n)
{
	auto arr = json::Array();

	for (const auto& basic_node : n)
	{
		assert(basic_node.good());
		auto elm = stream_value(basic_node);
		arr.append(elm);
	}

	return arr;
}

template<bool Root>
void stream_table(json::JSON& json, const toml::basic_node<Root>& n)
{
	for (const auto& basic_node : n)
	{
		assert(basic_node.good());
		if(basic_node.table())
		{
			auto tab = json::Object();
			stream_table(tab, basic_node);
			json[to_escaped_string(basic_node.as_string())] = tab;
		}
		else if(basic_node.key())
		{
			auto val = stream_value(basic_node.get_first_child());
			json[to_escaped_string(basic_node.as_string())] = val;
		}
		else
		{
			assert(basic_node.array_table());
			json::JSON& arr = json[to_escaped_string(basic_node.as_string())];
			for (const auto& arr_tab : basic_node)
			{
				auto tab = json::Object();
				stream_table(tab, arr_tab);
				arr.append(tab);
			}
		}
	}
}

void stream_to_json(std::ostream& strm, const toml::root_node& n)
{
	auto json = json::Object();
	stream_table(json, n);
	strm << json;
	return;
}
