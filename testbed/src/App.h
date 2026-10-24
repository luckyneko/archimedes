/*
 *  Created by LuckyNeko on 19/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

class Example;

// Which kind of device the testbed runs on (`--device gpu|cpu`).
enum class DeviceChoice
{
	Hardware, // the best GPU (falling back to the CPU device when there is none)
	Cpu,	  // the CPU device; fails if none presents to every window
};

// Owns the testbed lifecycle and drives one Example: instance + GLFW, the example's
// windows + surfaces, a shared device, a RenderContext per window, the per-frame loop
// (fork-join across render threads for multi-window, inline for single-window), and the
// device-before-surface teardown. Returns a process exit code.
class App
{
public:
	int run(Example& example, DeviceChoice deviceChoice = DeviceChoice::Hardware);
};
