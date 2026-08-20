#pragma once

#include <functional>
#include <stdio.h>

#define ERROR(msg, ...)	\
	fprintf(stderr, msg "\n", ##__VA_ARGS__);	\
	exit(1);

#define CHECK_VK_RESULT(res, msg)	\
	if(res != VK_SUCCESS){	\
		ERROR("Error in %s:%d - %s, code %x", __FILE__, __LINE__, msg, res);	\
	}

#define ARRAY_SIZE_IN_ELEMENTS(array)	\
	(sizeof(array) / sizeof(array[0]))

namespace czx {

	template <typename T, typename... Rest>
	void hashCombine(std::size_t& seed, const T& v, const Rest&... rest) {
		seed ^= std::hash<T>{}(v)+0x9e3779b9 + (seed << 6) + (seed >> 2);
		(hashCombine(seed, rest), ...);
	};
}	// namespace czx