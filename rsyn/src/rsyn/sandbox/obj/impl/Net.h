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
SandboxNet::getSandbox() {
	return data->sandbox;
} // end method

// -----------------------------------------------------------------------------

inline
const Sandbox
SandboxNet::getSandbox() const {
	return data->sandbox;
} // end method

// -----------------------------------------------------------------------------

inline
Design
SandboxNet::getDesign() {
	return getSandbox().getDesign();
} // end method

// -----------------------------------------------------------------------------

inline
const Design
SandboxNet::getDesign() const {
	return getSandbox().getDesign();
} // end method

// -----------------------------------------------------------------------------

inline
Net
SandboxNet::getRelated() const {
	return data->related;
} // end method

// -----------------------------------------------------------------------------

inline
const std::string &
SandboxNet::getName() const {
	return data? getSandbox()->netNames[data->id] : NullName;
} // end method

// -----------------------------------------------------------------------------

inline
int
SandboxNet::getNumPins() const {
	return (int) data->pins.size();
} // end method

// -----------------------------------------------------------------------------

inline
int
SandboxNet::getNumSinks() const {
	return data->numPinsOfType[SINK];
} // end method

// -----------------------------------------------------------------------------

inline
int
SandboxNet::getNumDrivers() const {
	return data->numPinsOfType[DRIVER];
} // end method

// -----------------------------------------------------------------------------

inline
SandboxPin
SandboxNet::getDriver() const {
	return data->driver;
} // end method

// -----------------------------------------------------------------------------

inline
TopologicalIndex
SandboxNet::getTopologicalIndex() const {
	TopologicalIndex order;

	if (hasMultipleDrivers()) {
		for (Rsyn::SandboxPin driver : allPins(Rsyn::DRIVER)) {
			order = driver.getTopologicalIndex();
		} // end for
	} else if (hasDriver()) {
		order = getDriver().getTopologicalIndex();
	} else {
		// No driver...
		if (hasSink()) {
			order = +std::numeric_limits<TopologicalIndex>::infinity();
			for (Rsyn::SandboxPin sink : allPins(Rsyn::SINK)) {
				order = std::min(order, sink.getTopologicalIndex());
			} // end for
		} else {
			// A floating ..
			// Recall the topological indexes are greater than zero, so, by
			// using zero here, we ensure that floating nets will be the
			// first ones which may be useful for debugging purposes.
			order = 0;
		} // end else
	} // end else

	return order;
} // end method

// -----------------------------------------------------------------------------

inline
bool
SandboxNet::hasMultipleDrivers() const {
	return getNumDrivers() > 1;
} // end method

// -----------------------------------------------------------------------------

inline
bool
SandboxNet::hasSingleDriver() const {
	return getNumDrivers() == 1;
} // end method

// -----------------------------------------------------------------------------

inline
bool
SandboxNet::hasDriver() const {
	return getNumDrivers() >= 1;
} // end method

// -----------------------------------------------------------------------------

inline
bool
SandboxNet::hasSink() const {
	return getNumSinks() >= 1;
} // end method

// -----------------------------------------------------------------------------

inline
bool
SandboxNet::isVirtual() const {
	Rsyn::SandboxPin driver = getDriver();
	return driver && driver.isVirtual();
} // end method

// -----------------------------------------------------------------------------

inline
Range<CollectionOfSandboxPins>
SandboxNet::allPins() const {
	return CollectionOfSandboxPins(data->pins);
} // end method

// -----------------------------------------------------------------------------

inline
Range<CollectionOfSandboxPinsFilteredByDirection>
SandboxNet::allPins(const Direction direction) const {
	return CollectionOfSandboxPinsFilteredByDirection(data->pins, direction);
} // end method

// -----------------------------------------------------------------------------

inline
Range<CollectionOfSandboxArcs>
SandboxNet::allArcs() const {
 	if (hasMultipleDrivers()) {
		throw Exception("SandboxNet::allArcs() does not support multiple drivers yet.");
	} else {
		if (hasDriver()) {
			return CollectionOfSandboxArcs(getDriver()->arcs[FORWARD]);
		} else {
			static std::vector<SandboxArc> emptyCollectionOfArcs; // dummy
			return CollectionOfSandboxArcs(emptyCollectionOfArcs);
		} // end else
	} // end else
} // end method

} // end namespace