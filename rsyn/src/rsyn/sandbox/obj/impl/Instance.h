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

// -----------------------------------------------------------------------------

inline
Sandbox
SandboxInstance::getSandbox() {
	return data->sandbox;
} // end method

// -----------------------------------------------------------------------------

inline
const Sandbox
SandboxInstance::getSandbox() const {
	return data->sandbox;
} // end method

// -----------------------------------------------------------------------------

inline
Design
SandboxInstance::getDesign() {
	return getSandbox().getDesign();
} // end method

// -----------------------------------------------------------------------------

inline
const Design
SandboxInstance::getDesign() const {
	return getSandbox().getDesign();
} // end method

// -----------------------------------------------------------------------------

inline
SandboxCell
SandboxInstance::asCell() const {
	return SandboxCell(data);
} // end method

// -----------------------------------------------------------------------------

inline
SandboxPort
SandboxInstance::asPort() const {
	return SandboxPort(data);
} // end method

// -----------------------------------------------------------------------------

inline
Instance
SandboxInstance::getRelated() const {
	return data->related;
} // end method

// -----------------------------------------------------------------------------

inline
InstanceType
SandboxInstance::getType() const {
	return data->type;
} // end method

// -----------------------------------------------------------------------------

inline
SandboxPin
SandboxInstance::getPinByName(const std::string &name) const {
	for (Rsyn::SandboxPin pin : allPins()) {
		if (pin.getName() == name)
			return pin;
	} // end method
	return nullptr;
} // end method

// -----------------------------------------------------------------------------

inline
const std::string &
SandboxInstance::getName() const {
	return data? getSandbox()->instanceNames[data->id] : NullName;
} // end method

// -----------------------------------------------------------------------------

inline
int
SandboxInstance::getNumPins() const {
	return (int) data->pins.size();
} // end method

// -----------------------------------------------------------------------------

inline
int
SandboxInstance::getNumPins(const Direction direction) const {
	// TODO: optimize this using instance types (e.g. cell can get the number
	// of pins of certain direction via its library cell).

	int counter = 0;
	for (Rsyn::SandboxPin pin : allPins()) {
		if (pin.getDirection() == direction) {
			counter++;
		} // end if
	} // end for

	return counter;
} // end method

// -----------------------------------------------------------------------------

inline
int
SandboxInstance::getNumInputPins() const {
	return getNumPins(Rsyn::IN);
} // end method

// -----------------------------------------------------------------------------

inline
int
SandboxInstance::getNumOutputPins() const {
	return getNumPins(Rsyn::OUT);
} // end method

// -----------------------------------------------------------------------------

inline
int
SandboxInstance::getNumBidirectionalPins() const {
	return getNumPins(Rsyn::BIDIRECTIONAL);
} // end method


// -----------------------------------------------------------------------------

inline
int
SandboxInstance::getNumArcs() const {
	return (int) data->arcs.size();
} // end method

// -----------------------------------------------------------------------------

inline
TopologicalIndex
SandboxInstance::getTopologicalIndex() const {
	// TODO: The topological order of an instance my be ambiguous in some cases
	// as in the case of registers. There's no arc between D and Q pins and
	// hence D may have a larger topological index than Q. In a forward traverse
	// of the netlist one may expect that Q would be used as the topological
	// index of the register while in a backward traverse one would expect
	// that the D pin was used.

	// NOTE: In the current implementation the topological order of register
	// is the topological order of the clock pin.

	TopologicalIndex order = MIN_TOPOLOGICAL_INDEX;
	for (Rsyn::SandboxArc arc : allArcs()) {
		order = std::max(order, arc.getFromPin().getTopologicalIndex());
	} // end for

	// Some cells may not have arcs, so as a fall back, get the largest
	// topological index of its pins.
	if (order == MIN_TOPOLOGICAL_INDEX) {
		for (Rsyn::SandboxPin pin : allPins()) {
			order = std::max(order, pin.getTopologicalIndex());
		} // end for
	} // end if

	return order;
} // end method

// -----------------------------------------------------------------------------

inline
bool
SandboxInstance::isPort() const {
	return data->type == Rsyn::PORT;
} // end method

// -----------------------------------------------------------------------------

inline
bool
SandboxInstance::isPort(const Direction direction) {
	return isPort() && (asPort().getDirection() == direction);
} // end method

// -----------------------------------------------------------------------------

inline
SandboxPin
SandboxInstance::getPinByIndex(const int index) const {
	return data->pins[index];
} // end method

// -----------------------------------------------------------------------------

inline
SandboxPin
SandboxInstance::getAnyInputPin() {
	for (Rsyn::SandboxPin pin : allPins()) {
		if (pin.getDirection() == Rsyn::IN) {
			return pin;
		} // end if
	} // end for
	return nullptr;
} // end method

// -----------------------------------------------------------------------------

inline
SandboxPin
SandboxInstance::getAnyOutputPin() {
	for (Rsyn::SandboxPin pin : allPins()) {
		if (pin.getDirection() == Rsyn::OUT) {
			return pin;
		} // end if
	} // end for
	return nullptr;
} // end method

// -----------------------------------------------------------------------------

inline
SandboxArc
SandboxInstance::getArc(const SandboxPin from, const SandboxPin to) {
	for (SandboxArc arc : allArcs()) {
		if ((arc.getFromPin() == from) && (arc.getToPin() == to)) {
			return arc;
		} // end if
	} // end for
	return nullptr;
} // end method

// -----------------------------------------------------------------------------

inline
SandboxArc
SandboxInstance::getArcByPinNames(const std::string &from, const std::string &to) {
	for (SandboxArc arc : allArcs()) {
		if ((arc.getFromPin().getName() == from) && (arc.getToPin().getName() == to)) {
			return arc;
		} // end if
	} // end for
	return nullptr;
} // end method

// -----------------------------------------------------------------------------

inline
SandboxArc
SandboxInstance::getAnyArc() {
	return !data->arcs.empty()? data->arcs[0] : nullptr;
} // end method

// -----------------------------------------------------------------------------

inline
Range<CollectionOfSandboxPinsFilteredByDirection>
SandboxInstance::allPins(const Direction direction) const {
	return CollectionOfSandboxPinsFilteredByDirection(data->pins, direction);
} // end method

// -----------------------------------------------------------------------------

inline
Range<CollectionOfSandboxPins>
SandboxInstance::allPins() const {
	return CollectionOfSandboxPins(data->pins);
} // end method

// -----------------------------------------------------------------------------

inline
Range<CollectionOfSandboxArcs>
SandboxInstance::allArcs() const {
	return CollectionOfSandboxArcs(data->arcs);
} // end method

// -----------------------------------------------------------------------------

inline
bool
SandboxInstance::isSequential() const {
	return getType() == Rsyn::CELL?
		asCell().getLibraryCell().isSequential() : false;
} // end method

// -----------------------------------------------------------------------------

inline
bool
SandboxInstance::isTie(const TieType type) const {
	return getType() == Rsyn::CELL?
		asCell().getLibraryCell().isTie(type) : false;
} // end method

// -----------------------------------------------------------------------------

inline
bool
SandboxInstance::isBuffer(const BufferType type) const {
	return getType() == Rsyn::CELL?
		asCell().getLibraryCell().isBuffer(type) : false;
} // end method

} // end namespace