/*
 * Copyright (c) 2014-2015, Hewlett-Packard Development Company, LP.
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details. You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * HP designates this particular file as subject to the "Classpath" exception
 * as provided by HP in the LICENSE.txt file that accompanied this code.
 */
#include "../include/common/raw_atomics.h"

#include <stdint.h>

namespace mvstore {
//namespace assorted {


bool assorted::raw_atomic_compare_exchange_weak_uint128(
        uint64_t *ptr,
        const uint64_t *old_value,
        const uint64_t *new_value) {
    if (ptr[0] != old_value[0] || ptr[1] != old_value[1]) {
        return false;  // this comparison is fast but not atomic, thus 'weak'
    } else {
        return raw_atomic_compare_exchange_strong_uint128(ptr, old_value, new_value);
    }
}
bool assorted::raw_atomic_compare_exchange_strong_uint128(
  uint64_t *ptr,
  const uint64_t *old_value,
  const uint64_t *new_value) {
  bool ret;
#if defined(__GNUC__) && defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_16)
  // gcc-x86 (-mcx16), then simply use __sync_bool_compare_and_swap.
  __uint128_t* ptr_casted = reinterpret_cast<__uint128_t*>(ptr);
  __uint128_t old_casted = *reinterpret_cast<const __uint128_t*>(old_value);
  __uint128_t new_casted = *reinterpret_cast<const __uint128_t*>(new_value);
  ret = ::__sync_bool_compare_and_swap(ptr_casted, old_casted, new_casted);
#elif defined(__GNUC__) && defined(__aarch64__)
  // gcc-AArch64 doesn't allow -mcx16. But, it supports __atomic_compare_exchange_16 with
  // libatomic.so. We need to link to it in that case.
  __uint128_t* ptr_casted = reinterpret_cast<__uint128_t*>(ptr);
  __uint128_t old_casted = *reinterpret_cast<const __uint128_t*>(old_value);
  __uint128_t new_casted = *reinterpret_cast<const __uint128_t*>(new_value);
  ret = ::__atomic_compare_exchange_16(
    ptr_casted,
    &old_casted,
    new_casted,
    false,              // strong CAS
    __ATOMIC_ACQ_REL,   // to make it atomic, of course acq_rel
    __ATOMIC_ACQUIRE);  // matters only when it fails. acquire is enough.
#else  // everything else
  // oh well, then resort to assembly, assuming x86. clang on ARMv8? oh please...
  // see: linux/arch/x86/include/asm/cmpxchg_64.h
  uint64_t junk;
  asm volatile("lock; cmpxchg16b %2;setz %1"
    : "=d"(junk), "=a"(ret), "+m" (*ptr)
    : "b"(new_value[0]), "c"(new_value[1]), "a"(old_value[0]), "d"(old_value[1]));
  // Note on roll-our-own non-gcc ARMv8 cas16. It's doable, but...
  // ARMv8 does have 128bit atomic instructions, called "pair" operations, such as ldaxp and stxp.
  // There is actually a library that uses it:
  // https://github.com/ivmai/libatomic_ops/blob/master/src/atomic_ops/sysdeps/gcc/aarch64.h
  // (but this is GPL. Don't open the URL unless you are ready for it.)
  // As of now (May 2014), GCC can't handle them, nor provide __uint128_t in ARMv8.
  // I think it's coming, however. I'm waiting for it... if it's not coming, let's do ourselves.
#endif
  return ret;
}

//}  // namespace assorted
}  // namespace mvstore

