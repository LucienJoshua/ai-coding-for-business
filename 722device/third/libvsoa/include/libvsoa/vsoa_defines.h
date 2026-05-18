/*
 * Copyright (c) 2022 ACOAUTO Team.
 * All rights reserved.
 *
 * Detailed license information can be found in the LICENSE file.
 *
 * File: vsoa_defines.h Vehicle SOA recommended Definition.
 *
 * Author: Han.hui <hanhui@acoinfo.com>
 *
 */

#ifndef VSOA_DEFINES_H
#define VSOA_DEFINES_H

/*
 * Standard URL path prefix:
 *
 * "/api/*"     Server RPC commands
 * "/par/*"     Server each component parameters geting, setting and publish
 * "/sta/*"     Server status get and publish
 * "/dbg/*"     Server debug message
 * "/res/*"     Server resources
 */
#define VSOA_PATH_API       "/api"
#define VSOA_PATH_PARAM     "/par"
#define VSOA_PATH_STATE     "/sta"
#define VSOA_PATH_DEBUG     "/dbg"
#define VSOA_PATH_RESRC     "/res"

#endif /* VSOA_DEFINES_H */
/*
 * end
 */
