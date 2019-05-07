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
 
#include "Writer.h"

#include <fstream>
#include <iostream>
#include <string>

#include <boost/filesystem.hpp>

#include <Rsyn/Session>
#include "rsyn/io/legacy/Legacy.h"


// Services
#include <Rsyn/PhysicalDesign>

#include "rsyn/io/parser/lef_def/DEFControlParser.h"
namespace Rsyn {

void Writer::start(const Rsyn::Json &params) {
	Rsyn::Session session;

	clsDesign = session.getDesign();
	clsLibrary = session.getLibrary();
	clsModule = session.getTopModule();
	clsPhysicalDesign = session.getPhysicalDesign();
} // end method

// -----------------------------------------------------------------------------

void Writer::stop() {
} // end method

// -----------------------------------------------------------------------------

void Writer::writeDEF(const std::string & filename, const bool full) {
	
	if (filename != "") {
		writeFullDEF(filename);
	} else {
		writeFullDEF(clsDesign.getName() + ".def");
	} // end if-else 

	
	/*if (full) {
		if (filename != "") {
			writeFullDEF(filename);
		} else {
			writeFullDEF(clsDesign.getName() + "-cada085.def");
		} // end if-else 
	} else {
		if (filename != "") {
			writeICCAD15DEF(filename);
		} else {
			writeICCAD15DEF(clsDesign.getName() + "-cada085.def");
		} // end if-else 
	} // end if-else 
	 */
} // end method

// -----------------------------------------------------------------------------

void Writer::writeFullDEF(string filename) {
	DEFControlParser defParser;
	DefDscp def;
	def.clsHasDieBounds = true;
	def.clsDieBounds = clsPhysicalDesign.getPhysicalDie().getBounds();
	def.clsHasDatabaseUnits = true;
	def.clsDatabaseUnits = clsPhysicalDesign.getDatabaseUnits(Rsyn::DESIGN_DBU);
	def.clsDesignName = clsDesign.getName();

	int numCells = clsDesign.getNumInstances(Rsyn::CELL);
	def.clsComps.reserve(numCells);
	for (Rsyn::Instance instance : clsModule.allInstances()) {
		if (instance.getType() != Rsyn::CELL)
			continue;

		Rsyn::Cell cell = instance.asCell(); // TODO: hack, assuming that the instance is a cell
		PhysicalCell ph = clsPhysicalDesign.getPhysicalCell(cell);
		def.clsComps.push_back(DefComponentDscp());
		DefComponentDscp &defComp = def.clsComps.back();
		defComp.clsName = cell.getName();
		defComp.clsMacroName = cell.getLibraryCellName();
		defComp.clsPos = ph.getPosition();
		defComp.clsIsFixed = instance.isFixed();
		defComp.clsOrientation = Rsyn::getPhysicalOrientation(ph.getOrientation());
		defComp.clsIsPlaced = ph.isPlaced();
	} // end for 


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
		
		
		Rsyn::PhysicalNet phNet = clsPhysicalDesign.getPhysicalNet(net);
		const PhysicalRouting & phRouting = phNet.getRouting();
		if(!phRouting.isValid())
			continue;
		
		
		std::vector<DefWireDscp> & wires = defNet.clsWires;
		wires.push_back(DefWireDscp());
		DefWireDscp & wire = wires.back();
		
		for(const PhysicalRoutingWire & phWire : phRouting.allWires()) {
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
			if(phWire.hasNonDefaultSourceExtension()) {
				DefRoutingPointDscp & routing = points.front();
				DBUxy ext = phWire.getExtendedSourcePosition();
				ext -= routing.clsPos;
				routing.clsExtension = ext[X] ? ext[X] : ext[Y];
				routing.clsHasExtension = true;
			} // end if 
			
			if(phWire.hasNonDefaultTargetExtension()) {
				DefRoutingPointDscp & routing = points.back();
				DBUxy ext = phWire.getExtendedTargetPosition();
				ext -= routing.clsPos;
				routing.clsExtension = ext[X] ? ext[X] : ext[Y];
				routing.clsHasExtension = true;
			} // end if 
		} // end for 
		
		for(const PhysicalRoutingVia & phVia : phRouting.allVias()) {
			if(!phVia.isValid())
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
		
		for(const PhysicalRoutingRect & rect : phRouting.allRects()) {
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


	int numPorts = clsModule.getNumPorts(Rsyn::IN) + clsModule.getNumPorts(Rsyn::OUT);
	def.clsPorts.reserve(numPorts);
	for (Rsyn::Port port : clsModule.allPorts()) {
		Rsyn::PhysicalPort phPort = clsPhysicalDesign.getPhysicalPort(port);
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

	int numRows = clsPhysicalDesign.getNumRows();
	def.clsRows.reserve(numRows);
	for (Rsyn::PhysicalRow phRow : clsPhysicalDesign.allPhysicalRows()) {
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

	// write def tracks 
	int numTracks = clsPhysicalDesign.getNumPhysicalTracks();
	def.clsTracks.reserve(numTracks);
	for (Rsyn::PhysicalTracks phTrack : clsPhysicalDesign.allPhysicalTracks()) {
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

	defParser.writeFullDEF(filename, def);
} // end method

} // end namespace

