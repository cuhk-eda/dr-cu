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

class SandboxNet : public Proxy<SandboxNetData> {
	
RSYN_FRIEND_OF_GENERIC_LIST_COLLECTION;

friend class Sandbox;
friend class SandboxPin;
friend class SandboxArc;

private:
	SandboxNet(SandboxNetData * data) : Proxy(data) {}
	
public:
	
	SandboxNet() {}
	SandboxNet(std::nullptr_t) {}
	
	Sandbox getSandbox();
	const Sandbox getSandbox() const;

	Design getDesign();
	const Design getDesign() const;

	Net getRelated() const;

	const std::string &getName() const;
		
	int getNumPins() const;
	int getNumSinks() const;
	int getNumDrivers() const;

	SandboxPin getDriver() const;
	
	TopologicalIndex getTopologicalIndex() const;
	
	bool hasMultipleDrivers() const;
	bool hasSingleDriver() const;
	bool hasDriver() const;
	bool hasSink() const;

	// Indicates whether or not this net is a virtual one (i.e. connects a
	// virtual port to its attached pin).
	bool isVirtual() const;

	Range<CollectionOfSandboxPins>
	allPins() const;

	Range<CollectionOfSandboxPinsFilteredByDirection>
	allPins(const Direction direction) const;

	Range<CollectionOfSandboxArcs>
	allArcs() const;
	
}; // end class
	
} // end namespace