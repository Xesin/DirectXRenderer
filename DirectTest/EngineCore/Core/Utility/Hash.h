#pragma once

#include "../Maths/Common.h"

// This requires SSE4.2 which is present on Intel Nehalem (Nov. 2008)
// and AMD Bulldozer (Oct. 2011) processors.  I could put a runtime
// check for this, but I'm just going to assume people playing with
// DirectX 12 on Windows 10 have fairly recent machines.
#ifdef _M_X64
#define ENABLE_SSE_CRC32 1
#else
#define ENABLE_SSE_CRC32 0
#endif

#if ENABLE_SSE_CRC32
#pragma intrinsic(_mm_crc32_u32)
#pragma intrinsic(_mm_crc32_u64)
#endif

#ifdef _WIN32

//  Windows
#define cpuid(info, x)    __cpuidex(info, x, 0)

#else

//  GCC Intrinsics
#include <cpuid.h>
void cpuid(int info[4], int InfoType) {
	__cpuid_count(InfoType, 0, info[0], info[1], info[2], info[3]);
}

#endif

namespace Utility
{
	inline size_t HashRange(const uint32_t* const Begin, const uint32_t* const End, size_t Hash)
	{

		int info[4];
		cpuid(info, 0);
		int nIds = info[0];

		cpuid(info, 0x80000000);
		unsigned nExIds = info[0];

		bool HW_SSE42 = (info[2] & ((int)1 << 20)) != 0;
		if (HW_SSE42) {
			const uint64_t* Iter64 = (const uint64_t*)Math::AlignUp(Begin, 8);
			const uint64_t* const End64 = (const uint64_t* const)Math::AlignDown(End, 8);

			// If not 64-bit aligned, start with a single u32
			if ((uint32_t*)Iter64 > Begin)
				Hash = _mm_crc32_u32((uint32_t)Hash, *Begin);

			// Iterate over consecutive u64 values
			while (Iter64 < End64)
				Hash = _mm_crc32_u64((uint64_t)Hash, *Iter64++);

			// If there is a 32-bit remainder, accumulate that
			if ((uint32_t*)Iter64 < End)
				Hash = _mm_crc32_u32((uint32_t)Hash, *(uint32_t*)Iter64);
		}
		else {

			// An inexpensive hash for CPUs lacking SSE4.2
			for (const uint32_t* Iter = Begin; Iter < End; ++Iter)
				Hash = 16777619U * Hash ^ *Iter;
		}

		return Hash;
	}

	template <typename T> inline size_t HashState(const T* StateDesc, size_t Count = 1, size_t Hash = 2166136261U)
	{
		static_assert((sizeof(T) & 3) == 0 && alignof(T) >= 4, "State object is not word-aligned");
		return HashRange((uint32_t*)StateDesc, (uint32_t*)(StateDesc + Count), Hash);
	}

} // namespace Utility