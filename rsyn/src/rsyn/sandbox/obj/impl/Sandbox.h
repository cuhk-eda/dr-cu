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
void
Sandbox::create(Rsyn::Module module, const std::string &name) {
	// Allocate memory for the new design.
	//data = std::make_shared<DesignData>();
	data = new SandboxData;
	data->module = module;
	data->name = name;
} // end method

// -----------------------------------------------------------------------------

inline
void
Sandbox::create(Rsyn::Cell seed) {
	Rsyn::Module module = seed.getParent();
	create(module, seed.getName());

	// Create instances and nets in the sandbox.
	for (Rsyn::Pin pin : seed.allPins()) {
		Rsyn::Net net = pin.getNet();
		if (!net || getRelated(net))
			continue;
		
		createNet(net);
		for (Rsyn::Pin netPin : net.allPins()) {
			Rsyn::Instance instance = netPin.getInstance();
			if (getRelated(instance))
				continue;

			createInstance(instance);
		} // end for
	} // end for

	// Connect pins in the sandbox according to the design. Pins with no related
	// net in the sandbox will be connected to a port.
	for (Rsyn::SandboxInstance instance : allInstances()) {
		for (Rsyn::SandboxPin pin : instance.allPins()) {
			Rsyn::Pin relatedPin = pin.getRelated();
			Rsyn::Net relatedNet = relatedPin.getNet();
			if (!relatedNet)
				continue;
			connectPin(pin, getRelated(relatedNet));
		} // end for
	} // end for

	// Create virtual ports to drive or be driven by dangling pins.
	for (Rsyn::SandboxPin pin : allPins()) {
		Rsyn::SandboxNet net = pin.getNet();
		if (!net) {
			// Dangling pin. Create a virtual port and attach this pin to it.
			Rsyn::SandboxPort port = createPort(pin.getDirection(), pin.getFullName());
			port->attachedPin = pin.data;

			// Create a net to connect the virtual port and the attached pin.
			Rsyn::SandboxNet net = createNet();
			pin.connect(net);
			port.getInnerPin().connect(net);
		} // end if
	} // end for
} // end method

// -----------------------------------------------------------------------------

inline
const Design
Sandbox::getDesign() const {
	return getModule()? getModule().getDesign() : nullptr;
} // end method

// -----------------------------------------------------------------------------

inline
Design
Sandbox::getDesign() {
	return getModule()? getModule().getDesign() : nullptr;
} // end method

// -----------------------------------------------------------------------------

inline
const Module
Sandbox::getModule() const {
	return data->module;
} // end method

// -----------------------------------------------------------------------------

inline
Module
Sandbox::getModule() {
	return data->module;
} // end method

// -----------------------------------------------------------------------------

inline
const std::string &
Sandbox::getName() const {
	return data->name;
} // end method

// -----------------------------------------------------------------------------

inline
std::string
Sandbox::generateUniqueInstanceName(const std::string &prefix) {
	std::ostringstream oss;
	oss << prefix << data->anonymousInstanceId;
	data->anonymousInstanceId++;
	return oss.str();
} // end method

// -----------------------------------------------------------------------------

inline
std::string
Sandbox::generateUniqueNetName(const std::string &prefix) {
	std::ostringstream oss;
	oss << prefix << data->anonymousNetId;
	data->anonymousNetId++;
	return oss.str();
} // end method

// -----------------------------------------------------------------------------

inline
int
Sandbox::generateNextSign() {
	return ++data->sign; /*must be pre-increment*/
} // end method

// -----------------------------------------------------------------------------

inline
int
Sandbox::getSign() const {
	return data->sign;
} // end method

// -----------------------------------------------------------------------------

inline
SandboxInstance
Sandbox::findInstanceByName(const std::string &name) const {
	auto it = data->instanceNameMapping.find(name);
	return it != data->instanceNameMapping.end()? it->second : nullptr;
} // end method

// -----------------------------------------------------------------------------

