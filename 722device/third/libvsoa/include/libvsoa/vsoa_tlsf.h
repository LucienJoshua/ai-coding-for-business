/*
 * Copyright (c) 2022 ACOAUTO Team.
 * All rights reserved.
 *
 * Detailed license information can be found in the LICENSE file.
 * This module is from https://github.com/mattconte/tlsf License: BSD
 *
 * File: vsoa_tlsf.h Vehicle SOA memory management for high performance server.
 *
 * Author: Han.hui <hanhui@acoinfo.com>
 *
 */

#ifndef VSOA_TLSF_H
#define VSOA_TLSF_H

/* This module is an internal module of VSOA */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 32-bit system malloc guarantees 32-bit alignment,
 * 64-bit system malloc guarantees 64-bit alignment,
 * so under 32-bit system, the memory allocated by malloc
 * is not allowed to use 64-bit aligned variables! */

/* Pool handle */
typedef void *tlsf_t;

/* Create a memory pool */
tlsf_t tlsf_create_with_pool(void *mem, size_t bytes);

/* Destroy a memory pool */
void tlsf_destroy(tlsf_t tlsf);

/* Allocate memory from pool */
void *tlsf_malloc(tlsf_t tlsf, size_t bytes);

/* Allocate memory from pool with specify alignment */
void *tlsf_memalign(tlsf_t tlsf, size_t align, size_t bytes);

/* Free memory to pool */
void tlsf_free(tlsf_t tlsf, void *ptr);

#ifdef __cplusplus
}
#endif

#endif /* VSOA_TLSF_H */
/*
 * end
 */
