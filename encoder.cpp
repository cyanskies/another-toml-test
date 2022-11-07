#include <array>
#include <charconv>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string_view>
#include <variant>

#include "json.hpp"

#include "another_toml.hpp"
#include "another_toml_string_util.hpp"

using namespace std::string_literals;
using namespace std::string_view_literals;
namespace toml = another_toml;

template<bool NoThrow>
bool convert_json(const json::JSON& j);

void make_file();

constexpr auto in_str = u8R"(
 {
         "escaped": {
           "type": "string",
           "value": "lol\"\"\""
         },
         "lit_one": {
           "type": "string",
           "value": "'one quote'"
         },
         "lit_one_space": {
           "type": "string",
           "value": " 'one quote' "
         },
         "lit_two": {
           "type": "string",
           "value": "''two quotes''"
         },
         "lit_two_space": {
           "type": "string",
           "value": " ''two quotes'' "
         },
         "mismatch1": {
           "type": "string",
           "value": "aaa'''bbb"
         },
         "mismatch2": {
           "type": "string",
           "value": "aaa\"\"\"bbb"
         },
         "one": {
           "type": "string",
           "value": "\"one quote\""
         },
         "one_space": {
           "type": "string",
           "value": " \"one quote\" "
         },
         "two": {
           "type": "string",
           "value": "\"\"two quotes\"\""
         },
         "two_space": {
           "type": "string",
           "value": " \"\"two quotes\"\" "
         }
       }
)"sv;

int main()
{
	try
	{
		auto str = std::string{};
#if 0
		auto sstream = std::stringstream{};
		sstream << std::cin.rdbuf();
		str = sstream.str();
#else
		//make_file();
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
		assert(ret.error == toml::parse_float_string_return::error_t{});
		if (ret.representation == toml::float_rep::scientific)
			w.write_value(ret.value, toml::float_rep::scientific, 20);
		else
			w.write_value(ret.value, {}, 20);
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
bool is_key(const json::JSON& t) noexcept
{
	return t.hasKey("type"s) &&
		t.hasKey("value"s) &&
		t.size() == 2;
}

bool is_table_array(const json::JSON& t)
{
	const auto children = t.ArrayRange();
	return std::all_of(begin(children), end(children), [](auto&& val) {
		return val.JSONType() == jtype::Object && !is_key(val);
		});
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
			if (is_table_array(value))
			{
				const auto tables = value.ArrayRange();
				for (auto& val : tables)
				{
					w.begin_array_table(name);
					parse_table<false>(val, w, toml::node_type::array_tables);
					w.end_array_table();
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

void make_file()
{
	auto w = toml::writer{};

	w.write_key("title");
	w.write_value("TOML Example");

	w.begin_table("owner");
	w.write("name", "Tom Preston-Werner");
	w.write("dob", toml::date_time{
		toml::local_date_time{
			toml::date{	1979, 5, 27	},
			toml::time{ 7, 32 }
		}, false, 8 });
	w.end_table();

	w.begin_table("database");
	w.write("enabled", true);
	w.begin_array("ports");
	for(auto i = 5000; i < 6500; ++i)
		w.write_value(i);
	w.write_value(8001);
	w.write_value(8002);
	w.end_array();

	w.begin_array("data");
	//nested array
	w.write({}, { "delta", "phi" });
	w.begin_array({});
	w.write_value(3.14f);
	w.end_array();
	w.end_array();

	w.begin_inline_table("temp_targets");
	w.write("cpu",79.5);
	w.write("case", 72.f);
	w.end_inline_table();

	w.end_table();

	w.begin_table("servers");

	//nested table
	w.begin_table("alpha");
	w.write("ip", "10.0.0.1");
	w.write("role", "frontend");
	w.end_table();

	w.begin_table("beta");
	w.write("ip", "10.0.0.2");
	w.write("role", "backend");
	w.end_table();

	w.end_table();

	w.begin_array_table("products");
	w.write("name", "Hammer");
	w.write("sku", 738594937);
	w.end_array_table();

	w.begin_array_table("products");
	w.end_array_table();

	w.begin_array_table("products");
	w.write("name", "Nail");
	w.write("sku", 284758393);
	w.write("color", "grey");
	w.write("floatsdfdf", { 1.2f, 1.2f, 1.2f }, toml::float_rep::scientific);
	
	w.end_array_table();

	auto opt = toml::writer_options{};
	//opt.simple_numerical_output = true;
	opt.ascii_output = true;
	opt.compact_spacing = true;
	opt.skip_empty_tables = false;
	opt.utf8_bom = true;

	w.set_options(opt);

	std::cout << w;
	auto toml_str = w.to_string();
}
