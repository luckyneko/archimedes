#include "App.h"
#include "Example.h"
#include "ExampleRegistry.h"

#include <cstdio>
#include <memory>
#include <string>

// Thin entry point: pick an example by name (CLI arg, default ripple-mesh) and run it.
// The App owns the windowing / device / loop; each Example is a swappable demo that
// exercises a slice of the renderer. `testbed --list` prints the available examples.
int main(int argc, char* argv[])
{
	const std::string name = argc > 1 ? argv[1] : defaultExampleName();
	if (name == "--list" || name == "-l")
	{
		listExamples();
		return 0;
	}

	std::unique_ptr<Example> example = makeExample(name);
	if (!example)
	{
		fprintf(stderr, "unknown example '%s' — run with --list to see the options\n", name.c_str());
		return 1;
	}

	printf("Running example: %s\n", name.c_str());
	App app;
	return app.run(*example);
}
