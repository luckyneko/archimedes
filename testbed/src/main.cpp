/*
 *  Created by LuckyNeko on 07/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "App.h"
#include "Example.h"
#include "ExampleRegistry.h"

#include <cstdio>
#include <memory>
#include <string>

// Thin entry point: pick an example by name (CLI arg, default ripple-mesh) and run it.
// The App owns the windowing / device / loop; each Example is a swappable demo that
// exercises a slice of the renderer. `testbed --list` prints the available examples;
// `--device gpu|cpu` picks the hardware device (default) or the CPU device.
int main(int argc, char* argv[])
{
	std::string name = defaultExampleName();
	DeviceChoice deviceChoice = DeviceChoice::Hardware;
	for (int i = 1; i < argc; ++i)
	{
		const std::string arg = argv[i];
		if (arg == "--list" || arg == "-l")
		{
			listExamples();
			return 0;
		}
		if (arg == "--device")
		{
			const std::string value = i + 1 < argc ? argv[++i] : "";
			if (value == "gpu")
				deviceChoice = DeviceChoice::Hardware;
			else if (value == "cpu")
				deviceChoice = DeviceChoice::Cpu;
			else
			{
				fprintf(stderr, "--device takes 'gpu' or 'cpu'\n");
				return 1;
			}
			continue;
		}
		name = arg;
	}

	std::unique_ptr<Example> example = makeExample(name);
	if (!example)
	{
		fprintf(stderr, "unknown example '%s' — run with --list to see the options\n", name.c_str());
		return 1;
	}

	printf("Running example: %s\n", name.c_str());
	App app;
	return app.run(*example, deviceChoice);
}