inline
SandboxCell
Sandbox::findCellByName(const std::string &name) const {
	Rsyn::SandboxInstance instance = findInstanceByName(name);
	if (instance) {
		return instance.getType() == Rsyn::CELL? instance.asCell() : nullptr;
	} else {
		return nullptr;
	} // end else
} // end method

// -----------------------------------------------------------------------------

inline
SandboxPort
Sandbox::findPortByName(const std::string &name) const {
	Rsyn::SandboxInstance instance = findInstanceByName(name);
	if (instance) {
		return instance.getType() == Rsyn::PORT? instance.asPort() : nullptr;
	} else {
		return nullptr;
	} // end else
} // end method

// -----------------------------------------------------------------------------

inline
SandboxNet
Sandbox::findNetByName(const std::string &name) const {
	auto it = data->netMapping.find(name);
	return it != data->netMapping.end()? it->second : nullptr;
} // end method

// -----------------------------------------------------------------------------

inline
SandboxPin
Sandbox::findPinByName(const std::string &cellName, const std::string &pinName) const {
	SandboxCell cell = findCellByName(cellName);
	if (cell) {
		return cell.getPinByName(pinName);
	} else {
		return nullptr;
	} // end else
} // end method

// -----------------------------------------------------------------------------

inline
SandboxPin
Sandbox::findPinByName(const std::string &name, const std::string::value_type separator) const {
	std::size_t split = name.find_first_of(separator);
	if (split == std::string::npos)
		return nullptr;

	const std::string cellName = name.substr(0, split);
	const std::string pinName = name.substr(split + 1, std::string::npos);
	return findPinByName(cellName, pinName);
} // end method

// -----------------------------------------------------------------------------

inline
Instance
Sandbox::getRelated(const SandboxInstance instance) const {
	return instance->related;
} // end method

// -----------------------------------------------------------------------------

inline
SandboxInstance
Sandbox::getRelated(const Instance instance) const {
	auto it = data->mappingInstance.find(instance);
	return it != data->mappingInstance.end()? it->second : nullptr;
} // end method

// -----------------------------------------------------------------------------

inline
Net
Sandbox::getRelated(const SandboxNet net) const {
	return net->related;
} // end method

// -----------------------------------------------------------------------------

inline
SandboxNet
Sandbox::getRelated(const Net net) const {
	auto it = data->mappingNet.find(net);
	return it != data->mappingNet.end()? it->second : nullptr;
} // end method

// -----------------------------------------------------------------------------

inline
SandboxInstance
Sandbox::createInstance(const Rsyn::Instance instance) {
	if (instance.getParent() != getModule()) {
		std::cout << "ERROR: Only instances from the same module can be created in a sandbox.\n";
		std::exit(1);
	} // end if

	Rsyn::SandboxInstance sandboxInstance = getRelated(instance);
	if (sandboxInstance) {
		return sandboxInstance;
	} // end if
	
	switch (instance.getType()) {
		case Rsyn::CELL: {
			Rsyn::Cell cell = instance.asCell();
			sandboxInstance = createCell(cell.getLibraryCell(), cell.getName());
			break;
		} // end case

		case Rsyn::PORT: {
			Rsyn::Port port = instance.asPort();
			sandboxInstance = createPort(port.getDirection(), port.getName());
			break;
		} // end case
			
		case Rsyn::MODULE: {
			std::cout << "ERROR: Modules cannot be created inside a sandbox.\n";
			std::exit(1);
			break;
		} // end case

		default: {
			std::cout << "ERROR: Unsupported instance type.\n";
			std::exit(1);
		} // end case
	} // end switch

	sandboxInstance->related = instance;
	data->mappingInstance[instance] = sandboxInstance;
} // end method

// -----------------------------------------------------------------------------

