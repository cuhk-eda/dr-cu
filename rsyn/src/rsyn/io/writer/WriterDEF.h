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
 * File:   WriterDEF.h
 * Author: jucemar
 *
 * Created on August 27, 2018, 8:53 PM
 */

#ifndef WRITERDEF_H
#define WRITERDEF_H

#include <Rsyn/Session>
#include <Rsyn/PhysicalDesign>
#include "rsyn/phy/util/DefDescriptors.h"
namespace Rsyn {

class WriterDEF : public Rsyn::Service {
protected:
	Rsyn::Session clsSession;
	Rsyn::Design clsDesign;
	Rsyn::Module clsModule;
	Rsyn::PhysicalDesign clsPhDesign;
	double clsDefVersion = 5.8;
	bool clsVersion : 1;
	bool clsDeviderChar : 1;
	bool clsBusBitChar : 1;
	bool clsTechnlogy : 1;
	bool clsUnits : 1;
	bool clsHistory : 1;
	bool clsExt : 1;
	bool clsPropertyDefinitions : 1;
	bool clsDieArea : 1;
	bool clsRows : 1;
	bool clsTracks : 1;
	bool clsGCellGrids : 1;
	bool clsVias : 1;
	bool clsStyles : 1;
	bool clsNonDefaultRules : 1;
	bool clsRegions : 1;
	bool clsComponentMaskShift : 1;
	bool clsComponents : 1;
	bool clsUplacedComponents : 1;
	bool clsPins : 1;
	bool clsPinProperties : 1;
	bool clsBlockages : 1;
	bool clsSlots : 1;
	bool clsFills : 1;
	bool clsSpecialNets : 1;
	bool clsNets : 1;
	bool clsRoutedNets : 1;
	bool clsScanChains : 1;
	bool clsGroups : 1;
	std::string clsPath;
	std::string clsFilename;
public:

	WriterDEF() {
		reset();
	} // end constructor 
	WriterDEF(const WriterDEF& orig) = default;
	virtual ~WriterDEF() = default;
	virtual void start(const Rsyn::Json &params) override;
	virtual void stop() override;
	
	
	void writeDEF();
	
	// contest mode
	void writeICCAD15();
	void writeISPD18();

	
	void reset() {
		cleanPath();
		cleanFilename();
		setDefVersion(5.8);
		enableAll();
	} // end method 

	void enableAll();
	void disableAll();
	void enableFloorplan();
	void enablePlacement();
	
	void setPath(const std::string & path) {
		clsPath = path;
	} // end method 

	void setFilename(const std::string & filename) {
		clsFilename = filename;
	} // end method 

	void setDefVersion(const double version) {
		clsDefVersion = version;
	} // end method
	
	// Enable write to def file
	void setVersion(const bool enable) {
		clsVersion = enable;
	} // end method 

	void setDeviderChar(const bool enable) {
		clsDeviderChar = enable;
	} // end method 

	void setBusBitChar(const bool enable) {
		clsBusBitChar = enable;
	} // end method 

	void setTechnlogy(const bool enable) {
		clsTechnlogy = enable;
	} // end method 

	void setUnits(const bool enable) {
		clsUnits = enable;
	} // end method 

	void setHistory(const bool enable) {
		clsHistory = enable;
	} // end method 

	void setExt(const bool enable) {
		clsExt = enable;
	} // end method 

	void setPropertyDefinitions(const bool enable) {
		clsPropertyDefinitions = enable;
	} // end method 

	void setDieArea(const bool enable) {
		clsDieArea = enable;
	} // end method 

	void setRows(const bool enable) {
		clsRows = enable;
	} // end method 

	void setTracks(const bool enable) {
		clsTracks = enable;
	} // end method 

	void setGCellGrids(const bool enable) {
		clsGCellGrids = enable;
	} // end method 

	void setVias(const bool enable) {
		clsVias = enable;
	} // end method 

	void setStyles(const bool enable) {
		clsStyles = enable;
	} // end method 

	void setNonDefaultRules(const bool enable) {
		clsNonDefaultRules = enable;
	} // end method 

	void setRegions(const bool enable) {
		clsRegions = enable;
	} // end method 

	void setComponentMaskShift(const bool enable) {
		clsComponentMaskShift = enable;
	} // end method 

	void setComponents(const bool enable) {
		clsComponents = enable;
	} // end method 

	void setUplacedComponents(const bool enable) {
		clsUplacedComponents = enable;
	} // end method 
	
	void setPins(const bool enable) {
		clsPins = enable;
	} // end method 

	void setPinProperties(const bool enable) {
		clsPinProperties = enable;
	} // end method 

	void setBlockages(const bool enable) {
		clsBlockages = enable;
	} // end method 

	void setSlots(const bool enable) {
		clsSlots = enable;
	} // end method 

