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
Port
SandboxPort::getRelated() const {
	Rsyn::Instance instance = SandboxInstance::getRelated();
	return instance? instance.asPort() : nullptr;
} // end method

// -----------------------------------------------------------------------------

inline
SandboxPin
SandboxPort::getInnerPin() const {
	// Ports have just one pin, which is the inner pin...
	return data->pins[0];
} // end method

// -----------------------------------------------------------------------------

inline
SandboxPin
SandboxPort::getOuterPin() const {
	return nullptr;
} // end method

// -----------------------------------------------------------------------------

inline
SandboxPin
SandboxPort::getOtherPin(Rsyn::SandboxPin) const {
	return nullptr;
} // end method

// -----------------------------------------------------------------------------

inline
SandboxPin
SandboxPort::getAttachedPin() const {
	return data->attachedPin;
} // end method

// -----------------------------------------------------------------------------

inline
Direction
SandboxPort::getDirection() const {
	return Global::getReverseDirection(getInnerPin().getDirection());
} // end method

// -----------------------------------------------------------------------------

inline
bool
SandboxPort::isVirtual() const {
	return data->attachedPin;
} // end method

} // end namespace