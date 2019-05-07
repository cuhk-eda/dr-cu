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

class SandboxPort : public SandboxInstance {
	
RSYN_FRIEND_OF_GENERIC_LIST_COLLECTION;
	
friend class Sandbox;
friend class SandboxInstance;

private:	
	SandboxPort(SandboxInstanceData * data) : SandboxInstance(data) {}
public:
	
	SandboxPort() : SandboxInstance(nullptr) {}
	SandboxPort(std::nullptr_t) : SandboxInstance(nullptr) {}

	Port getRelated() const;

	SandboxPin getInnerPin() const;

	// NOTE: dummy, but kept as it's need for some template functions
	SandboxPin getOuterPin() const; 
	SandboxPin getOtherPin(Rsyn::SandboxPin pin) const;

	// If this is a virtual port (i.e. does not exist in the design), returns
	// the pin that is driving/driven by this port.
	SandboxPin getAttachedPin() const;
	
	Direction getDirection() const;

	// Returns true if this port is not related to a real port in the design.
	bool isVirtual() const;
}; // end class
	
} // end namespace