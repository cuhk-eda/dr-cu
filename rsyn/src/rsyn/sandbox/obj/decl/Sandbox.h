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

class Sandbox : public Proxy<SandboxData> {
friend class SandboxInstance;
friend class SandboxNet;

template<typename _Object, typename _ObjectReference, typename _ObjectExtension> friend class SandboxAttributeBase;
template<typename _Object, typename _ObjectExtension> friend class SandboxAttributeImplementation;

private:
	Sandbox(SandboxData * data) : Proxy(data) {}

	static const std::string NULL_NAME;

	std::string generateUniqueInstanceName(const std::string &prefix);
	std::string generateUniqueNetName(const std::string &prefix);

	int generateNextSign();
	int getSign() const;	

	void updateTopologicalIndex(SandboxPin pin);

	////////////////////////////////////////////////////////////////////////////
	// Unique Identifiers for Rsyn Objects
	//--------------------------------------------------------------------------
	// Currently Rsyn uses unique identifiers (indexes) internally to manage
	// layers.
	//
	// Indexes should not be exposed to users as they are merely an internal
	// way to support layers and this scheme may be changed in the future.
	//
	// Layers allow users to associate data to the several Rsyn objects.
	// Moreover layers are aware to incremental changes in the netlist.
	////////////////////////////////////////////////////////////////////////////
private:

	Index getId(SandboxNet net) const;
	Index getId(SandboxInstance instance) const;
	Index getId(SandboxPin pin) const;
	Index getId(SandboxArc arc) const;

public:

	Sandbox() {}
	Sandbox(std::nullptr_t) {}

	void create(Rsyn::Module module, const std::string &name = "");
	void create(Rsyn::Cell seed);

	Design getDesign();
	const Design getDesign() const;
	Module getModule();
	const Module getModule() const;

	SandboxInstance createInstance(const Instance instance);
	SandboxNet createNet(const Net net);

	SandboxCell createCell(const std::string &libraryCellName, const std::string &cellName = "");
	SandboxCell createCell(const LibraryCell lcell, const std::string &name = "");
	SandboxPort createPort(const Direction direction, const std::string &name = "");
	SandboxNet createNet(const std::string &name = "");

	SandboxAttributeInitializer
	createAttribute();

	template<typename DefaultValueType>
	SandboxAttributeInitializerWithDefaultValue<DefaultValueType>
	createAttribute(const DefaultValueType &defaultValue);

	void connectPin(SandboxPin pin, SandboxNet net);
	void disconnectPin(SandboxPin pin);

	void remap(SandboxCell cell, LibraryCell newLibraryCell);
	void remap(SandboxCell cell, const std::string &newLibraryCellName);

	const std::string &getName() const;

	SandboxPort getPortByIndex(const int index);

	int getNumInstances() const;
	int getNumNets() const;
	int getNumPins() const;
	int getNumPorts(const Direction direction) const;

	SandboxInstance findInstanceByName(const std::string &name) const;
	SandboxCell findCellByName(const std::string &name) const;
	SandboxPort findPortByName(const std::string &name) const;

	SandboxNet findNetByName(const std::string &name) const;
	SandboxPin findPinByName(const std::string &cellName, const std::string &pinName) const;
	SandboxPin findPinByName(const std::string &name, const std::string::value_type separator = ':') const;

	Instance getRelated(const SandboxInstance instance) const;
	Net getRelated(const SandboxNet net) const;

	SandboxInstance getRelated(const Instance instance) const;
	SandboxNet getRelated(const Net net) const;

	Range<SandboxListCollection<SandboxInstanceData, SandboxInstance>>
	allInstances();

	Range<SandboxReferenceListCollection<SandboxPort>>
	allPorts();

	std::set<SandboxPort> &
	allPorts(const Rsyn::Direction direction);

	Range<SandboxListCollection<SandboxNetData, SandboxNet>>
	allNets();

	Range<SandboxListCollection<SandboxPinData, SandboxPin>>
	allPins();

	Range<SandboxListCollection<SandboxArcData, SandboxArc>>
	allArcs();

	std::vector<TupleElement<1, TopologicalIndex, SandboxNet>>
	allNetsInTopologicalOrder();

	std::vector<TupleElement<1, TopologicalIndex, SandboxNet>>
	allNetsInReverseTopologicalOrder();

	std::vector<TupleElement<1, TopologicalIndex, SandboxInstance>>
	allInstancesInTopologicalOrder();

	// Gets a vector with the nets in the fanout cone of a pin. The nets are
	// sorted in breadth-first order (which is not the same as topological
	// order). Breadth-first order is typically faster than topological order.
	// If not null, the first net is always the net to which the seed pins is
	// connected to.

	std::vector<Rsyn::SandboxNet>
	getFanoutConeNetsInBreadthFirstOrder(Rsyn::SandboxPin seed);

	// Gets a vector with the nets in the fan-in cone of a pin. The nets are
	// sorted in breadth-first order (which is not the same as topological
	// order). Breadth-first order is typically faster than topological order.
	// If not null, the first net is always the net to which the seed pins is
	// connected to.

	std::vector<Rsyn::SandboxNet>
	getFaninConeNetsInBreadthFirstOrder(Rsyn::SandboxPin seed);

}; // end class

} // end namespace