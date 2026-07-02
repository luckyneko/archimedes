/*
 *  Created by LuckyNeko on 19/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

#include <memory>
#include <string>

class Example;

// The set of testbed examples, selectable by name on the command line. Add new examples
// to the table in ExampleRegistry.cpp.
std::unique_ptr<Example> makeExample(const std::string& name); // nullptr if unknown
const char* defaultExampleName();
void listExamples(); // prints name + description for each (for `testbed --list`)
