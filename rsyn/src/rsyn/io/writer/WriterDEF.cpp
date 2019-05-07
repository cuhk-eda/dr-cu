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

/* 
 * File:   WriterDEF.cpp
 * Author: jucemar
 * 
 * Created on August 27, 2018, 8:53 PM
 */

#include "WriterDEF.h"
#include "rsyn/io/parser/lef_def/DEFControlParser.h"

namespace Rsyn {

void WriterDEF::start(const Rsyn::Json &params) {
	clsDesign = clsSession.getDesign();
	clsModule = clsDesign.getTopModule();
	clsPhDesign = clsSession.getPhysicalDesign();

	if (!params.empty()) {
		std::string path = getPath();
		path = params.value("path", path);
		setPath(path);

		std::string filename = getFilename();
		filename = params.value("filename", filename);
		setFilename(filename);
	} // end if 
} // end method 

// -----------------------------------------------------------------------------

void WriterDEF::stop() {

} // end method 

// -----------------------------------------------------------------------------

void WriterDEF::writeDEF() {
	DEFControlParser defParser;
	DefDscp def;
	loadDEFHeaderDscp(def);
	if (isPropertyDefinitionsEnabled()) {
		loadDEFPropertyDefinitions(def);
	} // end if 
	if (isDieAreaEnabled()) {
		loadDEFDieArea(def);
	} // end if 
	if (isRowsEnabled()) {
		loadDEFRows(def);
	} // end if 
	if (isTracksEnabled()) {
		loadDEFTracks(def);
	} // end if 
	if (isGCellGridsEnabled()) {
		loadDEFGCellGrid(def);
	} // end if 
	if (isViasEnabled()) {
		loadDEFVias(def);
	} // end if 
	if (isStylesEnabled()) {
		loadDEFStyles(def);
	} // end if 
	if (isNonDefaultRulesEnabled()) {
		loadDEFNonDefaultRules(def);
	} // end if 
	if (isRegionsEnabled()) {
		loadDEFRegions(def);
	} // end if 
	if (isComponentMaskShiftEnabled()) {
		loadDEFComponentMaskShift(def);
	} // end if 
	if (isComponentsEnabled()) {
		loadDEFComponents(def);
	} // end if 
	if (isPinsEnabled()) {
		loadDEFPins(def);
	} // end if 
	if (isPinPropertiesEnabled()) {
		loadDEFPinProperties(def);
	} // end if 
	if (isBlockagesEnabled()) {
		loadDEFBlockages(def);
	} // end if 
	if (isSlotsEnabled()) {
		loadDEFSlots(def);
	} // end if 
	if (isFillsEnabled()) {
		loadDEFFills(def);
	} // end if 
	if (isSpecialNetsEnabled()) {
		loadDEFSpecialNets(def);
	} // end if 
	if (isNetsEnabled()) {
		loadDEFNets(def);
	} // end if 
	if (isScanChainsEnabled()) {
		loadDEFScanChains(def);
	} // end if 
	if (isGroupsEnabled()) {
		loadDEFGroups(def);
	} // end if 

	std::string path = clsPath.empty() ? "./" : clsPath;
	path += isFilenameSet() ? getFilename() : clsDesign.getName();
	path += ".def";
	defParser.writeFullDEF(path, def);
} // end method 

// -----------------------------------------------------------------------------

// contest mode

void WriterDEF::writeICCAD15() {

	enableICCAD15();
	clsFilename = clsDesign.getName() + "-cad085";
	writeDEF();
} // end method 

// -----------------------------------------------------------------------------

void WriterDEF::writeISPD18() {

} // end method 

// -----------------------------------------------------------------------------

void WriterDEF::enableAll() {

	setVersion(true);
	setDeviderChar(true);
	setBusBitChar(true);
	setTechnlogy(true);
	setUnits(true);
	setHistory(true);
	setExt(true);
	setPropertyDefinitions(true);
	setDieArea(true);
	setRows(true);
	setTracks(true);
	setGCellGrids(true);
	setVias(true);
	setStyles(true);
	setNonDefaultRules(true);
	setRegions(true);
	setComponentMaskShift(true);
	setComponents(true);
	setPins(true);
	setPinProperties(true);
	setBlockages(true);
	setSlots(true);
	setFills(true);
	setSpecialNets(true);
	setNets(true);
	setRoutedNets(true);
	setScanChains(true);
	setGroups(true);
} // end method 

// -----------------------------------------------------------------------------

void WriterDEF::disableAll() {

	setVersion(false);
	setDeviderChar(false);
	setBusBitChar(false);
	setTechnlogy(false);
	setUnits(false);
	setHistory(false);
	setExt(false);
	setPropertyDefinitions(false);
	setDieArea(false);
	setRows(false);
	setTracks(false);
	setGCellGrids(false);
	setVias(false);
	setStyles(false);
	setNonDefaultRules(false);
	setRegions(false);
	setComponentMaskShift(false);
	setComponents(false);
	setUplacedComponents(false);
	setPins(false);
	setPinProperties(false);
	setBlockages(false);
	setSlots(false);
	setFills(false);
	setSpecialNets(false);
	setNets(false);
	setRoutedNets(false);
	setScanChains(false);
	setGroups(false);
} // end method 

// -----------------------------------------------------------------------------

void WriterDEF::enableFloorplan() {
} // end method 

// -----------------------------------------------------------------------------

void WriterDEF::enablePlacement() {

} // end method 

// -----------------------------------------------------------------------------

void WriterDEF::enableICCAD15() {

	clsFilename.clear();
	disableAll();
	setVersion(true);
	setDeviderChar(true);
	setBusBitChar(true);
	setTechnlogy(true);
	setUnits(true);
	setDieArea(true);
	setRows(true);
	setComponents(true);
	setPins(true);
} // end method 

// -----------------------------------------------------------------------------

void WriterDEF::enableISPD18() {

} // end method 

// -----------------------------------------------------------------------------

void WriterDEF::loadDEFHeaderDscp(DefDscp &def) {
	if (isVersion()) {
		def.clsHasVersion = true;
		def.clsVersion = getDefVersion();
	} // end if 

	if (isDeviderChar()) {
		// TODO 
	} // end if 

	if (isBusBitChar()) {
		// TODO 
	} // end if 

	def.clsDesignName = clsDesign.getName();

	if (isTechnlogy()) {
		// TODO 
	} // end if 

	if (isUnits()) {
		def.clsHasDatabaseUnits = true;
		def.clsDatabaseUnits = clsPhDesign.getDatabaseUnits(DESIGN_DBU);
	} // end if 

	if (isHistory()) {
		// TODO 
	} // end if 
	if (isExt()) {
		// TODO 
	} // end if 
} // end method 

// -----------------------------------------------------------------------------

void WriterDEF::loadDEFPropertyDefinitions(DefDscp & def) {
	// TODO 
} // end method 

// -----------------------------------------------------------------------------

void WriterDEF::loadDEFDieArea(DefDscp & def) {
	def.clsHasDieBounds = true;
	def.clsDieBounds = clsPhDesign.getPhysicalDie().getBounds();
} // end method 

// -----------------------------------------------------------------------------

void WriterDEF::loadDEFRows(DefDscp & def) {
	int numRows = clsPhDesign.getNumRows();
	def.clsRows.reserve(numRows);
	for (Rsyn::PhysicalRow phRow : clsPhDesign.allPhysicalRows()) {
		def.clsRows.push_back(DefRowDscp());
		DefRowDscp & defRow = def.clsRows.back();
		defRow.clsName = phRow.getName();
		defRow.clsSite = phRow.getSiteName();
		defRow.clsOrigin = phRow.getOrigin();
		defRow.clsStepX = phRow.getStep(X);
		defRow.clsStepY = 0; //phRow.getStep(Y); // Assuming all Y steps are 0
		defRow.clsNumX = phRow.getNumSites(X);
		defRow.clsNumY = phRow.getNumSites(Y);
		defRow.clsOrientation = Rsyn::getPhysicalOrientation(phRow.getSiteOrientation());
	} // end for 
} // end method 

// -----------------------------------------------------------------------------

void WriterDEF::loadDEFTracks(DefDscp & def) {
	int numTracks = clsPhDesign.getNumPhysicalTracks();
	def.clsTracks.reserve(numTracks);
	for (Rsyn::PhysicalTracks phTrack : clsPhDesign.allPhysicalTracks()) {
		def.clsTracks.push_back(DefTrackDscp());
		DefTrackDscp & defTrack = def.clsTracks.back();
		defTrack.clsDirection = Rsyn::getPhysicalTrackDirectionDEF(phTrack.getDirection());
		defTrack.clsLocation = phTrack.getLocation();
		defTrack.clsSpace = phTrack.getSpace();
		int numLayers = phTrack.getNumberOfLayers();
		defTrack.clsLayers.reserve(numLayers);
		for (Rsyn::PhysicalLayer phLayer : phTrack.allLayers())
			defTrack.clsLayers.push_back(phLayer.getName());
		defTrack.clsNumTracks = phTrack.getNumberOfTracks();
	} // end for 
} // end method 

// -----------------------------------------------------------------------------

void WriterDEF::loadDEFGCellGrid(DefDscp & def) {
	// TODO 
} // end method 

// -----------------------------------------------------------------------------

void WriterDEF::loadDEFVias(DefDscp & def) {
	// TODO 
	//	std::vector<DefViaDscp> & defVias = def.clsVias;
	//	defVias.reserve(clsPhDesign.getNumPhysicalVias());
	//	for (Rsyn::PhysicalVia phVia : clsPhDesign.allPhysicalVias()) {
	//		if (!phVia.isDesignVia()) {
	//			continue;
	//		} // end if 
	//		defVias.push_back(DefViaDscp());
	//		DefViaDscp & dscpVia = defVias.back();
	//		dscpVia.clsName = phVia.getName();
	//		
	//	} // end for 
} // end method 

// -----------------------------------------------------------------------------

void WriterDEF::loadDEFStyles(DefDscp & def) {
	// TODO 
} // end method 

// -----------------------------------------------------------------------------

void WriterDEF::loadDEFNonDefaultRules(DefDscp & def) {
	// TODO 
} // end method 

// -----------------------------------------------------------------------------

void WriterDEF::loadDEFRegions(DefDscp & def) {
	// TODO 
} // end method 

// -----------------------------------------------------------------------------

void WriterDEF::loadDEFComponentMaskShift(DefDscp & def) {
	// TODO 
} // end method 

// -----------------------------------------------------------------------------

void WriterDEF::loadDEFComponents(DefDscp & def) {
	int numCells = clsDesign.getNumInstances(Rsyn::CELL);
	def.clsComps.reserve(numCells);
	for (Rsyn::Instance instance : clsModule.allInstances()) {
		if (instance.getType() != Rsyn::CELL) {
			continue;
		} // end if 

		Rsyn::Cell cell = instance.asCell(); // TODO: hack, assuming that the instance is a cell
		PhysicalCell ph = clsPhDesign.getPhysicalCell(cell);
		def.clsComps.push_back(DefComponentDscp());
		DefComponentDscp &defComp = def.clsComps.back();
		defComp.clsName = cell.getName();
		defComp.clsMacroName = cell.getLibraryCellName();
		if (isUplacedComponentsEnabled()) {
			defComp.clsIsPlaced = false;
		} else {
			defComp.clsPos = ph.getPosition();
			defComp.clsIsFixed = instance.isFixed();
			defComp.clsOrientation = Rsyn::getPhysicalOrientation(ph.getOrientation());
			defComp.clsIsPlaced = ph.isPlaced();
		} // end if-else 
	} // end for 
} // end method 

// -----------------------------------------------------------------------------

void WriterDEF::loadDEFPins(DefDscp & def) {
	int numPorts = clsModule.getNumPorts(Rsyn::IN) + clsModule.getNumPorts(Rsyn::OUT);
	def.clsPorts.reserve(numPorts);
	for (Rsyn::Port port : clsModule.allPorts()) {
		Rsyn::PhysicalPort phPort = clsPhDesign.getPhysicalPort(port);
		def.clsPorts.push_back(DefPortDscp());
		DefPortDscp & defPort = def.clsPorts.back();
		defPort.clsName = port.getName();
		defPort.clsNetName = port.getName();
		if (port.getDirection() == Rsyn::IN)
			defPort.clsDirection = "INPUT";
		else if (port.getDirection() == Rsyn::OUT)
			defPort.clsDirection = "OUTPUT";

		defPort.clsLocationType = "FIXED";
		defPort.clsOrientation = Rsyn::getPhysicalOrientation(phPort.getOrientation());
		defPort.clsLayerName = phPort.getLayer().getName();
		defPort.clsLayerBounds = phPort.getBounds();
		defPort.clsPos = phPort.getPosition();

	} // end for 
} // end method 

// -----------------------------------------------------------------------------

void WriterDEF::loadDEFPinProperties(DefDscp & def) {
	// TODO 
} // end method 

// -----------------------------------------------------------------------------

void WriterDEF::loadDEFBlockages(DefDscp & def) {
	// TODO 
} // end method 

// -----------------------------------------------------------------------------

void WriterDEF::loadDEFSlots(DefDscp & def) {
	// TODO 
} // end method 

// -----------------------------------------------------------------------------

void WriterDEF::loadDEFFills(DefDscp & def) {
	// TODO 
} // end method 

// -----------------------------------------------------------------------------

void WriterDEF::loadDEFSpecialNets(DefDscp & def) {
	int numNets = clsDesign.getNumNets();
	def.clsNets.reserve(numNets);
	for (Rsyn::Net net : clsModule.allNets()) {

		def.clsNets.push_back(DefNetDscp());
		DefNetDscp & defNet = def.clsNets.back();
		defNet.clsName = net.getName();
		defNet.clsConnections.reserve(net.getNumPins());
		for (Rsyn::Pin pin : net.allPins()) {
			if (!pin.isPort())
				continue;
			defNet.clsConnections.push_back(DefNetConnection());
			DefNetConnection & netConnection = defNet.clsConnections.back();
			netConnection.clsComponentName = "PIN";
			netConnection.clsPinName = pin.getInstanceName();
		} // end for 
		for (Rsyn::Pin pin : net.allPins()) {
			if (pin.isPort())
				continue;
			defNet.clsConnections.push_back(DefNetConnection());
			DefNetConnection & netConnection = defNet.clsConnections.back();
			netConnection.clsComponentName = pin.getInstanceName();
			netConnection.clsPinName = pin.getName();
		} // end for


		if (!isRoutedNetsEnabled()) {
			continue;
		} // end if 


		Rsyn::PhysicalNet phNet = clsPhDesign.getPhysicalNet(net);
		const PhysicalRouting & phRouting = phNet.getRouting();
		if (!phRouting.isValid())
			continue;


		std::vector<DefWireDscp> & wires = defNet.clsWires;
		wires.push_back(DefWireDscp());
		DefWireDscp & wire = wires.back();

		for (const PhysicalRoutingWire & phWire : phRouting.allWires()) {
			std::vector<DefWireSegmentDscp> & segments = wire.clsWireSegments;
			segments.push_back(DefWireSegmentDscp());
			DefWireSegmentDscp & segment = segments.back();
			segment.clsLayerName = phWire.getLayer().getName();
			std::vector<DefRoutingPointDscp> & points = segment.clsRoutingPoints;
			points.reserve(phWire.getNumPoints());
			for (const DBUxy point : phWire.allPoints()) {
				points.push_back(DefRoutingPointDscp());
				DefRoutingPointDscp & routing = points.back();
				routing.clsPos = point;
			} // end for 
			if (phWire.hasNonDefaultSourceExtension()) {
				DefRoutingPointDscp & routing = points.front();
				DBUxy ext = phWire.getExtendedSourcePosition();
				ext -= routing.clsPos;
				routing.clsExtension = ext[X] ? ext[X] : ext[Y];
				routing.clsHasExtension = true;
			} // end if 

			if (phWire.hasNonDefaultTargetExtension()) {
				DefRoutingPointDscp & routing = points.back();
				DBUxy ext = phWire.getExtendedTargetPosition();
				ext -= routing.clsPos;
				routing.clsExtension = ext[X] ? ext[X] : ext[Y];
				routing.clsHasExtension = true;
			} // end if 
		} // end for 

		for (const PhysicalRoutingVia & phVia : phRouting.allVias()) {
			if (!phVia.isValid())
				continue;

			std::vector<DefWireSegmentDscp> & segments = wire.clsWireSegments;
			segments.push_back(DefWireSegmentDscp());
			DefWireSegmentDscp & segment = segments.back();
			segment.clsLayerName = phVia.getTopLayer().getName();
			std::vector<DefRoutingPointDscp> & points = segment.clsRoutingPoints;
			points.push_back(DefRoutingPointDscp());
			DefRoutingPointDscp & routing = points.back();
			routing.clsPos = phVia.getPosition();
			routing.clsHasVia = true;
			routing.clsViaName = phVia.getVia().getName();
		} // end for 

		for (const PhysicalRoutingRect & rect : phRouting.allRects()) {
			std::vector<DefWireSegmentDscp> & segments = wire.clsWireSegments;
			segments.push_back(DefWireSegmentDscp());
			DefWireSegmentDscp & segment = segments.back();
			segment.clsLayerName = rect.getLayer().getName();
			segment.clsNew = true;
			segment.clsRoutingPoints.push_back(DefRoutingPointDscp());
			DefRoutingPointDscp & point = segment.clsRoutingPoints.back();
			point.clsHasRectangle = true;
			const Bounds &bds = rect.getRect();
			point.clsPos = bds[LOWER];
			point.clsRect[UPPER] = bds[UPPER] - point.clsPos;
		} // end for 

	} // end for
} // end method 

// -----------------------------------------------------------------------------

void WriterDEF::loadDEFNets(DefDscp & def) {
	// TODO 
} // end method 

// -----------------------------------------------------------------------------

void WriterDEF::loadDEFScanChains(DefDscp & def) {
	// TODO 
} // end method 

// -----------------------------------------------------------------------------

void WriterDEF::loadDEFGroups(DefDscp & def) {
	// TODO 
} // end method 

// -----------------------------------------------------------------------------

} // end namespace 

