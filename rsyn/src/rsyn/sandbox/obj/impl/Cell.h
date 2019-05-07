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
Cell
SandboxCell::getRelated() const {
	Rsyn::Instance instance = SandboxInstance::getRelated();
	return instance? instance.asCell() : nullptr;
} // end method

// -----------------------------------------------------------------------------

inline
const std::string &
SandboxCell::getLibraryCellName() const {
	return data->lcell->name;
} // end method

// -----------------------------------------------------------------------------

inline
LibraryCell
SandboxCell::getLibraryCell() const {
	return data->lcell;
} // end method

// -----------------------------------------------------------------------------

inline
SandboxPin
SandboxCell::getPinByLibraryPin(const LibraryPin lpin) const {
	return data->pins[lpin.data->index];
} // end method

// -----------------------------------------------------------------------------

inline
void
SandboxCell::remap(LibraryCell libraryCell) {
	getSandbox().remap(*this, libraryCell);
} // end method

// -----------------------------------------------------------------------------

inline
void
SandboxCell::remap(const std::string &libraryCellName) {
	getSandbox().remap(*this, libraryCellName);
} // end method

} // end namespace