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

class SandboxPin : public Proxy<SandboxPinData> {
	
RSYN_FRIEND_OF_GENERIC_LIST_COLLECTION;

friend class Sandbox;
friend class SandboxPort;
friend class SandboxNet;

private:
	SandboxPin(SandboxPinData * data) : Proxy(data) {}
	
public:
	
	SandboxPin() {}
	SandboxPin(std::nullptr_t) {}
	
	Sandbox getSandbox();
	const Sandbox getSandbox() const;

	Design getDesign();
	const Design getDesign() const;

	Pin getRelated() const;

	Direction getDirection() const;
	
	int getIndex() const;
	const std::string getName() const;
	const std::string getFullName(const std::string::value_type separator = ':') const;
	const std::string getInstanceName() const;
	const std::string &getNetName() const;
	const std::string &getDirectionName() const;
	
	SandboxNet getNet() const;
	SandboxInstance getInstance() const;
	InstanceType getInstanceType() const;
	
	LibraryPin getLibraryPin() const;
	LibraryCell getLibraryCell() const;
	SandboxPort getPort() const;
	
	TopologicalIndex getTopologicalIndex() const;
	
	bool isPort() const;
	bool isPort(const Direction direction) const;
	
	bool isConnected() const;
	bool isDisconnected() const;
	
	bool isInput() const;
	bool isOutput() const;
	bool isBidirectional() const;	
	
	bool isDriver() const;
	bool isSink() const;	

	// Indicates whether or not this pin belongs to a virtual port (i.e.
	// connects a virtual port to its attached pin).
	bool isVirtual() const;

	SandboxArc getArcTo(SandboxPin to);
	SandboxArc getArcFrom(SandboxPin from);
	
	int getNumIncomingArcs() const;
	int getNumOutgomingArcs() const;
	
	Range<CollectionOfPredecessorSandboxPins>
	allPredecessorPins(const bool crossBoundaries = false) const;
	
	Range<CollectionOfSuccessorSandboxPins>
	allSucessorPins(const bool crossBoundaries = false) const;	
	
	const std::vector<SandboxArc> &
	allIncomingArcs() const; // allArcsToPin(Pin pin) const;
	
	const std::vector<SandboxArc> &
	allOutgoingArcs() const; // allArcsFromPin(Rsyn::Pin pin) const;
	
	void connect(SandboxNet net);
	void disconnect();	

	//! @Author Mateus
	//! @brief Returns the usage of the pins
	PinUse getUse() const;

	//! @Author Mateus 
	//! @brief Returns true if the pin is used for power or ground or false
	//!        otherwise.
	bool isPowerOrGround() const;
}; // end class
	
} // end namespace