/*
 * Copyright (c) 2022 ACOAUTO Team.
 * All rights reserved.
 *
 * Detailed license information can be found in the LICENSE file.
 *
 * File: vsoa_pmem.h Vehicle SOA packet memory for high performance server.
 *
 * Author: Han.hui <hanhui@acoinfo.com>
 *
 */

#ifndef VSOA_PMEM_H
#define VSOA_PMEM_H

/* This module is an internal module of VSOA */

#include <stddef.h>
#include "vsoa_parser.h"

/* VSOA minimum packet memory pool */
#define VSOA_PMEM_MIN_SIZE  (VSOA_MAX_PACKET_LENGTH + 512 + sizeof(vsoa_pbuf_t))

#ifdef __cplusplus
extern "C" {
#endif

/* Structure declaration */
struct vsoa_pmem;

/* VSOA packet memory pool */
typedef struct vsoa_pmem   vsoa_pmem_t;

/* VSOA packet buffer */
typedef struct vsoa_pbuf {
    struct vsoa_pbuf *next;
    struct vsoa_pbuf *prev;
    int sock;
    int ref;
    size_t length;
    void *payload;
} vsoa_pbuf_t;

/* VSOA packet memory pool create */
vsoa_pmem_t *vsoa_pmem_create(size_t size);

/* VSOA packet memory pool delete */
void vsoa_pmem_delete(vsoa_pmem_t *pmem);

/* VSOA packet memory pool allocate */
vsoa_pbuf_t *vsoa_pmem_alloc(vsoa_pmem_t *pmem, size_t length);

/* VSOA packet payload reference */
vsoa_pbuf_t *vsoa_pmem_ref(vsoa_pmem_t *pmem, vsoa_pbuf_t *pbuf);

/* VSOA packet memory pool free */
void vsoa_pmem_free(vsoa_pmem_t *pmem, vsoa_pbuf_t *pbuf);

/* VSOA packet memory payload */
#define vsoa_pmem_payload(pbuf, type)  ((type *)(pbuf)->payload)

#ifdef __cplusplus
}
#endif

#endif /* VSOA_PMEM_H */
/*
 * end
 */
