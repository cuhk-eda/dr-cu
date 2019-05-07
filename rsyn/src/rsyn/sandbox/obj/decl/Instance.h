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

class SandboxInstance : public Proxy<SandboxInstanceData> {

RSYN_FRIEND_OF_GENERIC_LIST_COLLECTION;

friend class Sandbox;
friend class SandboxPin;
friend class SandboxCell;
friend class SandboxPort;

private:	
	SandboxInstance(SandboxInstanceData * data) : Proxy(data) {}
	
public:
	
	SandboxInstance() {}
	SandboxInstance(std::nullptr_t) {}
		
	SandboxCell asCell() const;
	SandboxPort asPort() const;
	
	Sandbox getSandbox();
	const Sandbox getSandbox() const;

	Design getDesign();
	const Design getDesign() const;

	Instance getRelated() const;
	
	InstanceType getType() const;
	
	const std::string &getName() const;

	int getNumPins() const;
	int getNumPins(const Direction direction) const;
	int getNumInputPins() const;
	int getNumOutputPins() const;
	int getNumBidirectionalPins() const;

	int getNumArcs() const;
	
	SandboxPin getPinByIndex(const int index) const;
	SandboxPin getPinByName(const std::string &name) const;
	SandboxPin getAnyInputPin();
	SandboxPin getAnyOutputPin();
	
	SandboxArc getArc(const SandboxPin from, const SandboxPin to);
	SandboxArc getArcByPinNames(const std::string &from, const std::string &to);
	SandboxArc getAnyArc();
		
	TopologicalIndex getTopologicalIndex() const;
	
	bool isPort() const;
	bool isPort(const Direction direction);
	
	Range<CollectionOfSandboxPinsFilteredByDirection>
	allPins(const Direction direction) const;

	Range<CollectionOfSandboxPins> // TODO: CollectionOfConstantPins
	allPins() const;

	Range<CollectionOfSandboxArcs>
	allArcs() const;

	bool isSequential() const;
	bool isTie(const TieType type) const;
	bool isBuffer(const BufferType type) const;
}; // end class
	
} // end namespace