inline
SandboxNet
Sandbox::createNet(const Rsyn::Net net) {
	if (net.getParent() != getModule()) {
		std::cout << "ERROR: Only nets from the same module can be created in a sandbox.\n";
		std::exit(1);
	} // end if

	Rsyn::SandboxNet sandboxNet = getRelated(net);
	if (sandboxNet) {
		return sandboxNet;
	} // end if

	sandboxNet = createNet(net.getName());
	sandboxNet->related = net;
	data->mappingNet[net] = sandboxNet;
} // end method

// -----------------------------------------------------------------------------

inline
SandboxCell
Sandbox::createCell(const LibraryCell libraryCell, const std::string &name) {
	const LibraryCellData * lcell = libraryCell.data;

	const std::string &cellName = name == ""?
		generateUniqueInstanceName("__cell") : name;

	const int numPins = lcell->pins.size();
	const int numArcs = lcell->arcs.size();

	// Creates a new cell in the data structure.
	SandboxInstanceData * instance = &(data->instances.create()->value); // TODO: awful
	SandboxCell cell(instance);

	// Initializes instance.
	instance->id = data->instances.lastId();
	instance->sandbox = *this;
	instance->lcell = const_cast<LibraryCellData *>(lcell);
	instance->type = CELL;
	instance->pins.resize(numPins);
	instance->arcs.resize(numArcs);

	// Initializes instance's pins.
	for (int i = 0; i < numPins; i++) {
		SandboxPinData * pin = &(data->pins.create()->value); // TODO: awful
		LibraryPin lpin = lcell->pins[i];
		pin->id = data->pins.lastId();
		pin->instance = cell;
		pin->direction = lpin.data->direction;
		pin->type = Rsyn::CELL;
		pin->index = lpin.data->index;

		instance->pins[i] = pin;

		switch (pin->direction) {
			case Rsyn::IN:
				pin->order = 0;
				break;
			case Rsyn::OUT:
				pin->order = TOPOLOGICAL_SORTING_SMALL_GAP;
				break;
			case Rsyn::BIDIRECTIONAL:
				throw Exception("Rsyn does not support bidirectional pins yet.\n");
			default:
				throw Exception("Unknown pin direction.\n");
		} // end switch
	} // end for

	// Initializes cell's arcs.
	for (int i = 0; i < numArcs; i++) {
		SandboxArcData * arc = &(data->arcs.create()->value); // TODO: awful
		LibraryArc larc = lcell->arcs[i];
		arc->id = data->arcs.lastId();
		arc->type = INSTANCE_ARC;
		arc->libraryArcData = larc.data;
		arc->from = cell.getPinByLibraryPin(larc.data->from);
		arc->to = cell.getPinByLibraryPin(larc.data->to);

		arc->from.data->arcs[FORWARD].push_back(SandboxArc(arc));
		arc->to.data->arcs[BACKWARD].push_back(SandboxArc(arc));

		instance->arcs[i] = arc;
	} // end for

	// Stores cell name.
	if (data->instanceNames.size() <= instance->id) {
		data->instanceNames.resize(instance->id+1);
	} // end if

	data->instanceNames[instance->id] = cellName;
	data->instanceNameMapping[cellName] = SandboxInstance(instance);

	// Mark as dirty.
	data->dirty = true;

	// Return
	return cell;
} // end method

// -----------------------------------------------------------------------------

