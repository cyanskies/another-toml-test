#include <array>
#include <iostream>
#include <string_view>

#include "json.hpp"

#include "another_toml.hpp"

using namespace std::string_view_literals;
namespace toml = another_toml;

void stream_to_json(std::ostream&, const toml::node&);

constexpr auto str = u8R"(
[a.b.c]
       [a."b.c"]
       [a.'d.e']
       [a.' x ']
       [ d.e.f ]
       [ g . h . i ]
       [ j . "ʞ" . 'l' ]

       [x.1.2]
)"sv;

int main()
{
	try
	{
	#if 1
		auto toml_node = toml::parse(std::cin);
	#elif 0
		auto toml_node = toml::parse(str, toml::no_throw);
		if (!toml_node.good())
			return EXIT_FAILURE;
	#else
		auto beg = reinterpret_cast<const char*>(&*str.begin());
		auto end = beg + str.length();
		auto toml_node = toml::parse({beg, end});
	#endif
		stream_to_json(std::cout, toml_node);
		return EXIT_SUCCESS;
	}
	catch (...)
	{
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
void stream_table(json::JSON&, const toml::node&);

std::string json_escape_string(std::string s)
{
	auto pos = std::size_t{};
	while (pos < size(s))
	{
		switch (s[pos])
		{
		case '\"':
			s.replace(pos++, 1, "\\\"");
			break;
		case '\b':
			s.replace(pos++, 1, "\\b");
			break;
		case '\f':
			s.replace(pos++, 1, "\\f");
			break;
		case '\n':
			s.replace(pos++, 1, "\\n");
			break;
		case '\t':
			s.replace(pos++, 1, "\\t");
			break;
		case '\\':
			s.replace(pos++, 1, "\\\\");
			break;
		}

		++pos;
	}

	return s;
}

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
			json[json_escape_string(node.as_string())] = tab;
		}
		else if(node.key())
		{
			auto val = stream_value(node.get_child());
			json[json_escape_string(node.as_string())] = val;
		}
		else
		{
			assert(node.array_table());
			json::JSON& arr = json[json_escape_string(node.as_string())];
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
