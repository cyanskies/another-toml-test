#include <array>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string_view>

#include "json.hpp"

#include "another_toml.hpp"

using namespace std::string_view_literals;
namespace toml = another_toml;

constexpr auto in_str = u8R"(
       arr4 = "\u001F"
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
		auto beg = reinterpret_cast<const char*>(&*in_str.begin());
		auto end = beg + in_str.length();
		str = std::string{ beg, end };
#endif
		const auto j = json::JSON::Load(str);
		//stream_to_json(std::cout, toml_node);
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

