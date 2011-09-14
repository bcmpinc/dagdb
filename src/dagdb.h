#ifndef DAGDB_INTERFACE_H
#define DAGDB_INTERFACE_H

#include <cstdint>
#include <memory>

// This header specifies the various interfaces that can be used by the various 
// modules of this library.

namespace dagdb { //
namespace interface { //

	class map;
	//class element;

	/**
	 * Type used for keys in maps.
	 */
	/*class element {
	public:
		virtual ~element() {};
	};
	
	class data : element {
	public:
		virtual size_t length() = 0;
		virtual void read() = 0;
	};*/
	typedef int element;
	
	/**
	 * Iterates over the elements in a map.
	 */
	class map_iterator {
	private:
		class map_iterator_concept {
		public:
			virtual void operator++()=0;
			virtual element key()=0;
			virtual element value()=0;
			virtual ~map_iterator_concept() {}
		};
		template <class T>
		class map_iterator_model_fn : public map_iterator_concept {
		public:
			map_iterator_model_fn(T m) : ref(m) {}
			virtual void operator++() { ++ref; }
			virtual element key() { return ref.key(); }
			virtual element value() { return ref.value(); }
		private:
			T ref;
		};
		template <class T>
		class map_iterator_model_stl : public map_iterator_concept {
		public:
			map_iterator_model_stl(T m) : ref(m) {}
			virtual void operator++() { ++ref; }
			virtual element key() { return ref->first; }
			virtual element value() { return ref->second; }
		private:
			T ref;
		};
		template <class T>
		class map_iterator_selector {
		private:
			static T t;
			template <class C>
			static map_iterator_model_fn<T> test(decltype(&C::key)) {}
			template <class C>
			static map_iterator_model_stl<T> test(decltype(&C::operator->)) {}
			template <class C>
			static void test(...) {}
		public:
			static decltype(test<T>(NULL)) type;
		};
		std::shared_ptr<map_iterator_concept> ref;
	public:
		template <class T> 
		map_iterator(T m) : ref(new decltype(map_iterator_selector<T>::type)(m)) {}
		void operator++() { ref->operator++(); }
		element key() { return ref->key(); }
		element value() { return ref->value(); }
	};

	/**
	 * Searchable element -> element map.
	 */
	class map {
	private:
		class map_concept {
		public:
			virtual map_iterator find(const element) = 0;
			virtual map_iterator begin() = 0;
			virtual map_iterator end() = 0;
			virtual ~map_concept() {}
		};
		template <class T>
		class map_model : public map_concept {
		public:
			map_model(T m) : ref(m) {}
			virtual map_iterator begin() { return ref.begin(); }
			virtual map_iterator end() { return ref.end(); }
			virtual map_iterator find(const element elt) { return ref.find(elt); }
		private:
			T ref;
		};
		std::shared_ptr<map_concept> ref;
	public:
		template <class T> 
		map(T m) : ref(new map_model<T>(m)) {}
		
		/** 
		 * Searches for the element associated with the given element.
		 * Returns an empty pointer if the given element is not associated 
		 * with another element.
		 */
		map_iterator find(const element elt) { return ref->find(elt); }
		
		/**
		 * Iterator that can be used to enumerate all elements in the map.
		 */
		map_iterator begin() { return ref->begin(); }
		
		/**
		 * Past-end iterator.
		 */
		map_iterator end() { return ref->end(); }
	};
	
};
};

#endif