	void setFills(const bool enable) {
		clsFills = enable;
	} // end method 

	void setSpecialNets(const bool enable) {
		clsSpecialNets = enable;
	} // end method 

	void setNets(const bool enable) {
		clsNets = enable;
	} // end method 

	void setRoutedNets(const bool enable) {
		clsRoutedNets = enable;
	} // end method 
	
	void setScanChains(const bool enable) {
		clsScanChains = enable;
	} // end method 

	void setGroups(const bool enable) {
		clsGroups = enable;
	} // end method 

	void cleanPath() {
		clsPath.clear();
	} // end method 

	void cleanFilename() {
		clsFilename.clear();
	} // end method 

	const std::string & getPath() const {
		return clsPath;
	} // end method 

	const std::string & getFilename() const {
		return clsFilename;
	} // end method 
	
	double getDefVersion() {
		return clsDefVersion;
	} // end method
	
	bool isPathSet() const {
		return !clsPath.empty();
	} // end method 

	bool isFilenameSet() const {
		return !clsFilename.empty();
	} // end method 

	bool isVersion() {
		return clsVersion;
	} // end method 

	bool isDeviderChar() {
		return clsDeviderChar;
	} // end method 

	bool isBusBitChar() {
		return clsBusBitChar;
	} // end method 

	bool isTechnlogy() {
		return clsTechnlogy;
	} // end method 

	bool isUnits() {
		return clsUnits;
	} // end method 

	bool isHistory() {
		return clsHistory;
	} // end method 

	bool isExt() {
		return clsExt;
	} // end method 

	bool isPropertyDefinitionsEnabled() const {
		return clsPropertyDefinitions;
	} // end method 

	bool isDieAreaEnabled() const {
		return clsDieArea;
	} // end method 

	bool isRowsEnabled() const {
		return clsRows;
	} // end method 

	bool isTracksEnabled() const {
		return clsTracks;
	} // end method 

	bool isGCellGridsEnabled() const {
		return clsGCellGrids;
	} // end method 

	bool isViasEnabled() const {
		return clsVias;
	} // end method 

	bool isStylesEnabled() const {
		return clsStyles;
	} // end method 

	bool isNonDefaultRulesEnabled() const {
		return clsNonDefaultRules;
	} // end method 

	bool isRegionsEnabled() const {
		return clsRegions;
	} // end method 

	bool isComponentMaskShiftEnabled() const {
		return clsComponentMaskShift;
	} // end method 

	bool isComponentsEnabled() const {
		return clsComponents;
	} // end method 

	bool isUplacedComponentsEnabled() const {
		return clsUplacedComponents;
	} // end method 
	
	bool isPinsEnabled() const {
		return clsPins;
	} // end method 

	bool isPinPropertiesEnabled() const {
		return clsPinProperties;
	} // end method 

	bool isBlockagesEnabled() const {
		return clsBlockages;
	} // end method 

	bool isSlotsEnabled() const {
		return clsSlots;
	} // end method 

	bool isFillsEnabled() const {
		return clsFills;
	} // end method 

	bool isSpecialNetsEnabled() const {
		return clsSpecialNets;
	} // end method 

	bool isNetsEnabled() const {
		return clsNets;
	} // end method 
	
	bool isRoutedNetsEnabled() const {
		return clsRoutedNets;
	} // end method 

	bool isScanChainsEnabled() const {
		return clsScanChains;
	} // end method 

	bool isGroupsEnabled() const {
		return clsGroups;
	} // end method 

protected:
	
	// enable contest mode
	void enableICCAD15();
	void enableISPD18();

	
	// load Def Descriptor
	void loadDEFHeaderDscp(DefDscp & def);
	void loadDEFPropertyDefinitions(DefDscp & def);
	void loadDEFDieArea(DefDscp & def);
	void loadDEFRows(DefDscp & def);
	void loadDEFTracks(DefDscp & def);
	void loadDEFGCellGrid(DefDscp & def);
	void loadDEFVias(DefDscp & def);
	void loadDEFStyles(DefDscp & def);
	void loadDEFNonDefaultRules(DefDscp & def);
	void loadDEFRegions(DefDscp & def);
	void loadDEFComponentMaskShift(DefDscp & def);
	void loadDEFComponents(DefDscp & def);
	void loadDEFPins(DefDscp & def);
	void loadDEFPinProperties(DefDscp & def);
	void loadDEFBlockages(DefDscp & def);
	void loadDEFSlots(DefDscp & def);
	void loadDEFFills(DefDscp & def);
	void loadDEFSpecialNets(DefDscp & def);
	void loadDEFNets(DefDscp & def);
	void loadDEFScanChains(DefDscp & def);
	void loadDEFGroups(DefDscp & def);
}; // end class 

} // end namespace 

#endif /* WRITERDEF_H */