inline
SandboxPort
Sandbox::createPort(const Direction direction, const std::string &name) {
	const std::string &portName = name == ""?
		generateUniqueInstanceName("__cell") : name;

	const int numPins = 1;
	const int numArcs = 0;

	// Creates a new cell in the data structure.
	SandboxPort port = &(data->instances.create()->value); // TODO: awful

	// Initializes port data.
	port->id = data->instances.lastId();
	port->sandbox = *this;
	port->type = PORT;
	port->lcell = nullptr;
	port->pins.resize(numPins);
	port->arcs.resize(numArcs);

	// Creates the inner pin which will be added to the port.
	SandboxPinData * inner = &(data->pins.create()->value); // TODO: awful
	inner->id = data->pins.lastId();
	inner->instance = port;
	inner->direction = Global::getReverseDirection(direction);
	inner->type = Rsyn::PORT;
	inner->boundary = true;
	inner->index = 0;
	port->pins[0] = inner;

	// Topological index.
	switch (direction) {
		case Rsyn::IN:
			inner->order = TOPOLOGICAL_SORTING_SMALL_GAP;
			break;
		case Rsyn::OUT:
			inner->order = 1;
			break;
		default:
			throw Exception("Port direction not supported.");
	} // ens switch

	// Stores port (instance) name.
	if (data->instanceNames.size() <= port->id) {
		data->instanceNames.resize(port->id+1);
	} // end if

	data->instanceNames[port->id] = portName;
	data->instanceNameMapping[portName] = SandboxInstance(port);

	// Records this cell in it's parent module.
	data->ports.add(port);
	data->portsByDirection[direction].insert(port);

	// Mark as dirty.
	data->dirty = true;

	// Return
	return port;
} // end method

// -----------------------------------------------------------------------------

inline
SandboxNet
Sandbox::createNet(const std::string &name) {
	// Creates a new net in the data structure.
	SandboxNetData * net = &(data->nets.create()->value); // TODO: awful

	// Gets the net name.
	const std::string &netName = name == ""?
		generateUniqueNetName("__net") : name;

	// Initializes net.
	net->id = data->nets.lastId();
	net->sandbox = *this;

	// Stores net name.
	if (data->netNames.size() <= net->id) {
		data->netNames.resize(net->id+1);
	} // end if

	data->netNames[net->id] = netName;
	data->netMapping[netName] = net;

	// Mark as dirty.
	data->dirty = true;

	// Return.
	return net;
} // end method

// -----------------------------------------------------------------------------

inline
void
Sandbox::connectPin(SandboxPin pin, SandboxNet net) {
	if (pin->net) {
		disconnectPin(pin);
	} // end if

	net->pins.push_back(pin);
	pin->net = net;

	net->numPinsOfType[pin.getDirection()]++;
	if (pin.getDirection() == DRIVER) {
		net->driver = pin; // cache the driver
	} // end if

	// Create net arcs as necessary.
	switch (pin.getDirection()) {
		case Rsyn::DRIVER: {
			for (Rsyn::SandboxPin sink : net.allPins(Rsyn::SINK)) {
				SandboxArc arc = &(data->arcs.create()->value); // TODO: awful
				arc->id = data->arcs.lastId();
				arc->type = NET_ARC;
				arc->netData = net.data;
				arc->from = pin;
				arc->to = sink;

				pin->arcs[FORWARD].push_back(arc);
				sink->arcs[BACKWARD].push_back(arc);
			} // end for
			break;
		} // end case

		case Rsyn::SINK: {
			for (Rsyn::SandboxPin driver : net.allPins(Rsyn::DRIVER)) {
				SandboxArc arc = &(data->arcs.create()->value); // TODO: awful
				arc->id = data->arcs.lastId();
				arc->type = NET_ARC;
				arc->netData = net.data;
				arc->from = driver;
				arc->to = pin;

				driver->arcs[FORWARD].push_back(arc);
				pin->arcs[BACKWARD].push_back(arc);
			} // end for
			break;
		} // end case

		default:
			throw Exception("Unsupported direction.");
	} // end switch

	// Mark as dirty.
	data->dirty = true;

	// Update topological sorting.
	updateTopologicalIndex(pin);
} // end method

// -----------------------------------------------------------------------------

