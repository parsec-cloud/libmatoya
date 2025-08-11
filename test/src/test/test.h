// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#define GREEN "\x1b[92m"
#define RED   "\x1b[91m"
#define RESET "\x1b[0m"

extern uint64_t test_seed;

static inline void test_GetRandomBytes(void *buf, size_t size)
{
	uint8_t *dest = buf;
	while (size) {
		const size_t bytes = size < sizeof(test_seed) ? size : sizeof(test_seed);
		test_seed = test_seed * 6364136223846793005ULL + 1442695040888963407ULL;
		memcpy(dest, &test_seed, bytes);
		dest += bytes;
		size -= bytes;
	}
}

static inline uint32_t test_GetRandomUInt(uint32_t lo, uint32_t hi)
{
	uint32_t temp;
	test_GetRandomBytes(&temp, sizeof(temp));
	return lo + temp % (hi - lo);
}

#define test_print_cmp_(name, cmps, cmp, val, fmt) \
	printf("::%s file=%s,line=%d::%s %s" fmt "\n", (cmp) ? "notice" : "error", __FILE__, __LINE__, name, cmps, val);

#define test_cmp_(name, cmp, val, fmt) { \
	bool ___CMP___ = cmp; \
	test_print_cmp_(name, #cmp, ___CMP___, val, fmt); \
	if (!___CMP___) return false; \
}

#define test_cmp(name, cmp) \
	test_cmp_(name, (cmp), "", "%s")

#define test_cmp_warn_(name, cmp, val, fmt) { \
	bool ___CMP___ = cmp; \
	test_print_cmp_(name, #cmp, ___CMP___, val, fmt); \
	if (!___CMP___) return true; \
}

#define test_cmp_warn(name, cmp) \
	test_cmp_warn_(name, (cmp), "", "%s")

#define test_print_cmps(name, cmp, s) { \
	bool ___PCMP___ = cmp; \
	test_print_cmp_(name, #cmp, ___PCMP___, s, "%s") \
}

#define test_print_cmp(name, cmp) \
	test_print_cmps(name, cmp, "")

#define test_passed(name) \
	test_print_cmp_(name, "", true, "", "%s")

#define test_failed(name) { \
	test_print_cmp_(name, "", false, "", "%s"); \
	return false; \
}

#define test_cmpf(name, cmp, val) \
	test_cmp_(name, (cmp), val, ": %.2f")

#define test_cmpi32(name, cmp, val) \
	test_cmp_(name, cmp, val, ": %" PRId32)

#define test_cmpi64(name, cmp, val) \
	test_cmp_(name, (cmp), (int64_t) val, ": %" PRId64)

#define test_cmps(name, cmp, val) \
	test_cmp_(name, cmp, val, ": %s")
