#include <array>
#include <charconv>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string_view>
#include <variant>

#include "json.hpp"

#include "another_toml/except.hpp"
#include "another_toml/string_util.hpp"
#include "another_toml/writer.hpp"

using namespace std::string_literals;
using namespace std::string_view_literals;
namespace toml = another_toml;

template<bool NoThrow>
bool convert_json(const json::JSON& j);

void make_file();
void generate_huge_file();

constexpr auto in_str = u8R"(
  {
    "title": {"type": "string", "value": "TOML Example"},
    "clients": {
        "data": [
            [
                {"type": "string", "value": "gamma"},
                {"type": "string", "value": "delta"}
            ],
            [
                {"type": "integer", "value": "1"},
                {"type": "integer", "value": "2"}
            ]
        ],
        "hosts": [
            {"type": "string", "value": "alpha"},
            {"type": "string", "value": "omega"}
        ]
    },
    "database": {
        "connection_max": {"type": "integer", "value": "5000"},
        "enabled":        {"type": "bool", "value": "true"},
        "server":         {"type": "string", "value": "192.168.1.1"},
        "ports": [
            {"type": "integer", "value": "8001"},
            {"type": "integer", "value": "8001"},
            {"type": "integer", "value": "8002"}
        ]
    },
    "owner": {
        "dob":  {"type": "datetime", "value": "1979-05-27T07:32:00-08:00"},
        "name": {"type": "string", "value": "Lance Uppercut"}
    },
    "servers": {
        "alpha": {
            "dc": {"type": "string", "value": "eqdc10"},
            "ip": {"type": "string", "value": "10.0.0.1"}
        },
        "beta": {
            "dc": {"type": "string", "value": "eqdc10"},
            "ip": {"type": "string", "value": "10.0.0.2"}
        }
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
#elif 0
		auto beg = reinterpret_cast<const char*>(&*in_str.begin());
		auto end = beg + in_str.length();
		str = std::string{ beg, end };
#else
		make_file();
		return EXIT_SUCCESS;
#endif
		const auto j = json::JSON::Load(str);
		if (convert_json<false>(j))
			return EXIT_SUCCESS;
		else
			return EXIT_FAILURE;
	}
	catch (const std::exception& e)
	{
		std::cout << e.what();
		return EXIT_FAILURE;
	}
}

static std::string lower_string(std::string_view v)
{
	auto s = std::string{};
	s.reserve(size(v));
	std::transform(begin(v), end(v), back_inserter(s), tolower);
	return s;
}

namespace another_toml
{
	std::string to_unescaped_string2(std::string_view str);
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
		w.write_value(toml::to_unescaped_string2(str));
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
		// NOTE: We lowercase the string because toml-test: tests/valid/spec/float-2.json
		//		provides inv values as "+Inf" rather than "+inf", (possibly a bug in toml-test)
		//		our api doesn't take floats as string anyway;
		//		and valid toml files cannot store infinity in uppercase.
		const auto str = lower_string(value.ToString());
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
static bool is_key(const json::JSON& t) noexcept
{
	return t.hasKey("type"s) &&
		t.hasKey("value"s) &&
		t.size() == 2;
}

static bool is_table_array(const json::JSON& t)
{
	const auto children = t.ArrayRange();
	if (begin(children) == end(children)) // catch empty arrays, these are probably not table arrays(but empty normal arrays)
		return false;
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
		const auto name = toml::to_unescaped_string2(raw_name);
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
	auto opts = toml::writer_options{};
	opts.skip_empty_tables = false;
	writer.set_options(opts);

	if (parse_table<NoThrow>(j, writer))
	{
		std::cout << writer;
		return true;
	}
	return false;
}

void make_file()
{
	auto g = toml::writer{};
	g.begin_table("a");
	g.write("junk", 5);
	g.begin_table("a");
	g.begin_array("x");
	for (auto i = 0; i < 100; ++i)
		g.write_value(i);
	g.end_array();
	g.write("long_string", "llllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllll");
	g.begin_table("b");
	g.begin_table("long_dotted_table", toml::table_def_type::dotted);
	g.write("another_long_string", "aaaaaaaaaaaaaaaaaaa\naaaaaaa aaaaaaaaaaaaaaaaaaaaa\naaaaaaaa aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa aaaaaaaaaaaaaaaaaaaaaaaaaaaa aaaaaaaaaaaaaaaaaaaaaaaaaaaa aaaaaaaaaaaaaaaaaaaaaaa aaaaaaaaaaaaaaaaaaaaa aaaaaaaaaaaaaaaaaaa");
	
	g.end_table();
	g.end_table();
	g.end_table();
	g.end_table();

	g.begin_table("a");
	g.write("thing", 500);
	g.end_table();

	std::cout << g;

	auto w = toml::writer{};

	w.write_key("title");
	w.write_value("TOML Example");

	w.begin_table("owner");
	w.write("name", "Tom Preston-Werner\'\'", toml::writer::literal_string_tag);
	w.write("dob", toml::date_time{
		toml::local_date_time{
			toml::date{	1979, 5, 27	},
			toml::time{ 7, 32 }
		}, false, 8 });
	w.end_table();

	w.begin_table("database");
	w.write("enabled", true);
	w.begin_array("ports");
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

	w.begin_table("beta", toml::table_def_type::dotted);
	w.begin_table("zeta", toml::table_def_type::dotted);
	w.write("ip", "10.0.0.2");
	w.write("role", "df");
	w.end_table();
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

	//w.set_options(opt);

	std::cout << w;
	auto toml_str = w.to_string();

	/*name = "Orange"
		physical.color = "orange"
		physical.shape = "round"
		site."google.com" = true*/

	w.write("name", "Orange");

	w.begin_table("physical", toml::table_def_type::dotted);
		w.write("color", "orange");
		w.write("shape", "round");
	w.end_table();

	w.begin_table("site", toml::table_def_type::dotted);
		w.write("google.com", true);
	w.end_table();

	return;
}

// should be some work for the parser
// about 1kb of unicode
constexpr auto unicode_str = u8"\u0000 \u0008 \u000c \u007f  \u0080 \u00ff \ud7ff \ue000 \uffff \U00010000 \U0010ffff"sv;
constexpr auto number = 1.000430343442f;

constexpr auto  table_names = std::array{
	"a", "b", "c", "d", "e", "f", "g", "h", "i"
};

constexpr auto str_names = std::array{
	"j", "k", "l", "m", "n", "o", "p"
};

constexpr auto float_names = std::array{
	"q", "r", "s", "t", "u", "v", "w", "x", "y", "z"
};

void write_elms(toml::writer& w)
{
	for (auto s : str_names)
		w.write(s, unicode_str);

	for (auto f : float_names)
		w.write(f, number);

	return;
}

// generate a large toml file using the encoder.
void generate_huge_file()
{
	// the goal is to generate a 50mb file
	auto w = toml::writer{};

	for (auto t : table_names)
	{
		w.begin_table(t);
		write_elms(w);
		for (auto t2 : table_names)
		{
			w.begin_table(t2);
			write_elms(w);
			for (auto t3 : table_names)
			{
				w.begin_table(t3);
				write_elms(w);
				w.end_table();
			}
			w.end_table();
		}
		w.end_table();
	}

	auto f = std::ofstream{ std::filesystem::path{ "large.toml"s } };
	f << w;
}