inline
void
Sandbox::disconnectPin(SandboxPin pin) {
	if (pin->net) {
		// Remove the pin from the net.
		SandboxNet net = pin->net;
		const int numPins = net->pins.size();
		for (int i = 0; i < numPins; i++) {
			if (net->pins[i] == pin) {
				// Swap the element to be delete with the last one and then shrink
				// the vector. Note that is affect the pin ordering inside the net.
				std::swap(net->pins[i], net->pins.back());
				net->pins.resize(net->pins.size() - 1);
				break;
			} // end if
		} // end for

		// Remove arcs from the pin.
		switch (pin.getDirection()) {
			case Rsyn::DRIVER:
				pin->arcs[FORWARD].clear();
				break;
			case Rsyn::SINK:
				pin->arcs[BACKWARD].clear();
				break;
			default:
				throw Exception("Unsupported direction.");
		} // end switch

		// Disconnect the pin.
		pin->net = nullptr;

		// Adjust pins count.
		net->numPinsOfType[pin.getDirection()]--;

		// Update the cached driver if necessary.
		if (pin.getDirection() == Rsyn::DRIVER) {
			for (SandboxPin pin : net.allPins(Rsyn::OUT)) {
				net->driver = pin;
				break;
			} // end for
		} // end if

		// Mark as dirty.
		data->dirty = true;
	} // end if
} // end method

// -----------------------------------------------------------------------------

// TODO: Improve performance of remaping. The first thing to do is to cache
// the remaping table.

inline
void
Sandbox::remap(SandboxCell cell, LibraryCell newLibraryCell) {
	LibraryCell oldLibraryCell = cell.getLibraryCell();

	// Check if the cells have the same number of arcs...
	// TODO: a deep comparison is necessary.
	if (oldLibraryCell.getNumArcs() != newLibraryCell.getNumArcs()) {
		throw IncompatibleLibraryCellForRemapping(
				oldLibraryCell.getName(), newLibraryCell.getName());
	} // end if

	// Check if the cells have the same number of pins...
	if (oldLibraryCell.getNumPins() != newLibraryCell.getNumPins()) {
		throw IncompatibleLibraryCellForRemapping(
				oldLibraryCell.getName(), newLibraryCell.getName());
	} // end if

	// Create a mapping between the old and new pins.
	// TODO: Cache this!
	std::map<Rsyn::LibraryPin, Rsyn::LibraryPin> mapLibraryPins;
	for (LibraryPin oldLibraryPin : oldLibraryCell.allLibraryPins()) {
		LibraryPin newLibraryPin =
				newLibraryCell.getLibraryPinByName(oldLibraryPin.getName());

		if (!newLibraryPin) {
			throw IncompatibleLibraryCellForRemapping(
					oldLibraryCell.getName(), newLibraryCell.getName());
		} // end if

		mapLibraryPins[oldLibraryPin] = newLibraryPin;
	} // end method

	// Create a mapping between the old and new arcs.
	// TODO: Cache this!
	std::map<Rsyn::LibraryArc, Rsyn::LibraryArc> mapLibraryArcs;
	for (LibraryArc oldLibraryArc : oldLibraryCell.allLibraryArcs()) {
		LibraryArc newLibraryArc =
				newLibraryCell.getLibraryArcByPinNames(oldLibraryArc.getFromName(), oldLibraryArc.getToName());

		if (!newLibraryArc) {
			throw IncompatibleLibraryCellForRemapping(
					oldLibraryCell.getName(), newLibraryCell.getName());
		} // end if

		mapLibraryArcs[oldLibraryArc] = newLibraryArc;
	} // end method

	// Update library cell.
	cell->lcell = newLibraryCell.data;

	// Update pins.
	for (SandboxPin pin : cell.allPins()) {
		Rsyn::LibraryPin newLibraryPin = mapLibraryPins[oldLibraryCell.getLibraryPinByIndex(pin->index)];
		pin->index = newLibraryPin->index;
	} // end for

	// Update arcs.
	for (SandboxArc arc : cell.allArcs()) {
		Rsyn::LibraryArc newLibraryArc = mapLibraryArcs[arc.getLibraryArc()];
		arc->libraryArcData = newLibraryArc.data;
	} // end for
} // end method

// -----------------------------------------------------------------------------

