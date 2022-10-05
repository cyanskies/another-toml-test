#include <fstream>
#include <iostream>

#include "another_toml.hpp"

namespace toml = another_toml;

int main()
{
	auto example_file = std::ifstream{ "example.toml" };
	auto toml_node = toml::parse(example_file);

	return EXIT_SUCCESS;
}