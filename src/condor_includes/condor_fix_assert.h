/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#ifndef __CONDOR_FIX_ASSERT_H
#define __CONDOR_FIX_ASSERT_H

#ifndef WIN32	/* on Win32, we do EXCEPT instead of assert */
#include <assert.h>
#else
#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "condor_header_features.h"
#include "condor_system.h"
#include "condor_debug.h"
#endif	/* of else ifndef WIN32 */

#endif /* CONDOR_FIX_ASSERT_H */