inline
void
Sandbox::remap(SandboxCell cell, const std::string &newLibraryCellName) {
	LibraryCell newLibraryCell = getDesign().findLibraryCellByName(newLibraryCellName);
	if (!newLibraryCell) {
		throw LibraryCellNotFoundException(newLibraryCellName);
	} // end if
	remap(cell, newLibraryCell);
} // end method

// -----------------------------------------------------------------------------

inline
int
Sandbox::getNumInstances() const {
	return data->instances.size();
} // end method

// -----------------------------------------------------------------------------

inline
int
Sandbox::getNumNets() const {
	return data->nets.size();
} // end method

// -----------------------------------------------------------------------------

inline
int
Sandbox::getNumPins() const {
	return data->pins.size();
} // end method


////////////////////////////////////////////////////////////////////////////////
// Unique Identifiers for Rsyn Objects
////////////////////////////////////////////////////////////////////////////////

inline Index Sandbox::getId(SandboxNet net) const { return net->id; }
inline Index Sandbox::getId(SandboxInstance instance) const { return instance->id; }
inline Index Sandbox::getId(SandboxPin pin) const { return pin->id; }
inline Index Sandbox::getId(SandboxArc arc) const { return arc->id; }

////////////////////////////////////////////////////////////////////////////////
// Attributes
////////////////////////////////////////////////////////////////////////////////

inline
SandboxAttributeInitializer
Sandbox::createAttribute() {
	return SandboxAttributeInitializer(*this);
} // end method

// -----------------------------------------------------------------------------

template<typename DefaultValueType>
inline
SandboxAttributeInitializerWithDefaultValue<DefaultValueType>
Sandbox::createAttribute(const DefaultValueType &defaultValue) {
	return SandboxAttributeInitializerWithDefaultValue<DefaultValueType>(*this, defaultValue);
} // end method

////////////////////////////////////////////////////////////////////////////////
// Topological Ordering
////////////////////////////////////////////////////////////////////////////////

