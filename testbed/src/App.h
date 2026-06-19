#pragma once

class Example;

// Owns the testbed lifecycle and drives one Example: instance + GLFW, the example's
// windows + surfaces, a shared device, a RenderContext per window, the per-frame loop
// (fork-join across render threads for multi-window, inline for single-window), and the
// device-before-surface teardown. Returns a process exit code.
class App
{
public:
	int run(Example& example);
};
