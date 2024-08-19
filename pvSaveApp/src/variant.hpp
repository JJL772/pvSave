
#pragma once

#include <cstdint>
#include <cstddef>
#include <typeindex>
#include <typeinfo>
#include <type_traits>
#include <cstring>
#include <utility>
#include <string>

#include "utility.hpp"

namespace pvsave {

/**
 * \brief Sequential indices for common types
 * See Variant::type_code()
 * Use these in switch statements to generate a jump table instead of a cascading if statement
 */
enum class ETypeCode : uint16_t {
	VOID = 0,
	INT8, UINT8, INT16, UINT16,
	INT32, UINT32, INT64, UINT64,
	STRING, FLOAT, DOUBLE, POINTER, REFERENCE, CHAR, CSTRING,
	OTHER,
};

template<typename T>
constexpr ETypeCode type_code_for() { return ETypeCode::OTHER; }

template<> constexpr ETypeCode type_code_for<void>() { return ETypeCode::VOID; }
template<> constexpr ETypeCode type_code_for<int8_t>() { return ETypeCode::INT8; }
template<> constexpr ETypeCode type_code_for<uint8_t>() { return ETypeCode::UINT8; }
template<> constexpr ETypeCode type_code_for<int16_t>() { return ETypeCode::INT16; }
template<> constexpr ETypeCode type_code_for<uint16_t>() { return ETypeCode::UINT16; }
template<> constexpr ETypeCode type_code_for<int32_t>() { return ETypeCode::INT32; }
template<> constexpr ETypeCode type_code_for<uint32_t>() { return ETypeCode::UINT32; }
template<> constexpr ETypeCode type_code_for<int64_t>() { return ETypeCode::INT64; }
template<> constexpr ETypeCode type_code_for<uint64_t>() { return ETypeCode::UINT64; }
template<> constexpr ETypeCode type_code_for<std::string>() { return ETypeCode::STRING; }
template<> constexpr ETypeCode type_code_for<float>() { return ETypeCode::FLOAT; }
template<> constexpr ETypeCode type_code_for<double>() { return ETypeCode::DOUBLE; }
template<> constexpr ETypeCode type_code_for<char>() { return ETypeCode::CHAR; }
template<> constexpr ETypeCode type_code_for<const char*>() { return ETypeCode::CSTRING; }


class TypeCode {
public:
	
	TypeCode() = default;
	TypeCode(const TypeCode& a) {
		m_code = a.m_code;
		m_index = a.m_index;
	}

	template<typename T>
	static TypeCode from_type() {
		TypeCode t;
		t.set<T>();
		return t;
	}

	template<typename T>
	inline void set() {
		m_index = typeid(T);
		m_code = type_code_for<T>();
	}

	inline void set(const std::type_index& index, ETypeCode code) {
		m_index = index;
		m_code = code;
	}

	inline void clear() { set<void>(); }

	inline const std::type_index& type_index() const { return m_index; }
	inline ETypeCode type_code() const { return m_code; }

	template<typename T>
	inline bool is() const {
		return m_index == typeid(T);
	}

	inline bool operator==(const TypeCode& other) const {
		return other.m_index == m_index;
	}

	inline bool operator!=(const TypeCode& other) const {
		return other.m_index != m_index;
	}

	inline bool operator==(const std::type_index& other) const {
		return other == m_index;
	}

	inline bool operator!=(const std::type_index& other) const {
		return other != m_index;
	}

private:
	std::type_index m_index = typeid(void);
	ETypeCode m_code = type_code_for<void>();
};


/**
 * Sort-of type-safe union
 * Unlike std::variant, this stores all types (POD or not) in the same buffer
 */
template<typename...Types>
class Variant {
	enum class ProxyOp {
		Construct,		// Default construct
		Destruct,
		Copy,			// Copy assign
		Move			// Move assign
	};

	using ProxyPtr = void(*)(Variant<Types...>* var, ProxyOp op, const void* src);

	static constexpr size_t ALIGNMENT = align_max<Types...>();
	static constexpr size_t SIZE = size_max<Types...>();
public:

	enum TypeFlags : uint16_t {
		Movable = (1<<1),		// has move assignment
		Trivial = (1<<2),		// Trivial/POD type
	};

	Variant() = default;

	template<class T>
	Variant(const T& value) {
		set(value);
	}

	Variant(Variant&& other) {
		*this = other;
	}

	Variant(const Variant& other) {
		*this = other;
	}

	Variant& operator=(const Variant& other) {
		copy_from(other, false);
		return *this;
	}

	Variant& operator=(Variant&& other) {
		copy_from(other, true);

		// Avoid duplicate destroys
		other.m_proxy = nullptr;
		other.m_flags = 0;
		other.m_type.clear();
		return *this;
	}

	/**
	 * Assign this variant to a new type or value
	 */
	template<typename T>
	Variant& operator=(const T& other) {
		set(other);
		return *this;
	}

	~Variant() {
		destruct();
	}

	inline std::type_index typeindex() const { return m_type.type_index(); }

