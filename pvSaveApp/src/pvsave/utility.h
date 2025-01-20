//////////////////////////////////////////////////////////////////////////////
// This file is part of 'pvSave'.
// It is subject to the license terms in the LICENSE.txt file found in the 
// top-level directory of this distribution and at: 
//    https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. 
// No part of 'pvSave', including this file, 
// may be copied, modified, propagated, or distributed except according to 
// the terms contained in the LICENSE.txt file.
//////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstddef>

namespace pvsave {

/**
 * \brief Returns the maximum size of the types in the sequence
 */
template <typename T>
constexpr size_t size_max() {
	return sizeof(T);
}

template <typename A, typename B>
constexpr size_t size_max() {
	return sizeof(A) > sizeof(B) ? sizeof(A) : sizeof(B);
}

template <typename A, typename B, typename C, typename... T>
constexpr size_t size_max() {
	auto a = size_max<A, B>();
	auto b = size_max<C, T...>();
	return a > b ? a : b;
}

/**
 * \brief Returns the maximum alignment of the types if the sequence
 */
template <typename T>
constexpr size_t align_max() {
	return alignof(T);
}

template <typename A, typename B>
constexpr size_t align_max() {
	return alignof(A) > alignof(B) ? alignof(A) : alignof(B);
}

template <typename A, typename B, typename C, typename... T>
constexpr size_t align_max() {
	auto a = align_max<A, B>();
	auto b = align_max<C, T...>();
	return a > b ? a : b;
}

/** C++11 compat for C++17 features */
#undef IF_CONSTEXPR
#if __cplusplus >= 201703L
#define IF_CONSTEXPR constexpr
#else
#define IF_CONSTEXPR
#endif

}