/*
 * Copyright (c) 2023 ACOAUTO Team.
 * All rights reserved.
 *
 * Detailed license information can be found in the LICENSE file.
 *
 * File: vsoa_regulator.h Vehicle SOA speed regulator.
 *
 * Author: Han.hui <hanhui@acoinfo.com>
 *
 */

#ifndef VSOA_REGULATOR_H
#define VSOA_REGULATOR_H

#include <time.h>
#include "vsoa_parser.h"

/* The VSOA speed regulator can be used on both the server and the client. 
 * When used on the server: it can flexibly control the publish frequency of the specified URL. 
 * When used on the client: it can also flexibly handle the frequency of subscription messages. */

#ifdef __cplusplus
extern "C" {
#endif

/* Structure declaration */
struct vsoa_regulator;

/* Speed regulator type */
typedef struct vsoa_regulator vsoa_regulator_t;

/* Speed regulator notify function type
 * This callback function is executed in the vsoa timer thread context.
 * NOTICE: In order to ensure data consistency, this function is mutually exclusive with
 *         `vsoa_regulator_update()` and `vsoa_regulator_clear()` (executed in the regulator mutex lock state).
 *         This function needs to ensure fast data processing and return as soon as possible. */
typedef void (*vsoa_regulator_func_t)(void *arg, vsoa_url_t *url, vsoa_payload_t *payload);

/* Create a speed regulator, period cannot be less than 1ms */
vsoa_regulator_t *vsoa_regulator_create(const struct timespec *period);

/* Delete speed regulator 
 * This function will delete all previously added slots and uncommitted data */
void vsoa_regulator_delete(vsoa_regulator_t *regulator);

/* Change regulator period, period cannot be less than 1ms */
bool vsoa_regulator_period(vsoa_regulator_t *regulator, const struct timespec *period);

/* Add a slot with `url` as key
 * The `buf_size` is the maximum payload length that this slot can saved. */
bool vsoa_regulator_slot(vsoa_regulator_t *regulator, const vsoa_url_t *url, vsoa_regulator_func_t ondelay, void *arg, size_t buf_size);

/* Delete the slot specified by `url`
 * This function will delete all uncommitted data */
bool vsoa_regulator_unslot(vsoa_regulator_t *regulator, const vsoa_url_t *url);

/* Whether it has the slot specified by `url` */
bool vsoa_regulator_has_slot(vsoa_regulator_t *regulator, const vsoa_url_t *url, size_t *buf_size);

/* Submit data to the slot specified by `url`.
 * The `payload` size cannot exceed the size when the slot is created. */
bool vsoa_regulator_update(vsoa_regulator_t *regulator, const vsoa_url_t *url, const vsoa_payload_t *payload);

/* Clear buffered data specified by `url` */
bool vsoa_regulator_clear(vsoa_regulator_t *regulator, const vsoa_url_t *url);

#ifdef __cplusplus
}
#endif

#endif /* VSOA_REGULATOR_H */
/*
 * end
 */
