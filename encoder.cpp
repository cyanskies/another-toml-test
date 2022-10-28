#include <array>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string_view>
#include <variant>

#include "json.hpp"

#include "another_toml.hpp"
#include "string_util.hpp"

using namespace std::string_literals;
using namespace std::string_view_literals;
namespace toml = another_toml;

template<bool NoThrow>
bool convert_json(const json::JSON& j);

constexpr auto in_str = u8R"(
{
         "after": {
           "type": "float",
           "value": "3141.5927"
         },
         "before": {
           "type": "float",
           "value": "3141.5927"
         },
         "exponent": {
           "type": "float",
           "value": "3.0e14"
         }
       }
)"sv;

int main()
{
	try
	{
		auto str = std::string{};
#if 1
		auto sstream = std::stringstream{};
		sstream << std::cin.rdbuf();
		str = sstream.str();
#else
		auto beg = reinterpret_cast<const char*>(&*in_str.begin());
		auto end = beg + in_str.length();
		str = std::string{ beg, end };
#endif
		const auto j = json::JSON::Load(str);
		if (convert_json<false>(j))
			return EXIT_SUCCESS;
		else
			return EXIT_FAILURE;
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

using jtype = json::JSON::Class;

template<bool NoThrow>
bool parse_value(const json::JSON& v, toml::writer& w)
{
	const auto& value = v.at("value"s);
	const auto& type_node = v.at("type"s);
	const auto type = type_node.ToString();
	if (type == "string"s)
	{
		auto str = value.ToString();
		try
		{
			// bad, we're using this to test whether
			// str has invalid escape codes(using try/catch)
			// should write a function for this
			w.write_value(toml::to_unescaped_string(str));
		}
		catch (const toml::unicode_error&)
		{
			// invalid escape code(treat as literal string)
			w.write_value(value.ToString(), toml::writer::literal_string_tag);
		}
	}
	else if (type == "integer"s)
	{
		auto integral = int64_t{};
		const auto str = value.ToString();
		auto ret = std::from_chars(data(str), data(str) + size(str), integral);
		if (ret.ec != std::errc{})
			return false;

		w.write_value(integral);
	}
	else if (type == "float"s)
	{
		const auto str = value.ToString();
		const auto ret = toml::parse_float_string(str);
		assert(ret);
		if (ret->representation == toml::writer::float_rep::scientific)
			w.write_value(ret->value, toml::writer::float_rep::scientific);
		else
			w.write_value(ret->value);
	}
	else if (type == "bool"s)
	{
		const auto str = value.ToString();
		if (str == "0" ||
			str == "true")
			w.write_value(true);
		else
			w.write_value(false);
	}
	else if (type == "datetime"s ||
		type == "datetime-local"s ||
		type == "date-local"s ||
		type == "time-local"s)
	{
		const auto var = toml::parse_date_time(value.ToString());
		return std::visit([&w](auto&& value) {
			if constexpr (std::is_same_v<std::decay_t<decltype(value)>, std::monostate>)
				return false;
			else
			{
				w.write_value(value);
				return true;
			}
			}, var);
	}
	else
		return false;

	return true;
}

template<bool NoThrow>
bool parse_array(const json::JSON& a, toml::writer& w)
{
	const auto children = a.ArrayRange();
	for (auto& val : children)
	{
		switch (val.JSONType())
		{
		case jtype::Array:
		{
			w.begin_array({});
			parse_array<false>(val, w);
			w.end_array();
		}break;
		case jtype::Object:
		{
			//table 
			if (val.hasKey("type"s) &&
				val.hasKey("value"s) &&
				val.size() == 2)
			{
				if (!parse_value<NoThrow>(val, w))
					return false;
			}
			else
			{
				w.begin_inline_table({});
				if (!parse_table<NoThrow>(val, w, toml::node_type::inline_table))
					return false;
				w.end_inline_table();
			}
		}break;
		}
	}

	return false;
}

// if true, arrays are probably arrays of tables
// 
// {}
bool is_key(const json::JSON& t)
{
	const auto children = t.ObjectRange();
	/*if (std::distance(children.begin(), children.end()) != 2)
		return false;*/

	for (auto& [name, value] : children)
	{
		if (value.hasKey("type"s) &&
			value.hasKey("value"s) &&
			value.size() == 2)
			return true;
	}

	return false;
}

bool has_child_keys(const json::JSON& t)
{
	if (t.JSONType() == jtype::Array)
	{
		const auto children = t.ArrayRange();
		for (auto& value : children)
		{
			if (is_key(value))
				return true;
			else if (has_child_keys(value))
				return true;
		}
	}
	else if(t.JSONType() == jtype::Object)
	{
		const auto children = t.ObjectRange();
		for (auto& [name, value] : children)
		{
			if (is_key(value))
				return true;
			else if (has_child_keys(value))
				return true;
		}
	}
	return false;
}

template<bool NoThrow>
bool parse_table(const json::JSON& t, toml::writer& w, toml::node_type parent_type = toml::node_type::table)
{
	const auto children = t.ObjectRange();
	for (auto& [raw_name, value] : children)
	{
		const auto name = toml::to_unescaped_string(raw_name);
		switch (value.JSONType())
		{
		case jtype::Array:
		{
			if (has_child_keys(value))
			{
				const auto tables = value.ArrayRange();
				for (auto& val : tables)
				{
					w.begin_array_tables(name);
					parse_table<false>(val, w, toml::node_type::array_tables);
					w.end_array_tables();
				}
			}
			else
			{
				w.begin_array(name);
				parse_array<false>(value, w);
				w.end_array();
			}
		} break;
		case jtype::Object:
		{
			//table 
			if (value.hasKey("type"s) &&
				value.hasKey("value"s) &&
				value.size() == 2)
			{
				w.write_key(name);
				if (!parse_value<NoThrow>(value, w))
					return false;
			}
			else
			{
				if (parent_type == toml::node_type::inline_table)
				{
					w.begin_inline_table(name);
					if (!parse_table<NoThrow>(value, w, toml::node_type::inline_table))
						return false;
					w.end_inline_table();
				}
				else
				{
					w.begin_table(name);
					if (!parse_table<NoThrow>(value, w))
						return false;
					w.end_table();
				}
			}
			break;
		}
		default:
			return false;
		}
	}

	return true;
}

template<bool NoThrow>
bool convert_json(const json::JSON& j)
{
	assert(j.JSONType() == jtype::Object);
	auto writer = toml::writer{};
	
	if (parse_table<NoThrow>(j, writer))
	{
		auto str = writer.to_string();
		std::cout << str;
		return true;
	}
	return false;
}