inline
void
Sandbox::updateTopologicalIndex(SandboxPin pin) {
	// Some checks...
	static_assert(std::is_integral<TopologicalIndex>::value, "Integer required.");
	static_assert(std::is_signed<TopologicalIndex>::value, "Signed type required.");

	static_assert(TOPOLOGICAL_SORTING_SMALL_GAP > 0, "small < 0");
	static_assert(TOPOLOGICAL_SORTING_LARGE_GAP > 0, "large < 0");
	static_assert(TOPOLOGICAL_SORTING_SMALL_GAP <=
			TOPOLOGICAL_SORTING_LARGE_GAP, "small gap > large gap");

	// Gets the signature for this update search.
	const int sign = generateNextSign();

	// Gets the lower bound index.
	TopologicalIndex lower =
			-std::numeric_limits<TopologicalIndex>::infinity();
	bool hasLower = false;
	for (Rsyn::SandboxPin predecessor : pin.allPredecessorPins(true)) {
		lower = std::max(lower, predecessor.getTopologicalIndex());
		hasLower = true;
	} // end for

	// Gets the upper bound index.
	TopologicalIndex upper =
			+std::numeric_limits<TopologicalIndex>::infinity();
	bool hasUpper = false;
	for (Rsyn::SandboxPin successor : pin.allSucessorPins(true)) {
		upper = std::min(upper, successor.getTopologicalIndex());
		hasUpper = true;
	} // end for

	// Set pin's topological ordering.
	if (!hasLower && !hasUpper) {
		pin->order = 0;
	} else if (hasLower && !hasUpper) {
		pin->order = lower + TOPOLOGICAL_SORTING_SMALL_GAP;
	} else if (!hasLower && hasUpper) {
		pin->order = upper - TOPOLOGICAL_SORTING_SMALL_GAP;
	} else {
		// Ok, we got a lower and an upper bound...

		if (lower < upper && (upper - lower >= 2)) {
			// The easy case...
			// Since we use integral types, we must guarantee that there's room
			// for the new pin between the lower and upper bounds, that's why
			// we check if (upper - lower >= 2).
			pin->order = (lower + upper) / 2;
		} else {
			// We need to propagate the indexes...
			const TopologicalIndex smallGap = TOPOLOGICAL_SORTING_SMALL_GAP;
			const TopologicalIndex largeGap = TOPOLOGICAL_SORTING_LARGE_GAP;

			const TopologicalIndex left0 = upper;
			const TopologicalIndex left1 = lower + smallGap + 1;
			const TopologicalIndex right = left1 + largeGap;
			const TopologicalIndex w0 = (right - left0);
			const TopologicalIndex w1 = (right - left1);

			pin->order = (lower + left1) / 2;

			std::queue<std::tuple<Rsyn::SandboxPin, TopologicalIndex>> open;
			for (Rsyn::SandboxPin successor : pin.allSucessorPins(true)) {
				if (successor->order < right) {
					open.push(std::make_tuple(successor, pin->order));
				} // end if
			} // end for

			while (!open.empty()) {
				Rsyn::SandboxPin current = std::get<0>(open.front());
				const TopologicalIndex generatorOrder = std::get<1>(open.front());
				open.pop();

				if (current == pin) {
					// loop detected;
					std::cout << "WARNING: Loop detected.\n";
					continue;
				} // end if

				if (current->order > generatorOrder) {
					// no need to continue propagating...
					continue;
				} // end if

				TopologicalIndex order;
				if (current->sign != sign) {
					current->sign = sign;
					order = (TopologicalIndex) std::floor(
							(float((current->order - upper)*w1)/float(w0)) + left1);
					if (order <= generatorOrder) {
						order = generatorOrder + smallGap;
					} // end if
				} else {
					order = generatorOrder + smallGap;
				} // end else

				current->order = order;

				for (Rsyn::SandboxPin successor : current.allSucessorPins(true)) {
					if (successor->order <= order) {
						open.push(std::make_tuple(successor, order));
					} // end if
				} // end for
			} // end while
		} // end else
	} // end else
} // end method

////////////////////////////////////////////////////////////////////////////////
// Range-Based Loop
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------

inline
Range<SandboxListCollection<SandboxInstanceData, SandboxInstance>>
Sandbox::allInstances() {
	return SandboxListCollection<SandboxInstanceData, SandboxInstance>(data->instances);
} // end method

// -----------------------------------------------------------------------------

inline
Range<SandboxReferenceListCollection<SandboxPort>>
Sandbox::allPorts() {
	return SandboxReferenceListCollection<SandboxPort>(data->ports);
} // end method

// -----------------------------------------------------------------------------

inline
std::set<SandboxPort> &
Sandbox::allPorts(const Rsyn::Direction direction) {
	return data->portsByDirection[direction];
} // end method

// -----------------------------------------------------------------------------

inline
Range<SandboxListCollection<SandboxNetData, SandboxNet>>
Sandbox::allNets() {
	return SandboxListCollection<SandboxNetData, SandboxNet>(data->nets);
} // end method

// -----------------------------------------------------------------------------

inline
Range<SandboxListCollection<SandboxPinData, SandboxPin>>
Sandbox::allPins() {
	return SandboxListCollection<SandboxPinData, SandboxPin>(data->pins);
} // end method

// -----------------------------------------------------------------------------

inline
Range<SandboxListCollection<SandboxArcData, SandboxArc>>
Sandbox::allArcs() {
	return SandboxListCollection<SandboxArcData, SandboxArc>(data->arcs);
} // end method

// -----------------------------------------------------------------------------

inline
std::vector<TupleElement<1, TopologicalIndex, SandboxNet>>
Sandbox::allNetsInTopologicalOrder() {
	const int numNets = data->nets.size();

	std::vector<TupleElement<1, TopologicalIndex, SandboxNet>> sortedNets;
	sortedNets.resize(numNets);

	int i = 0;
	for (Rsyn::SandboxNet net : allNets()) {
		sortedNets[i++] = std::make_tuple(net.getTopologicalIndex(), net);
	} // end for
	sortedNets.resize(i);
	std::sort(sortedNets.begin(), sortedNets.end());

	return sortedNets;
} // end method

