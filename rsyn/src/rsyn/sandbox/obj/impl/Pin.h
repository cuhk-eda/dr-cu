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

inline
const std::string
SandboxPin::getName() const {
	switch (getInstanceType()) {
		case Rsyn::CELL:
			return getLibraryPin().getName();
		case Rsyn::PORT:
			return getInstanceName();
		case Rsyn::MODULE:
			return getPort().getName();
	} // end switch
	return "<unknown>";
} // end method

// -----------------------------------------------------------------------------

inline
const std::string
SandboxPin::getFullName(const std::string::value_type separator) const {
	if (isPort()) {
		return data? getName() : NullName;
	} else {
		return data? (getInstance().getName() + separator + getName()) : NullName;
	} // end else
} // end if

// -----------------------------------------------------------------------------

inline
Sandbox
SandboxPin::getSandbox() {
	return getInstance().getSandbox();
} // end method

// -----------------------------------------------------------------------------

inline
const Sandbox
SandboxPin::getSandbox() const {
	return getInstance().getSandbox();
} // end method

// -----------------------------------------------------------------------------

inline
Design
SandboxPin::getDesign() {
	return getSandbox().getDesign();
} // end method

// -----------------------------------------------------------------------------

inline
const Design
SandboxPin::getDesign() const {
	// TODO: too many indirection (more inside cell), maybe we need to cache
	// this.
	return getSandbox().getDesign();
} // end method

// -----------------------------------------------------------------------------

inline
Pin
SandboxPin::getRelated() const {
	Instance relatedInstance = getInstance().getRelated();
	return relatedInstance? relatedInstance.getPinByIndex(data->id) : nullptr;
} // end method

// -----------------------------------------------------------------------------

inline
SandboxInstance
SandboxPin::getInstance() const {
	return data->instance;
} // end method

// -----------------------------------------------------------------------------

inline
SandboxNet
SandboxPin::getNet() const {
	return data->net;
} // end method

// -----------------------------------------------------------------------------

inline
Direction
SandboxPin::getDirection() const {
	return data->direction;
} // end method

// -----------------------------------------------------------------------------

inline
int
SandboxPin::getIndex() const {
	return data->index;
} // end method

// -----------------------------------------------------------------------------

inline
const std::string
SandboxPin::getInstanceName() const {
	return getInstance().getName();
} // end method

// -----------------------------------------------------------------------------

inline
InstanceType
SandboxPin::getInstanceType() const {
	return data->type;
} // end method

// -----------------------------------------------------------------------------

inline
const std::string &
SandboxPin::getNetName() const {
	return getNet().getName();
} // end method

// -----------------------------------------------------------------------------

inline
const std::string &
SandboxPin::getDirectionName() const {
	return Global::getDirectionName(getDirection());
} // end method

// -----------------------------------------------------------------------------

inline
LibraryPin
SandboxPin::getLibraryPin() const {
	Rsyn::LibraryCell lcell = getLibraryCell();
	return lcell? lcell.getLibraryPinByIndex(data->index) : nullptr;
} // end method

// -----------------------------------------------------------------------------

inline
LibraryCell
SandboxPin::getLibraryCell() const {
	return getInstanceType() == Rsyn::CELL?
		data->instance->lcell : nullptr;
} // end method

// -----------------------------------------------------------------------------

inline
SandboxPort
SandboxPin::getPort() const {
	switch (getInstanceType()) {
		case Rsyn::MODULE:
			return nullptr;
		case Rsyn::PORT:
			return getInstance().asPort();
		default:
			return nullptr;
	} // end if
} // end method

// -----------------------------------------------------------------------------

inline
bool
SandboxPin::isPort() const {
	return data->boundary;
} // end method

// -----------------------------------------------------------------------------

inline
bool
SandboxPin::isPort(const Direction direction) const {
	return isPort() && (getPort().getDirection() == direction);
} // end method

// -----------------------------------------------------------------------------

inline
bool
SandboxPin::isConnected() const {
	return getNet();
} // end method

