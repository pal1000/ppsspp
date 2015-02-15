// Copyright (c) 2012- PPSSPP Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0 or later versions.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official git repository and contact information can be found at
// https://github.com/hrydgard/ppsspp and http://www.ppsspp.org/.

#include "ext/sfmt19937/sfmt.h"

#include "sceSfmt19937.h"
#include "Common/Log.h"
#include "Core/HLE/HLE.h"
#include "Core/HLE/FunctionWrappers.h"

static int sceSfmt19937InitGenRand(u32 sfmt, u32 seed) {
	if (!Memory::IsValidAddress(sfmt)) {
		ERROR_LOG(HLE, "sceSfmt19937InitGenRand(sfmt=%08x, seed=%08x) - bad address(es)", sfmt, seed);
		return -1;
	}
	INFO_LOG(HLE, "sceSfmt19937InitGenRand(sfmt=%08x, seed=%08x)", sfmt, seed);

	sfmt_t *psfmt = (sfmt_t *)Memory::GetPointerUnchecked(sfmt);
	sfmt_init_gen_rand(psfmt, seed);

	return 0;
}

static int sceSfmt19937InitByArray(u32 sfmt, u32 seeds, int seedslen) {
	if (!Memory::IsValidAddress(sfmt) || !Memory::IsValidAddress(seeds) || !Memory::IsValidAddress(seeds + 4 * (seedslen - 1))) {
		ERROR_LOG(HLE, "sceSfmt19937InitByArray(sfmt=%08x, seeds=%08x, seedslen=%08x)  - bad address(es)", sfmt, seeds, seedslen);
		return -1;
	}
	INFO_LOG(HLE, "sceSfmt19937InitByArray(sfmt=%08x, seeds=%08x, seedslen=%08x)", sfmt, seeds, seedslen);

	sfmt_t *psfmt = (sfmt_t *)Memory::GetPointerUnchecked(sfmt);
	u32 *pseeds = (u32 *)Memory::GetPointerUnchecked(seeds);
	sfmt_init_by_array(psfmt, pseeds, seedslen);

	return 0;
}

static u32 sceSfmt19937GenRand32(u32 sfmt) {
	if (!Memory::IsValidAddress(sfmt)) {
		ERROR_LOG(HLE, "sceSfmt19937GenRand32(sfmt=%08x)  - bad address(es)", sfmt);
		return -1;
	}
	INFO_LOG(HLE, "sceSfmt19937GenRand32(sfmt=%08x)", sfmt);

	sfmt_t *psfmt = (sfmt_t *)Memory::GetPointerUnchecked(sfmt);
	u32 ret = sfmt_genrand_uint32(psfmt);

	return ret;
}

static u64 sceSfmt19937GenRand64(u32 sfmt) {
	if (!Memory::IsValidAddress(sfmt)) {
		ERROR_LOG(HLE, "sceSfmt19937GenRand64(sfmt=%08x)  - bad address(es)", sfmt);
		return -1;
	}
	INFO_LOG(HLE, "sceSfmt19937GenRand64(sfmt=%08x)", sfmt);

	sfmt_t *psfmt = (sfmt_t *)Memory::GetPointerUnchecked(sfmt);
	u64 ret = sfmt_genrand_uint64(psfmt);

	return ret;
}

static int sceSfmt19937FillArray32(u32 sfmt, u32 array, int arraylen) {
	if (!Memory::IsValidAddress(sfmt) || !Memory::IsValidAddress(array) || !Memory::IsValidAddress(array + 4 * (arraylen - 1))) {
		ERROR_LOG(HLE, "sceSfmt19937FillArray32(sfmt=%08x, ar=%08x, arlen=%08x)  - bad address(es)", sfmt, array, arraylen);
		return -1;
	}
	INFO_LOG(HLE, "sceSfmt19937FillArray32(sfmt=%08x, ar=%08x, arlen=%08x)", sfmt, array, arraylen);

	sfmt_t *psfmt = (sfmt_t *)Memory::GetPointerUnchecked(sfmt);
	u32 *parray = (u32 *)Memory::GetPointerUnchecked(array);
	sfmt_fill_array32(psfmt, parray, arraylen);

	return 0;
}

static int sceSfmt19937FillArray64(u32 sfmt, u64 array, int arraylen) {
	if (!Memory::IsValidAddress(sfmt) || !Memory::IsValidAddress(array) || !Memory::IsValidAddress(array + 8 * (arraylen - 1))) {
		ERROR_LOG(HLE, "sceSfmt19937FillArray64(sfmt=%08x, ar=%08x, arlen=%08x)  - bad address(es)", sfmt, array, arraylen);
		return -1;
	}
	INFO_LOG(HLE, "sceSfmt19937FillArray64(sfmt=%08x, ar=%08x, arlen=%08x)", sfmt, array, arraylen);

	sfmt_t *psfmt = (sfmt_t *)Memory::GetPointerUnchecked(sfmt);
	u64 *parray = (u64 *)Memory::GetPointerUnchecked(array);
	sfmt_fill_array64(psfmt, parray, arraylen);

	return 0;
}

const HLEFunction sceSfmt19937[] =
{
	{ 0x161ACEB2, WrapI_UU<sceSfmt19937InitGenRand>, "sceSfmt19937InitGenRand" },
	{ 0xDD5A5D6C, WrapI_UUI<sceSfmt19937InitByArray>, "sceSfmt19937InitByArray" },
	{ 0xB33FE749, WrapU_U<sceSfmt19937GenRand32>, "sceSfmt19937GenRand32" },
	{ 0xD5AC9F99, WrapU64_U<sceSfmt19937GenRand64>, "sceSfmt19937GenRand64" },
	{ 0xDB025BFA, WrapI_UUI<sceSfmt19937FillArray32>, "sceSfmt19937FillArray32" },
	{ 0xEE2938C4, WrapI_UU64I<sceSfmt19937FillArray64>, "sceSfmt19937FillArray64" },
};

void Register_sceSfmt19937()
{
	RegisterModule("sceSfmt19937", ARRAY_SIZE(sceSfmt19937), sceSfmt19937);
}
