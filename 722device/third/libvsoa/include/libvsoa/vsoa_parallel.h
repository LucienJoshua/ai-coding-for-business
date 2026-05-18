/*
 * Copyright (c) 2022 ACOAUTO Team.
 * All rights reserved.
 *
 * Detailed license information can be found in the LICENSE file.
 *
 * File: vsoa_parallel.h Vehicle SOA high performance server parallel sender.
 *
 * Author: Han.hui <hanhui@acoinfo.com>
 *
 */

#ifndef VSOA_PARALLEL_H
#define VSOA_PARALLEL_H

/* This module is an internal module of VSOA */

#include "vsoa_pmem.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Structure declaration */
struct vsoa_parallel;

/* VSOA parallel sender */
typedef struct vsoa_parallel   vsoa_parallel_t;

/* VSOA parallel sender create */
vsoa_parallel_t *vsoa_parallel_create(int cnt, vsoa_pmem_t *pmem);

/* VSOA parallel sender delete */
void vsoa_parallel_delete(vsoa_parallel_t *parallel);

/* VSOA parallel sender delete the packet to be sent with the specified socket */
void vsoa_parallel_discard(vsoa_parallel_t *parallel, int idx, int sock);

/* VSOA parallel sender write */
void vsoa_parallel_write(vsoa_parallel_t *parallel, int idx, vsoa_pbuf_t *pbuf);

/* VSOA parallel sender count */
int vsoa_parallel_count(vsoa_parallel_t *parallel);

#ifdef __cplusplus
}
#endif

#endif /* VSOA_PARALLEL_H */
/*
 * end
 */
