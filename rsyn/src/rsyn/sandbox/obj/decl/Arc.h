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

class SandboxArc : public Proxy<SandboxArcData> {
RSYN_FRIEND_OF_GENERIC_LIST_COLLECTION;

friend class Sandbox;
private:
	SandboxArc(SandboxArcData * data) : Proxy(data) {}
	
public:
	
	SandboxArc() {}
	SandboxArc(std::nullptr_t) {}	
		
	std::string getName() const;
	std::string getFullName() const;
	
	const std::string getFromName() const;
	const std::string getToName() const;	

	Design getDesign();
	const Design getDesign() const;

	Arc getRelated() const;

	ArcType getType() const;
	
	SandboxInstance getInstance() const;
	SandboxNet getNet() const;
	LibraryArc getLibraryArc() const;
	
	SandboxPin getFromPin() const;
	SandboxPin getToPin() const;
	
	SandboxNet getFromNet() const;
	SandboxNet getToNet() const;	
}; // end class
	
} // end namespace
