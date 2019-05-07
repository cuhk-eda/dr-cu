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
 
#ifndef RSYN_SANDBOX_ATTRIBUTE_H
#define RSYN_SANDBOX_ATTRIBUTE_H

#include <deque>

namespace Rsyn {

class SandboxAttributeInitializer {
friend class Sandbox;
private:
	Sandbox sandbox;
	SandboxAttributeInitializer(Sandbox sandbox): sandbox(sandbox) {};
public:
	Sandbox getSandbox() { return sandbox; }
}; // end class

// -----------------------------------------------------------------------------

template<typename DefaultValueType>
class SandboxAttributeInitializerWithDefaultValue {
friend class Sandbox;
private:
	Sandbox sandbox;
	DefaultValueType defaultValue;
	SandboxAttributeInitializerWithDefaultValue(Sandbox sandbox, DefaultValueType defaultValue)
			: sandbox(sandbox), defaultValue(defaultValue) {};
public:
	Sandbox getSandbox() { return sandbox; }
	DefaultValueType &getDefaultValue() { return defaultValue; }
}; // end class

// -----------------------------------------------------------------------------

template<typename _Object, typename _ObjectExtension>
class SandboxAttributeImplementation;

// -----------------------------------------------------------------------------

template <typename RsynObject, typename RsynObjectExtension>
class SandboxAttribute : public std::shared_ptr<SandboxAttributeImplementation<RsynObject, RsynObjectExtension>> {
public:	
	SandboxAttribute() {}
	
	SandboxAttribute(SandboxAttributeInitializer initializer) { operator=(initializer); }
	void operator=(SandboxAttributeInitializer initializer);
	
	template<typename DefaultValueType>
	SandboxAttribute(SandboxAttributeInitializerWithDefaultValue<DefaultValueType> initializer) { operator=(initializer); }
	template<typename DefaultValueType>
	void operator=(SandboxAttributeInitializerWithDefaultValue<DefaultValueType> initializer);
	
	RsynObjectExtension &operator[](RsynObject obj);
	const RsynObjectExtension &operator[](RsynObject obj) const;
}; // end class

// -----------------------------------------------------------------------------

template<typename _Object, typename _ObjectReference, typename _ObjectExtension>
class SandboxAttributeBase {
private:
	// [TODO] Make sandbox and list const.
	
	Sandbox clsSandbox;
	List<_Object, RSYN_SANDBOX_LIST_CHUNCK_SIZE> *clsListPtr;
	_ObjectExtension clsDefaultValue;
	
	typename List<_Object, RSYN_SANDBOX_LIST_CHUNCK_SIZE>::CreateElementCallbackHandler clsHandlerOnCreate;
	typename List<_Object, RSYN_SANDBOX_LIST_CHUNCK_SIZE>::DestructorCallbackHandler clsListDestructorCallbackHandler;
	
	std::deque<_ObjectExtension> clsData;
	
	void accommodate(const Index index) {
		if (index >= clsData.size()) {
			clsData.resize(index + 1, clsDefaultValue);
		} // end if
	} // end method
	
protected:
	
	void setupCallbacks() {
		clsHandlerOnCreate = clsListPtr->addCreateCallback([&](const int index) {
			accommodate(clsListPtr->largestId());
		}); // end method
		
		clsListDestructorCallbackHandler = clsListPtr->addDestructorEventCallback([&]() {
			clsListPtr = nullptr;
		}); // end method		
		
	} // end method
	
	void load(Sandbox sandbox, List<_Object, RSYN_SANDBOX_LIST_CHUNCK_SIZE> &list, _ObjectExtension defaultValue = _ObjectExtension()) {
		clsSandbox = sandbox;
		clsListPtr = &list;
		clsDefaultValue = defaultValue;
		accommodate(clsListPtr->largestId());
		setupCallbacks();
	} // end method		
	
public:
	
	SandboxAttributeBase() : clsSandbox(nullptr), clsListPtr(nullptr) {
	} // end constructor
	
	SandboxAttributeBase(const SandboxAttributeBase<_Object, _ObjectReference, _ObjectExtension> &other) {
		operator=(other);
	} // end constructor
	
	SandboxAttributeBase(Sandbox sandbox, List<_Object, RSYN_SANDBOX_LIST_CHUNCK_SIZE> &list) {
		load(sandbox, list);
	} // end constructor

