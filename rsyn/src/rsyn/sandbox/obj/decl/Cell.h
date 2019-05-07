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

class SandboxCell : public SandboxInstance {
friend class Sandbox;
friend class SandboxInstance;
private:	
	SandboxCell(SandboxInstanceData * data) : SandboxInstance(data) {}
public:
	
	SandboxCell() : SandboxInstance(nullptr) {}
	SandboxCell(std::nullptr_t) : SandboxInstance(nullptr) {}

	Cell getRelated() const;

	const std::string &getLibraryCellName() const;

	LibraryCell getLibraryCell() const;
	
	SandboxPin getPinByLibraryPin(LibraryPin lpin) const;
	
	void remap(LibraryCell libraryCell);
	void remap(const std::string &libraryCellName);
	
}; // end class
	
} // end namespace