// -----------------------------------------------------------------------------

inline
bool
SandboxPin::isDisconnected() const {
	return !getNet();
} // end method

// -----------------------------------------------------------------------------

inline
bool
SandboxPin::isInput() const {
	return getDirection() == Rsyn::IN;
} // end method

// -----------------------------------------------------------------------------

inline
bool
SandboxPin::isOutput() const {
	return getDirection() == Rsyn::OUT;
} // end method

// -----------------------------------------------------------------------------

inline
bool
SandboxPin::isBidirectional() const {
	return getDirection() == Rsyn::BIDIRECTIONAL;
} // end method

// -----------------------------------------------------------------------------

inline
bool
SandboxPin::isDriver() const {
	return isOutput();
} // end method

// -----------------------------------------------------------------------------

inline
bool
SandboxPin::isSink() const {
	return isInput();
} // end method

// -----------------------------------------------------------------------------

inline
bool
SandboxPin::isVirtual() const {
	if (!isPort())
		return false;
	return getPort().isVirtual();
} // end method

// -----------------------------------------------------------------------------

inline
SandboxArc
SandboxPin::getArcTo(SandboxPin to) {
	for (SandboxArc arc : allOutgoingArcs()) {
		if (arc.getToPin() == to) {
			return arc;
		} // end if
	} // end for
	return nullptr;
} // end method

// -----------------------------------------------------------------------------

inline
SandboxArc
SandboxPin::getArcFrom(SandboxPin from) {
	for (SandboxArc arc : allIncomingArcs()) {
		if (arc.getFromPin() == from) {
			return arc;
		} // end if
	} // end for
	return nullptr;
} // end method

// -----------------------------------------------------------------------------
inline
int
SandboxPin::getNumIncomingArcs() const {
	return (int) data->arcs[BACKWARD].size();
} // end method

// -----------------------------------------------------------------------------

inline
int
SandboxPin::getNumOutgomingArcs() const {
	return (int) data->arcs[FORWARD].size();
} // end method

// -----------------------------------------------------------------------------

inline
Range<CollectionOfPredecessorSandboxPins>
SandboxPin::allPredecessorPins(const bool crossBoundaries) const {
	return CollectionOfPredecessorSandboxPins(*this, crossBoundaries);
} // end method

// -----------------------------------------------------------------------------

inline
Range<CollectionOfSuccessorSandboxPins>
SandboxPin::allSucessorPins(const bool crossBoundaries) const {
	return CollectionOfSuccessorSandboxPins(*this, crossBoundaries);
} // end method

// -----------------------------------------------------------------------------

inline
const std::vector<SandboxArc> &
SandboxPin::allIncomingArcs() const {
	return data->arcs[BACKWARD];
} // end method

// -----------------------------------------------------------------------------

inline
const std::vector<SandboxArc> &
SandboxPin::allOutgoingArcs() const {
	return data->arcs[FORWARD];
} // end method

// -----------------------------------------------------------------------------

inline
void
SandboxPin::connect(SandboxNet net) {
	getSandbox().connectPin(*this, net);
} // end method

// -----------------------------------------------------------------------------

inline
void
SandboxPin::disconnect() {
	getSandbox().disconnectPin(*this);
} // end method

// -----------------------------------------------------------------------------

inline
TopologicalIndex
SandboxPin::getTopologicalIndex() const {
	return data->order;
} // end method

// -----------------------------------------------------------------------------

inline
PinUse
SandboxPin::getUse() const {
	Rsyn::LibraryPin lpin = getLibraryPin();
	
	if (!lpin) {
		return Rsyn::UNKNOWN_USE;
	} // end if
	
	return lpin.getUse();
} // end method

// -----------------------------------------------------------------------------

inline
bool
SandboxPin::isPowerOrGround() const {
	Rsyn::LibraryPin lpin = getLibraryPin();
	
	if (!lpin) {
		return false;
	} // end if
	
	return (lpin.getUse() == POWER || lpin.getUse() == GROUND);
} // end method

} // end namespace