	SandboxAttributeBase<_Object, _ObjectReference, _ObjectExtension> &
	operator=(const SandboxAttributeBase<_Object, _ObjectReference, _ObjectExtension> &other) {
		unload();
		
		clsSandbox = other.clsSandbox;
		clsListPtr = other.clsListPtr;
		clsData = other.clsData;
						
		// When a layer gets copied, we need to setup new callbacks.
		setupCallbacks();
		
		return *this;
	} // end method
	
	~SandboxAttributeBase() {
		unload();
	} // end constructor
	
	void unload() {
		if (clsSandbox) {
			clsSandbox = nullptr;
		} // end if
		
		if (clsListPtr) {
			clsListPtr->deleteCreateCallback(clsHandlerOnCreate);
			clsListPtr->deleteDestructorCallback(clsListDestructorCallbackHandler);
			clsListPtr = nullptr;
		} // end if

		clsData.clear();
		clsData.shrink_to_fit();
	} // end method	

	inline _ObjectExtension &operator[](_ObjectReference obj) { return clsData[clsSandbox.getId(obj)]; }
	inline const _ObjectExtension &operator[](_ObjectReference obj) const { return clsData[clsSandbox.getId(obj)]; }
}; // end class	

////////////////////////////////////////////////////////////////////////////////
// Attribute: (Generic) - Internal Use Only
////////////////////////////////////////////////////////////////////////////////

template<typename _Object, typename _ObjectExtension>
class SandboxAttributeImplementation {
}; // end class

////////////////////////////////////////////////////////////////////////////////
// Attribute: SandboxNet
////////////////////////////////////////////////////////////////////////////////

template<typename _ObjectExtension>
class SandboxAttributeImplementation<Rsyn::SandboxNet, _ObjectExtension>
	: public Rsyn::SandboxAttributeBase<SandboxNetData, SandboxNet, _ObjectExtension> {
public:
	SandboxAttributeImplementation() {}
	
	SandboxAttributeImplementation(SandboxAttributeInitializer initializer) { operator=(initializer); }
	void operator=(SandboxAttributeInitializer initializer) {
		Sandbox sandbox = initializer.getSandbox();
		SandboxAttributeBase<SandboxNetData, SandboxNet, _ObjectExtension>::load(sandbox, sandbox.data->nets);
	} // end operator
	
	template<typename DefaultValueType>
	SandboxAttributeImplementation(SandboxAttributeInitializerWithDefaultValue<DefaultValueType> initializer) { operator=(initializer); }
	template<typename DefaultValueType>
	void operator=(SandboxAttributeInitializerWithDefaultValue<DefaultValueType> initializer) {
		Sandbox sandbox = initializer.getSandbox();
		SandboxAttributeBase<SandboxNetData, SandboxNet, _ObjectExtension>::load(sandbox, sandbox.data->nets, initializer.getDefaultValue());
	} // end operator		
}; // end class

////////////////////////////////////////////////////////////////////////////////
// Attribute: SandboxInstance
////////////////////////////////////////////////////////////////////////////////

template<typename _ObjectExtension>
class SandboxAttributeImplementation<Rsyn::SandboxInstance, _ObjectExtension>
	: public Rsyn::SandboxAttributeBase<SandboxInstanceData, SandboxInstance, _ObjectExtension> {
public:
	SandboxAttributeImplementation() {}
	
	SandboxAttributeImplementation(SandboxAttributeInitializer initializer) { operator=(initializer); }
	void operator=(SandboxAttributeInitializer initializer) {
		Sandbox sandbox = initializer.getSandbox();
		SandboxAttributeBase<SandboxInstanceData, SandboxInstance, _ObjectExtension>::load(sandbox, sandbox.data->instances);
	} // end operator
	
	template<typename DefaultValueType>
	SandboxAttributeImplementation(SandboxAttributeInitializerWithDefaultValue<DefaultValueType> initializer) { operator=(initializer); }
	template<typename DefaultValueType>
	void operator=(SandboxAttributeInitializerWithDefaultValue<DefaultValueType> initializer) {
		Sandbox sandbox = initializer.getSandbox();
		SandboxAttributeBase<SandboxInstanceData, SandboxInstance, _ObjectExtension>::load(sandbox, sandbox.data->instances, initializer.getDefaultValue());
	} // end operator	
}; // end class

////////////////////////////////////////////////////////////////////////////////
// Attribute: SandboxPin
////////////////////////////////////////////////////////////////////////////////

template<typename _ObjectExtension>
class SandboxAttributeImplementation<Rsyn::SandboxPin, _ObjectExtension>
	: public Rsyn::SandboxAttributeBase<SandboxPinData, SandboxPin, _ObjectExtension> {
public:
	SandboxAttributeImplementation() {}
	
	SandboxAttributeImplementation(SandboxAttributeInitializer initializer) { operator=(initializer); }
	void operator=(SandboxAttributeInitializer initializer) {
		Sandbox sandbox = initializer.getSandbox();
		SandboxAttributeBase<SandboxPinData, SandboxPin, _ObjectExtension>::load(sandbox, sandbox.data->pins);
	} // end operator
	
	template<typename DefaultValueType>
	SandboxAttributeImplementation(SandboxAttributeInitializerWithDefaultValue<DefaultValueType> initializer) { operator=(initializer); }
	template<typename DefaultValueType>
	void operator=(SandboxAttributeInitializerWithDefaultValue<DefaultValueType> initializer) {
		Sandbox sandbox = initializer.getSandbox();
		SandboxAttributeBase<SandboxPinData, SandboxPin, _ObjectExtension>::load(sandbox, sandbox.data->pins, initializer.getDefaultValue());
	} // end operator		
}; // end class

////////////////////////////////////////////////////////////////////////////////
// Attribute: SandboxArc
////////////////////////////////////////////////////////////////////////////////

template<typename _ObjectExtension>
class SandboxAttributeImplementation<Rsyn::SandboxArc, _ObjectExtension>
	: public Rsyn::SandboxAttributeBase<SandboxArcData, SandboxArc, _ObjectExtension> {
public:
	SandboxAttributeImplementation() {}
	
	SandboxAttributeImplementation(SandboxAttributeInitializer initializer) { operator=(initializer); }
	void operator=(SandboxAttributeInitializer initializer) {
		Sandbox sandbox = initializer.getSandbox();
		SandboxAttributeBase<SandboxArcData, SandboxArc, _ObjectExtension>::load(sandbox, sandbox.data->arcs);
	} // end operator

	template<typename DefaultValueType>
	SandboxAttributeImplementation(SandboxAttributeInitializerWithDefaultValue<DefaultValueType> initializer) { operator=(initializer); }
	template<typename DefaultValueType>
	void operator=(SandboxAttributeInitializerWithDefaultValue<DefaultValueType> initializer) {
		Sandbox sandbox = initializer.getSandbox();
		SandboxAttributeBase<SandboxArcData, SandboxArc, _ObjectExtension>::load(sandbox, sandbox.data->arcs, initializer.getDefaultValue());
	} // end operator
}; // end class

////////////////////////////////////////////////////////////////////////////////
// SandboxAttribute
////////////////////////////////////////////////////////////////////////////////

template<typename RsynObject, typename RsynObjectExtension>
inline
void SandboxAttribute<RsynObject, RsynObjectExtension>::operator=(SandboxAttributeInitializer initializer) {
	this->reset(new SandboxAttributeImplementation<RsynObject, RsynObjectExtension>(initializer));
} // end method

template<typename RsynObject, typename RsynObjectExtension>
template<typename DefaultValueType>
inline
void SandboxAttribute<RsynObject, RsynObjectExtension>::operator=(SandboxAttributeInitializerWithDefaultValue<DefaultValueType> initializer) {
	this->reset(new SandboxAttributeImplementation<RsynObject, RsynObjectExtension>(initializer));
} // end method

template<typename RsynObject, typename RsynObjectExtension>
inline
RsynObjectExtension &SandboxAttribute<RsynObject, RsynObjectExtension>::operator[](RsynObject obj) {
	return (*this)->operator[](obj); 
} // end method

template<typename RsynObject, typename RsynObjectExtension>
inline
const RsynObjectExtension &SandboxAttribute<RsynObject, RsynObjectExtension>::operator[](RsynObject obj) const {
	return (*this)->operator[](obj); 
} // end method

} // end namespace

#endif

