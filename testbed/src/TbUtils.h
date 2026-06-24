#pragma once

#include <archimedes/archimedes.h>

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

// Small shared helpers for the testbed examples (loading compiled shaders). The shader
// directory is wired by testbed/CMakeLists.txt via TESTBED_SHADER_DIR.
namespace tb
{
	inline std::vector<char> readFile(const std::string& path)
	{
		std::ifstream file(path, std::ios::ate | std::ios::binary);
		std::vector<char> buffer;
		if (!file.is_open())
		{
			fprintf(stderr, "failed to open file: %s\n", path.c_str());
			return buffer;
		}
		buffer.resize(size_t(file.tellg()));
		file.seekg(0);
		file.read(buffer.data(), std::streamsize(buffer.size()));
		return buffer;
	}

	// Loads a compiled SPIR-V shader from the testbed shader dir (e.g. "mesh.vert.spv").
	inline acm::Shader loadShader(acm::Device& device, const std::string& spvName)
	{
		return device.createShader(readFile(std::string(TESTBED_SHADER_DIR) + "/" + spvName));
	}
} // namespace tb
