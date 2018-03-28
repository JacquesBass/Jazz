/* Jazz (c) 2018 kaalam.ai (The Authors of Jazz), using (under the same license):

   BBVA - Jazz: A lightweight analytical web server for data-driven applications.

   Copyright 2016-2017 Banco Bilbao Vizcaya Argentaria, S.A.

  This product includes software developed at

   BBVA (https://www.bbva.com/)

   Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/


/**< \brief Allocation functions for Jazz.

	This module defines functions to explicitely allocate RAM for JazzDataBlock structures. The module is
functional and understands ownership of pointers between persisted, volatile or one-shot JazzDataBlock
structures. JazzDataObject descendants are memory-wise just JazzDataBlock structures belonging to a class,
so their allocation will also be handled by this module and not via "fancy" C++ object allocation. Unlike
in Jazz 0.1.+, there is no support for embedded R (or any other interpreters).
*/


#if defined CATCH_TEST
#ifndef INCLUDED_JAZZ_CATCH2
#define INCLUDED_JAZZ_CATCH2

#include "src/catch2/catch.hpp"

#endif
#endif


#ifndef INCLUDED_JAZZ_ELEMENTS_ALLOC
#define INCLUDED_JAZZ_ELEMENTS_ALLOC

namespace jazz_alloc
{

}

#endif