// -----------------------------------------------------------------------------

inline
std::vector<TupleElement<1, TopologicalIndex, SandboxNet>>
Sandbox::allNetsInReverseTopologicalOrder() {
	const int numNets = data->nets.size();

	std::vector<TupleElement<1, TopologicalIndex, SandboxNet>> sortedNets;
	sortedNets.resize(numNets);

	int i = 0;
	for (Rsyn::SandboxNet net : allNets()) {
		sortedNets[i++] = std::make_tuple(-net.getTopologicalIndex(), net);
	} // end for
	sortedNets.resize(i);
	std::sort(sortedNets.begin(), sortedNets.end());

	return sortedNets;
} // end method

// -----------------------------------------------------------------------------

inline
std::vector<TupleElement<1, TopologicalIndex, SandboxInstance>>
Sandbox::allInstancesInTopologicalOrder() {
	const int numInstances = data->instances.size();

	std::vector<TupleElement<1, TopologicalIndex, SandboxInstance>> sortedInstances;
	sortedInstances.resize(numInstances);

	int i = 0;
	for (Rsyn::SandboxInstance instance : allInstances()) {
		sortedInstances[i++] = std::make_tuple(instance.getTopologicalIndex(), instance);
	} // end for
	sortedInstances.resize(i);
	std::sort(sortedInstances.begin(), sortedInstances.end());

	return sortedInstances;
} // end method

// -----------------------------------------------------------------------------

inline
std::vector<Rsyn::SandboxNet>
Sandbox::getFanoutConeNetsInBreadthFirstOrder(Rsyn::SandboxPin seed) {
	std::vector<Rsyn::SandboxNet> result;
	std::queue<Rsyn::SandboxNet> open;

	// Push seed net.
	Rsyn::SandboxNet net = seed.getNet();
	if (net)
		open.push(net);

	// Breadth-first search.
	const int sign = generateNextSign();

	while (!open.empty()) {
		Rsyn::SandboxNet currentNet = open.front();
		open.pop();

		if (currentNet->sign == sign)
			continue;

		currentNet->sign = sign;
		result.push_back(currentNet);

		// Add neighbors.
		for (Rsyn::SandboxPin sink : currentNet.allPins(SINK)) {
			for (Rsyn::SandboxArc arc : sink.allOutgoingArcs()) {
				Rsyn::SandboxNet net = arc.getToNet();
				if (net && net->sign != sign)
					open.push(net);
			} // end for
		} // end for
	} // end while

	// Return.
	return result;
} // end method

// -----------------------------------------------------------------------------

inline
std::vector<Rsyn::SandboxNet>
Sandbox::getFaninConeNetsInBreadthFirstOrder(Rsyn::SandboxPin seed) {
	std::vector<Rsyn::SandboxNet> result;
	std::queue<Rsyn::SandboxNet> open;

	// Push seed net.
	Rsyn::SandboxNet net = seed.getNet();
	if (net)
		open.push(net);

	// Breadth-first search.
	const int sign = generateNextSign();

	while (!open.empty()) {
		Rsyn::SandboxNet currentNet = open.front();
		open.pop();

		if (currentNet->sign == sign)
			continue;

		currentNet->sign = sign;
		result.push_back(currentNet);

		// Add neighbors.
		for (Rsyn::SandboxPin driver : currentNet.allPins(DRIVER)) {
			for (Rsyn::SandboxArc arc : driver.allIncomingArcs()) {
				Rsyn::SandboxNet net = arc.getFromNet();
				if (net && net->sign != sign)
					open.push(net);
			} // end for
		} // end for
	} // end while

	// Return.
	return result;
} // end method

} // end namespace