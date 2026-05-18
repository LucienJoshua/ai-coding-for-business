/*
 * Copyright (c) 2024 ACOAUTO Team.
 * All rights reserved.
 *
 * Detailed license information can be found in the LICENSE file.
 *
 * File: vsoa_clisuber.h Vehicle SOA client subscriber.
 *
 * Author: Han.hui <hanhui@acoinfo.com>
 *
 */

#ifndef VSOA_CLISUBER_H
#define VSOA_CLISUBER_H

#include <stdbool.h>
#include "vsoa_parser.h"
#include "vsoa_client.h"

/*
 * This module provides the function of synchronously obtaining subscription messages.
 * The user's main loop only needs to call the UPDATE functions at the right time
 * to obtain the latest subscription data, which greatly simplifies the synchronization
 * logic of obtaining subscription messages and the main business loop.
 */

/* Structure declaration */
struct vsoa_client_suber;

/* Client message type */
typedef struct vsoa_client_suber vsoa_client_suber_t;

/* Client on message analysis deserializer callback */
typedef bool (*vsoa_client_suber_deserializer_t)(void *arg, vsoa_client_suber_t *clisuber,
                                                 const vsoa_url_t *url, const vsoa_payload_t *payload);

/* Create client subscriber */
vsoa_client_suber_t *vsoa_client_suber_create(void);

/* Delete client subscriber
 * This subscriber can be deleted only when it is no longer used. */
void vsoa_client_suber_delete(vsoa_client_suber_t *clisuber);

/* Get client subscriber on message callback
 * Set this function as the client on message callback,
 * or call this function in the client on message callback,
 * the `arg` parameter is the corresponding `vsoa_client_suber_t` type instance. */
void vsoa_client_suber_onmsg(void *arg, vsoa_client_t *client, vsoa_url_t *url, vsoa_payload_t *payload, bool quick);

/* Check if the specified url message is valid.
 * If valid, return `true` and get the message buffer size. */
bool vsoa_client_suber_msg_is_valid(vsoa_client_suber_t *clisuber, const vsoa_url_t *url, size_t *buf_size);

/* Get valid messages. The buffer size must be >= the actual message size.
 * After getting the message, the corresponding URL will no longer be valid until you subscribe to a new message again. */
bool vsoa_client_suber_update(vsoa_client_suber_t *clisuber, const vsoa_url_t *url, vsoa_payload_t *payload, void *buffer, size_t buf_size);

/* Same as `vsoa_client_suber_update` and add automatic deserializer.
 * Deserializer failed is also considered as the message has been consumed. */
bool vsoa_client_suber_update_with_deserializer(vsoa_client_suber_t *clisuber, const vsoa_url_t *url,
                                                vsoa_client_suber_deserializer_t deserializer, void *arg);

/* Same as `vsoa_client_suber_update` and
 * Directly use the `payload.data` field for binary copying */
bool vsoa_client_suber_update_binary(vsoa_client_suber_t *clisuber, const vsoa_url_t *url, void *data, size_t data_len);

/* Set client subscriber message interval
 * The minimum interval for receiving messages. Within this interval,
 * even if a message is received, `vsoa_client_suber_has_updated` will return `false`. */
bool vsoa_client_suber_set_interval(vsoa_client_suber_t *clisuber, const vsoa_url_t *url, int64_t interval);

#endif /* VSOA_CLISUBER_H */
/*
 * end
 */
