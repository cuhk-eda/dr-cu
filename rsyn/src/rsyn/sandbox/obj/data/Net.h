/* Copyright 2014-2018 Rsyn
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
 
namespace Rsyn {

class SandboxObjectData;
	
struct SandboxNetData : SandboxObjectData {
	// Sandbox
	Sandbox sandbox;

	// Design net to which this net relates to.
	Net related;

	// Using a vector for fast traverse, but slow insertion and removal.
	std::vector<SandboxPin> pins;
	
	// Driver. If multiple-drivers, store one of them without any assumptions.
	SandboxPin driver;

	// Cache number of pins per direction.
	std::array<int, NUM_SIGNAL_DIRECTIONS> numPinsOfType;
	
	// Ussed in some netlist traversals.
	int sign;

	SandboxNetData() :
		sign(-1),
		driver(nullptr), 
		numPinsOfType({0, 0, 0, 0}) {
	} // end constructor	
}; // end struct

} // end namespace