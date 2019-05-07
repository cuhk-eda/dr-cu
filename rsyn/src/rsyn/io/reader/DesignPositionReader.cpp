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
 
#include "DesignPositionReader.h"

#include <boost/filesystem.hpp>

#include "rsyn/io/parser/lef_def/DEFControlParser.h"

#include <Rsyn/PhysicalDesign>
#include <Rsyn/PhysicalDesign>

namespace Rsyn {

bool DesignPositionReader::run(const Rsyn::Json & config) {
	return load(config);
} // end method 

// -----------------------------------------------------------------------------

bool DesignPositionReader::load(const Rsyn::Json & config) {
	this->session = session;
	std::string path = config.value("path", "");
	clsDesign = session.getDesign();
	clsModule = session.getTopModule();
	clsPhysicalDesign = session.getPhysicalDesign();
	std::string ext = boost::filesystem::extension(path);
	
	// checking the extension file
	if (ext.compare(".def") == 0) {
		openDef(path);
	} else {
		throw Exception("Invalid extension file in the path " + path);
	} // end if-else

	return true;
} // end method 

// -----------------------------------------------------------------------------

void DesignPositionReader::openDef(std::string & path) {
	DEFControlParser defParser;
	DefDscp defDscp;
	defParser.parseDEF(path, defDscp);
	Rsyn::PhysicalDesign clsPhysicalDesign = session.getPhysicalDesign();

	for (const DefComponentDscp &component : defDscp.clsComps) {
		Rsyn::Cell cell = clsDesign.findCellByName(component.clsName);

		if (!cell) {
			throw Exception("Library cell '" + component.clsName + "' not found.\n");
		} // end if

		if (cell.isFixed())
			continue;
		PhysicalCell physicalCell = clsPhysicalDesign.getPhysicalCell(cell);
		
		clsPhysicalDesign.placeCell(physicalCell, component.clsPos);
	} // end for 
} // end method 

// -----------------------------------------------------------------------------

} // end namespace 

