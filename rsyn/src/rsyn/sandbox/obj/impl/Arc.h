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
Design
SandboxArc::getDesign() {
	return getFromPin().getDesign();
} // end method

// -----------------------------------------------------------------------------

inline
const Design
SandboxArc::getDesign() const {
	return getFromPin().getDesign();
} // end method

// -----------------------------------------------------------------------------

inline
Arc
SandboxArc::getRelated() const {
	Rsyn::Pin relatedFromPin = getFromPin().getRelated();
	Rsyn::Pin relatedToPin = getToPin().getRelated();

	if (!relatedFromPin || !relatedToPin)
		return nullptr;

	switch (getType()) {
		case INSTANCE_ARC: {
			Rsyn::Instance relatedInstance = getInstance().getRelated();
			return relatedInstance?
				relatedInstance.getArc(relatedFromPin, relatedToPin) : nullptr;
		} // end case

		case NET_ARC: {
			Rsyn::Net net = relatedFromPin.getNet();
			return net?
				net.getArc(relatedFromPin, relatedToPin) : nullptr;
		} // end case
	} // end switch

	return nullptr;
} // end method

// -----------------------------------------------------------------------------

inline
ArcType
SandboxArc::getType() const {
	return data->type;
} // end method

// -----------------------------------------------------------------------------

inline
SandboxInstance
SandboxArc::getInstance() const {
	return data->type == INSTANCE_ARC? data->from.getInstance() : nullptr;
} // end method

// -----------------------------------------------------------------------------

inline
SandboxNet
SandboxArc::getNet() const {
	return data->type == NET_ARC? data->netData : nullptr;
} // end method

// -----------------------------------------------------------------------------

inline
LibraryArc
SandboxArc::getLibraryArc() const {
	return getType() == INSTANCE_ARC? data->libraryArcData : nullptr;
} // end method

// -----------------------------------------------------------------------------

inline
SandboxPin
SandboxArc::getFromPin() const {
	return data->from;
} // end method

// -----------------------------------------------------------------------------

inline
SandboxPin
SandboxArc::getToPin() const {
	return data->to;
} // end method

// -----------------------------------------------------------------------------

inline
std::string
SandboxArc::getName() const {
	return getFromName() + "->" + getToName();
} // end method

// -----------------------------------------------------------------------------

inline
std::string
SandboxArc::getFullName() const {
	return getInstance().getName() + ":" + getName();
} // end method

// -----------------------------------------------------------------------------

inline
const std::string
SandboxArc::getFromName() const {
	return getFromPin().getName();
} // end method

// -----------------------------------------------------------------------------

inline
const std::string
SandboxArc::getToName() const {
	return getToPin().getName();
} // end method

// -----------------------------------------------------------------------------

inline
SandboxNet
SandboxArc::getFromNet() const {
	return getFromPin().getNet();
} // end method

// -----------------------------------------------------------------------------

inline
SandboxNet
SandboxArc::getToNet() const {
	return getToPin().getNet();
} // end method

} // end namespace