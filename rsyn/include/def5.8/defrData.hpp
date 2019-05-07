// *****************************************************************************
// *****************************************************************************
// Copyright 2013 - 2014, Cadence Design Systems
// 
// This  file  is  part  of  the  Cadence  LEF/DEF  Open   Source
// Distribution,  Product Version 5.8. 
// 
// Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
// 
//        http://www.apache.org/licenses/LICENSE-2.0
// 
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
//    implied. See the License for the specific language governing
//    permissions and limitations under the License.
// 
// For updates, support, or to become part of the LEF/DEF Community,
// check www.openeda.org for details.
// 
//  $Author: arakhman $
//  $Revision: #6 $
//  $Date: 2013/08/09 $
//  $State:  $
// *****************************************************************************
// *****************************************************************************

#include <cstring>
#include <string>
#include <map>
#include <vector>

#include "defrReader.hpp"

#ifndef defrData_h
#define defrData_h

#define DEFSIZE 4096
#define IN_BUF_SIZE 16384
#define RING_SIZE 10
#define RING_STRING_SIZE 64


BEGIN_LEFDEF_PARSER_NAMESPACE

struct defCompareStrings 
{
    bool operator()(const std::string &lhs, const std::string &rhs) const {
        return std::strcmp(lhs.c_str(), rhs.c_str()) < 0;
    }
};

typedef std::map<std::string, std::string, defCompareStrings> defAliasMap;
typedef std::map<std::string, std::string, defCompareStrings> defDefineMap;

class defrData {

public:
    defrData();
    ~defrData();

    static void reset();
    FILE*  defrLog; 
    char   defPropDefType; // save the current type of the property
    char*  ch; 
    char*  defMsg; 
    char*  deftoken; 
    char*  deftokenVal; 
    char*  last; 
    char*  magic; 
    char*  next; 
    char*  pv_deftoken; 
    char*  rowName; // to hold the rowName for message
    char*  shieldName; // to hold the shieldNetName
    char*  shiftBuf; 
    char*  warningMsg; 
    double save_x; 
    double save_y; 
    double lVal;
    double rVal;
    int  aOxide; // keep track for oxide
    int  assertionWarnings; 
    int  bit_is_keyword; 
    int  bitsNum; // Scanchain Bits value
    int  blockageWarnings; 
    int  by_is_keyword; 
    int  caseSensitiveWarnings; 
    int  componentWarnings; 
    int  constraintWarnings; 
    int  cover_is_keyword; 
    int  defIgnoreVersion; // ignore checking version number
    int  defInPropDef; 
    int  defInvalidChar; 
    int  defMsgCnt; 
    int  defMsgPrinted; // number of msgs output so far
    int  defPrintTokens; 
    int  defRetVal; 
    int  def_warnings; 
    int  defaultCapWarnings; 
    int  do_is_keyword; 
    int  dumb_mode; 
    int  errors; 
    int  fillWarnings; 
    int  first_buffer; 
    int  fixed_is_keyword; 
    int  gcellGridWarnings; 
    int  hasBlkLayerComp; // only 1 BLOCKAGE/LAYER/COMP
    int  hasBlkLayerSpac; // only 1 BLOCKAGE/LAYER/SPACING
    int  hasBlkLayerTypeComp; // SLOTS or FILLS
    int  hasBlkPlaceComp; // only 1 BLOCKAGE/PLACEMENT/COMP
    int  hasBlkPlaceTypeComp; // SOFT or PARTIAL
    int  hasBusBit; // keep track BUSBITCHARS is in the file
    int  hasDes; // keep track DESIGN is in the file
    int  hasDivChar; // keep track DIVIDERCHAR is in the file
    int  hasDoStep; 
    int  hasNameCase; // keep track NAMESCASESENSITIVE is in the file
    int  hasOpenedDefLogFile; 
    int  hasPort; // keep track is port defined in a Pin
    int  hasVer; // keep track VERSION is in the file
    int  iOTimingWarnings; 
    int  input_level; 
    int  mask_is_keyword; 
    int  mustjoin_is_keyword; 
    int  names_case_sensitive; // always true in 5.6
    int  needNPCbk; // if cbk for net path is needed
    int  needSNPCbk; // if cbk for snet path is needed
    int  netOsnet; // net = 1 & snet = 2
    int  netWarnings; 
    int  new_is_keyword; 
    int  nl_token; 
    int  no_num; 
    int  nonDefaultWarnings; 
    int  nondef_is_keyword; 
    int  ntokens; 
    int  orient_is_keyword; 
    int  parsing_property; 
    int  pinExtWarnings; 
    int  pinWarnings; 
    int  real_num; 
    int  rect_is_keyword; 
    int  regTypeDef; // keep track that region type is defined 
    int  regionWarnings; 
    int  ringPlace; 
    int  routed_is_keyword; 
    int  rowWarnings; 
    int  sNetWarnings; 
    int  scanchainWarnings; 
    int  shield; // To identify if the path is shield for 5.3
    int  shiftBufLength; 
    int  specialWire_mask; 
    int  step_is_keyword; 
    int  stylesWarnings; 
    int  trackWarnings; 
    int  unitsWarnings; 
    int  versionWarnings; 
    int  viaRule; // keep track the viarule has called first
    int  viaWarnings; 
    int  virtual_is_keyword; 
    int  deftokenLength;
    long long nlines;

    std::vector<char>  History_text; 
    defAliasMap        def_alias_set; 
    defDefineMap       def_defines_set;

    char*  specialWire_routeStatus;
    char*  specialWire_routeStatusName;
    char*  specialWire_shapeType;
    double VersionNum;
    double xStep;
    double yStep;
        
    //defrParser vars.
    defiPath PathObj;
    defiProp Prop;
    defiSite Site;
    defiComponent Component;
    defiComponentMaskShiftLayer ComponentMaskShiftLayer;
    defiNet Net;
    defiPinCap PinCap;
    defiSite CannotOccupy;
    defiSite Canplace;
    defiBox DieArea;
    defiPin Pin;
    defiRow Row;
    defiTrack Track;
    defiGcellGrid GcellGrid;
    defiVia Via;
    defiRegion Region;
    defiGroup Group;
    defiAssertion Assertion;
    defiScanchain Scanchain;
    defiIOTiming IOTiming;
    defiFPC FPC;
    defiTimingDisable TimingDisable;
    defiPartition Partition;
    defiPinProp PinProp;
    defiBlockage Blockage;
    defiSlot Slot;
    defiFill Fill;
    defiNonDefault NonDefault;
    defiStyles Styles;
    defiGeometries Geometries;
    int doneDesign;      // keep track if the Design is done parsing
    
    // The following global variables are for storing the propertydefination
    // types.  Only real & integer need to store since the parser can
    // distinguish string and quote string
    defiPropType CompProp;
    defiPropType CompPinProp;
    defiPropType DesignProp;
    defiPropType GroupProp;
    defiPropType NDefProp;
    defiPropType NetProp;
    defiPropType PinDefProp;
    defiPropType RegionProp;
    defiPropType RowProp;
    defiPropType SNetProp;

    // Flags to control what happens
    int NeedPathData;

    defiSubnet* Subnet;
    int msgLimit[DEF_MSGS];
    char buffer[IN_BUF_SIZE];
    char* ring[RING_SIZE];
    int ringSizes[RING_SIZE];
    std::string stack[20];  /* the stack itself */
};

extern defrData *defData;

END_LEFDEF_PARSER_NAMESPACE

#endif

