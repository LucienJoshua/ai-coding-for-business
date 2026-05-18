/*
 * Copyright (c) 2021 ACOAUTO Team.
 * All rights reserved.
 *
 * Detailed license information can be found in the LICENSE file.
 *
 * File: vsoa_list.h Vehicle SOA list.
 *
 * Author: Han.hui <hanhui@acoinfo.com>
 *
 */

#ifndef VSOA_LIST_H
#define VSOA_LIST_H

/* List initialize */
#define INIT_LIST(node) \
        (node)->next = (node)->prev = NULL;

/* List add / delete */
#define INSERT_TO_HEADER(node, header) \
        do { \
            (node)->next = (header); \
            (node)->prev = NULL; \
            if (header) { \
                (header)->prev = (node); \
            } \
            (header) = (node); \
        } while (0)

#define DELETE_FROM_LIST(node, header) \
        do { \
            if ((node)->next) { \
                (node)->next->prev = (node)->prev; \
            } \
            if ((node)->prev) { \
                (node)->prev->next = (node)->next; \
            } else { \
                (header) = (node)->next; \
            } \
        } while (0)

/* List add / delete with void header */
#define INSERT_TO_HEADER_VOID(node, header, type) \
        do { \
            (node)->next = (type *)(header); \
            (node)->prev = NULL; \
            if (header) { \
                ((type *)(header))->prev = (node); \
            } \
            (header) = (void *)(node); \
        } while (0)

#define DELETE_FROM_LIST_VOID(node, header) \
        do { \
            if ((node)->next) { \
                (node)->next->prev = (node)->prev; \
            } \
            if ((node)->prev) { \
                (node)->prev->next = (node)->next; \
            } else { \
                (header) = (void *)(node)->next; \
            } \
        } while (0)

/* List concat */
#define CONCAT_TO_HEADER(type, header_d, header_s) \
        do { \
            if (header_d) { \
                type *tail; \
                for (tail = (header_d); tail->next; tail = tail->next); \
                tail->next = (header_s); \
                if (header_s) { \
                    (header_s)->prev = (tail); \
                } \
            } else { \
                (header_d) = (header_s); \
            } \
        } while (0)

/* List walk */
#define FOREACH_FROM_LIST(node, header) \
        for ((node) = (header); (node) != NULL; (node) = (node)->next)

#define FOREACH_FROM_LIST_REVERSE(node, tail) \
        for ((node) = (tail); (node) != NULL; (node) = (node)->prev)

#define FOREACH_FROM_LIST_SAFE(node, n, header) \
        for ((node) = (header), (n) = (node) ? (node)->next : NULL; \
             (node) != NULL; (node) = (n), (n) = (n) ? (n)->next : NULL)

/* FIFO List operate */
#define INSERT_TO_FIFO(node, header, tail) \
        do { \
            (node)->next = NULL; \
            (node)->prev = tail; \
            if (tail) { \
                (tail)->next = (node); \
            } \
            (tail) = (node); \
            if (!(header)) { \
                (header) = (node); \
            } \
        } while (0)

#define DELETE_FROM_FIFO(node, header, tail) \
        do { \
            if ((node)->next) { \
                (node)->next->prev = (node)->prev; \
            } else { \
                (tail) = (node)->prev; \
            } \
            if ((node)->prev) { \
                (node)->prev->next = (node)->next; \
            } else { \
                (header) = (node)->next; \
            } \
        } while (0)

/* Insert left and right */
#define INSERT_TO_LEFT(node, right, header) \
        do { \
            (node)->next = (right); \
            (node)->prev = (right)->prev; \
            if ((right)->prev) { \
                (right)->prev->next = (node); \
            } \
            (right)->prev = (node); \
            if ((header) == (right)) { \
                (header) = (node); \
            } \
        } while (0)

#define INSERT_TO_RIGHT(node, left, tail) \
        do { \
            (node)->prev = (left); \
            (node)->next = (left)->next; \
            if ((left)->next) { \
                (left)->next->prev = (node); \
            } \
            (left)->next = (node); \
            if ((tail) == (left)) { \
                (tail) = (node); \
            } \
        } while (0)

#endif /* VSOA_LIST_H */
/*
 * end
 */