	inline void clear() { destruct(); }

	inline void destruct() {
		if (m_type == typeid(void))
			return; // Already destroyed
		m_proxy(this, ProxyOp::Destruct, nullptr);
		m_type.set<void>();
		m_flags = 0;
	}

	/**
	 * Construct a new T in this variant
	 * \tparam T type to construct
	 * \tparam A arguments
	 * \param args Arguments to pass to T's constructor
	 * \return Reference to the newly created T
	 */
	template<typename T, typename...A>
	inline T& construct(A&&... args) {
		destruct(); // Kill off any previously stored data
		m_type.set<T>();

		m_flags = 0;
		if constexpr (std::is_move_assignable_v<T>)
			m_flags |= Movable;
		if constexpr (std::is_trivial_v<T>)
			m_flags |= Trivial;

		// Build wrappers around common type-dependent operations. This is so we can call the right methods even when no type is readily available
		m_proxy = [](Variant<Types...>* v, ProxyOp op, const void* src) {
			switch(op) {
			case ProxyOp::Construct:
				new (v->data()) T;
				return;
			case ProxyOp::Destruct:
				if constexpr (!std::is_trivial_v<T>)
					v->get_unchecked<T>()->~T();
				return;
			case ProxyOp::Move:
				if constexpr (std::is_move_assignable_v<T>) {
					(*v->get_unchecked<T>()) = std::move(*reinterpret_cast<const T*>(src));
					return;
				}
				else [[fallthrough]];
			case ProxyOp::Copy:
				(*v->get_unchecked<T>()) = *reinterpret_cast<const T*>(src);
				return;
			}
		};

		new (m_data) T(args...);
		return reinterpret_cast<T&>(m_data);
	}

	template<typename T>
	inline T* get() {
		if (m_type == typeid(T))
			return reinterpret_cast<T*>(m_data);
		return nullptr;
	}

	template<typename T>
	inline const T* get() const {
		if (m_type == typeid(T))
			return reinterpret_cast<T*>(m_data);
		return nullptr;
	}

	/**
	 * Returns the contained object by value, or default constructed T if types don't match
	 * Use get() if you want to check for validity (or pair with is<> checks)
	 */
	template<typename T>
	inline T value() const {
		if (m_type != typeid(T))
			return T();
		return *reinterpret_cast<const T*>(m_data);
	}

	/**
	 * Set to a new value and/or type
	 * \brief value The new value of the variant
	 */
	template<typename T>
	inline void set(const T& value) {
		if (m_type != typeid(T)) {
			destruct();
			construct<T>();
		}
		(*get_unchecked<T>()) = value;
	}

	/**
	 * Check if the variant's current type is T
	 * Not constrained to this variant's types
	 * \tparam T type to compare against
	 */
	template<typename T>
	inline bool is() const {
		return m_type.is<T>();
	}

	inline void* data() { return m_data; }
	inline const void* data() const { return m_data; }

	inline constexpr size_t alignment() const { return ALIGNMENT; }

	/**
	 * Returns size, in bytes, of the internal data store
	 */
	inline constexpr size_t data_size() const { return SIZE; }

	inline uint16_t type_flags() const { return m_flags; }

	/**
	 * \brief Returns the type code of the current item
	 * This type code isn't necessarily unique and is only defined for common primitive types
	 * Intended to be used in switch statements to avoid slow cascading if blocks
	 */
	inline ETypeCode type_code() const { return m_type.type_code(); }

	/**
	 * \brief Returns complete type description
	 */
	inline const TypeCode& type() const { return m_type; }

	/**
	 * Tests for properties of the current type
	 */
	inline bool is_trivial() const { return !!(m_flags & Trivial); }
	inline bool is_movable() const { return !!(m_flags & Movable); }

protected:
	void copy_from(const Variant& other, bool move) {
        // Can't copy void, we just clear ourselves out
        if (other.type_code() == ETypeCode::VOID) {
            clear();
            return;
        }

		// Differing typeindex- dispose current, default construct new
		if (m_type != other.m_type) {
			destruct();
			if (!other.is_trivial())
				other.m_proxy(this, ProxyOp::Construct, nullptr);
			m_type = other.m_type;
			m_proxy = other.m_proxy;
		}

		// Invoke copy ctor for non-trivial types
		if (!other.is_trivial())
			other.m_proxy(this, move ? ProxyOp::Move : ProxyOp::Copy, const_cast<char*>(other.m_data));
		else
			std::memcpy(m_data, other.data(), SIZE); // Use raw memcpy for everything else
		m_flags = other.m_flags;
		m_type = other.m_type;
		m_proxy = other.m_proxy;
	}


	template<typename T>
	inline T* get_unchecked() { return reinterpret_cast<T*>(m_data); }

	// Helpers to call the correct class operators
	ProxyPtr m_proxy = nullptr;
	
	uint16_t m_flags = 0;
	TypeCode m_type = TypeCode::from_type<void>();

	alignas(ALIGNMENT) char m_data[SIZE];
};

}