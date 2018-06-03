/* Jazz (c) 2018 kaalam.ai (The Authors of Jazz), using (under the same license):

   1. Biomodelling - The AATBlockQueue class (c) Jacques Basaldúa, 2009-2012 licensed
	  exclusively for the use in the Jazz server software.

	  Copyright 2009-2012 Jacques Basaldúa

   2. BBVA - Jazz: A lightweight analytical web server for data-driven applications.

		Copyright 2016-2017 Banco Bilbao Vizcaya Argentaria, S.A.

	  This product includes software developed at

	   BBVA (https://www.bbva.com/)

   Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

   3. LMDB: Copyright 2011-2017 Howard Chu, Symas Corp. All rights reserved.

   Licensed under http://www.OpenLDAP.org/license.html

   Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

/* NOTES from the Jazz draft description including tasks for this
   --------------------------------------------------------------

In Jazz an lvalue is one of:

A chain of keeprs abstracted as block ids starting from root ending with the name of a block. They must all exist except, possibly, the last name. If the last name is new, it is created, if it exists, overridden.
A data block that will be returned as a Python object, R object or http (GET) resource.
In Jazz an rvalue is one of:

A block constructor. A constant expression that can be used to build a block from it.
Chains of keeprs abstracted as existing blocks starting from root. This includes functions and blocks passed as arguments to functions.
A combination of the previous two.
A data block that will be passed as a Python object, R object or http (PUT) resource.
A delete predicate. This deletes the corresponding lvalue.
Since the API has to be REST compatible and is intended for using over a network.

All rvalue evaluations are safe. They cannot have side effects. Function calls cannot have side effects.
All lvalue assignments are idempotent. Assigning twice has the same effect than assigning once. There is no += operator.

GET with a valid rvalue. To read from Jazz.
HEAD with a valid rvalue. Internally the same as GET, but returns the header only.
PUT with a valid lvalue. To write blocks into a Jazz keepr.
DELETE with a valid lvalue. To delete blocks or keeprs (even recursively).
OPTIONS with a string. Parses the string and returns the commands that would accept that string as a URL.
GET with lvalue=rvalue. Assignment in the server. Similar to “PUT(lvalue, GET(rvalue))” without traffic.

//TODO: Consider these statements when implementing jazz_api
//TODO: Create explanation on jazz_api for the doxy page and linked from the reference

*/

#include "src/include/jazz.h"
#include "src/jazz_functional/jazz_bebop.h"


#ifndef INCLUDED_JAZZ_MAIN_API
#define INCLUDED_JAZZ_MAIN_API


/**< \brief TODO

//TODO: Write module description for jazz_api when implemented.
*/
namespace jazz_api
{

//TODO: Document interface for module jazz_api.

//TODO: Implement interface for module jazz_api.

}

#endif
