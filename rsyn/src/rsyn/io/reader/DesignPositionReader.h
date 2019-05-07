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
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   DesignPositionReader.h
 * Author: jucemar
 *
 * Created on 20 de Fevereiro de 2017, 19:12
 */

#ifndef RSYN_DESIGNPOSITIONREADER_H
#define RSYN_DESIGNPOSITIONREADER_H

#include <Rsyn/Session>
#include "rsyn/core/Rsyn.h"
#include <Rsyn/PhysicalDesign>

namespace Rsyn {

class DesignPositionReader : public Process {
protected:
	Rsyn::Session session;
	Rsyn::Design clsDesign;
	Rsyn::Module clsModule;
	Rsyn::PhysicalDesign clsPhysicalDesign;

public:
	DesignPositionReader() {}
	DesignPositionReader(const DesignPositionReader& orig) {}
	virtual ~DesignPositionReader() {}
	virtual bool run(const Rsyn::Json &params) override;
protected:
	bool load(const Rsyn::Json &params);
	void openDef(std::string & path);
}; // end class 

}

#endif /* RSYN_DESIGNPOSITIONREADER_H */

