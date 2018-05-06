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

   Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/


#include <math.h>


#include "src/jazz_elements/jazz_containers.h"


namespace jazz_containers
{

#ifdef DEBUG
long long num_alloc_ok = 0, num_alloc_failed = 0, num_free = 0, num_realloc_ok = 0, num_realloc_failed = 0;
#endif


/** Create a new (one_shot) JazzBlock as a selection (of possibly all) of an existing JazzBlock

	\param p_as_block	An existing block from which everything is copied, possibly with a selection over its rows.
	\param p_row_filter A filter that is applicable to p_as_block. I.e., p_row_filter->can_filter(p_as_block) == true
						If p_row_filter == nullptr then p_as_block is copied into a newly allocated pointer.
						See parameter dim in the new_jazz_block() version that uses dim to understand how selection is applied.
	\param att			An alternative source of attributes. When this parameter in != nullptr, the new block will get its
						attributes from att instead of copying those in p_as_block->.

	OWNERSHIP: If you create a one shot block using new_jazz_block(), you earn the responsibility to free it with free_jazz_block().
	This is not the normal way to create JazzBlocks, when you use a JazzBlockKeepr descendant, that object will allocate and free
	the JazzBlocks automatically. The same applies to JazzBlocks created in the stack of a bebop program which are also managed
	automatically.

	\return	The new JazzBlock or nullptr if failed.
*/
pJazzBlock new_jazz_block (pJazzBlock	  p_as_block,
						   pJazzFilter	  p_row_filter,
						   AllAttributes *att)
{
	if (p_as_block == nullptr || p_as_block->size < 0 || p_as_block->range.dim[0] < 1)
		return nullptr;

	int tensor_diff		= 0,
		old_tensor_size = p_as_block->size*(p_as_block->cell_type & 0xff),
		new_tensor_size,
		bytes_per_row,
		selected_rows;

	if (p_row_filter != nullptr) {
		int	tensor_rows = p_as_block->size/p_as_block->range.dim[0];

		if (!p_row_filter->can_filter(p_as_block))
			return nullptr;

		if (p_row_filter->cell_type == CELL_TYPE_BYTE_BOOLEAN) {
			selected_rows = 0;
			for (int i = 0; i < p_row_filter->size; i++)
				if (p_row_filter->tensor.cell_bool[i])
					selected_rows++;
		} else {
			selected_rows = p_row_filter->range.filter.length;
		}
		if (p_as_block->size) {
			bytes_per_row	= old_tensor_size/tensor_rows;
			new_tensor_size = selected_rows*bytes_per_row;

			old_tensor_size = (uintptr_t) p_as_block->align_128bit(old_tensor_size);
			new_tensor_size = (uintptr_t) p_as_block->align_128bit(new_tensor_size);

			tensor_diff = new_tensor_size - old_tensor_size;
		}
	}

	int attrib_diff		   = 0,
		new_num_attributes = 0;

	if (att	!= nullptr) {
		int new_attrib_bytes = 0;

		for (AllAttributes::iterator it = att->begin(); it != att->end(); ++it) {
			int len = it->second == nullptr ? 0 : strlen(it->second);
			if (len)
				new_attrib_bytes += len + 1;
			new_num_attributes++;
		}

		if (!new_num_attributes)
			return nullptr;

		new_attrib_bytes += new_num_attributes*2*sizeof(int);

		int old_attrib_bytes = p_as_block->num_attributes*2*sizeof(int);

		if (p_as_block->cell_type != CELL_TYPE_JAZZ_STRING) {
			pJazzStringBuffer psb = p_as_block->p_string_buffer();
			old_attrib_bytes += std::max(0, psb->buffer_size - 4);
		}

		attrib_diff = new_attrib_bytes - old_attrib_bytes;
	}

	int total_bytes = p_as_block->total_bytes + tensor_diff + attrib_diff;

	pJazzBlock pjb = (pJazzBlock) malloc(total_bytes);

	if (pjb == nullptr) {
#ifdef DEBUG
		num_alloc_failed++;
#endif
		return nullptr;
	}

#ifdef DEBUG
	num_alloc_ok++;
#endif

	memcpy(pjb, p_as_block, sizeof(JazzBlockHeader));

	pjb->total_bytes = total_bytes;

	if (tensor_diff) {
		pjb->size = selected_rows*p_as_block->range.dim[0];

		u_char *p_dest = &pjb->tensor.cell_byte[0],
			   *p_src  = &p_as_block->tensor.cell_byte[0];

		if (p_row_filter->cell_type == CELL_TYPE_BYTE_BOOLEAN) {
			for (int i = 0; i < p_row_filter->size; i++) {
				if (p_row_filter->tensor.cell_bool[i]) {
					memcpy(p_dest, p_src, bytes_per_row);
					p_dest = p_dest + bytes_per_row;
				}
				p_src = p_src + bytes_per_row;
			}
		} else {
			for (int i = 0; i < p_row_filter->range.filter.length; i++) {
				memcpy(p_dest, p_src + p_row_filter->tensor.cell_int[i]*bytes_per_row, bytes_per_row);
				p_dest = p_dest + bytes_per_row;
			}
		}
	} else {
		memcpy(&pjb->tensor, &p_as_block->tensor, old_tensor_size);
	}

	if (att	!= nullptr)	{
		if (p_as_block->cell_type != CELL_TYPE_JAZZ_STRING) {
			pjb->num_attributes = 0;
			pjb->set_attributes(att);
		} else {
			pjb->num_attributes = new_num_attributes;

			pJazzStringBuffer p_nsb = pjb->p_string_buffer(), p_osb = p_as_block->p_string_buffer();

			pjb->init_string_buffer();

			memcpy(&p_nsb->buffer, &p_osb->buffer, p_osb->buffer_size);

			p_nsb->last_idx = p_osb->last_idx;

			int i = 0;
			int *ptk = pjb->p_attribute_keys();

			for (AllAttributes::iterator it = att->begin(); it != att->end(); ++it) {
				ptk[i] = it->first;
				ptk[i + new_num_attributes] = pjb->get_string_offset(p_nsb, it->second);

				i++;
			}
		}
	} else {
		memcpy(pjb->p_attribute_keys(), p_as_block->p_attribute_keys(), p_as_block->num_attributes*2*sizeof(int));

		pJazzStringBuffer p_nsb = pjb->p_string_buffer(), p_osb = p_as_block->p_string_buffer();

		memcpy(p_nsb, p_osb, p_osb->buffer_size + sizeof(JazzStringBuffer));
	}

	return pjb;
}


/** Create a new (one_shot) JazzBlock (including a JazzFilter) from scratch

	\param cell_type		The type for the tensor's cell types in [CELL_TYPE_BYTE..CELL_TYPE_DOUBLE]
	\param dim				This defines both the rank and the dimensions of the tensor. Note that, except for the first position a
							dimension of 0 and 1 is the same dim = {3, 1} is a vector of 3 elements with rank 1, exactly like {3, 0}.
							As a matter of convention, dim should always end with a 0 except when it is JAZZ_MAX_TENSOR_RANK long.
							For the first dimension 1 means one element and 0 means no element. Both have rank 1. The latter is the
							typical result of a selection where no row matches the condition. Blocks never have rank == 0 and zero-element
							blocks have the same rank as the block from which they were selected. When 0 rows are selected from a block
							of dim = {r, s, t} the resulting block is {0, s, t} with size == 0 and rank == 3.
							If dim == nullptr and p_text != nullptr, dim will be set automatically to the number of lines (see eoln) in p_text
							when cell_type == CELL_TYPE_JAZZ_STRING.
	\param att				The attributes to set when creating the block. They are be immutable. To change the attributes of a JazzBlock
							use the version of new_jazz_block() with parameter p_as_block.
	\param fill_tensor		How to fill the tensor. When creating anything that is not a JazzFilter, p_bool_filter is ignored and the options
							are: JAZZ_FILL_NEW_DONT_FILL (don't do anything with the tensor), JAZZ_FILL_NEW_WITH_ZERO (fill with binary zero
							no matter what the cell_type is), JAZZ_FILL_NEW_WITH_NA fill with the appropriate NA for the cell_type)
							When creating a filter, p_bool_filter must be a vector of length == size and the filter will be created as
							boolean (when fill_tensor == JAZZ_FILL_BOOLEAN_FILTER) or integer (when fill_tensor == JAZZ_FILL_INTEGER_FILTER)
	\param p_bool_filter	The vector of boolean (each true value means the corresponding row is selected) used when fill_tensor ==
							JAZZ_FILL_BOOLEAN_FILTER and fill_tensor == JAZZ_FILL_INTEGER_FILTER
	\param stringbuff_size	One of the possible ways to allocate space for strings is declaring this size. When this is non-zero a buffer
							will be allocated with this size plus whatever size is required by the strings in att. new_jazz_block() will
							only allocate the space and do nothing with it. The caller should assign strings to cells with JazzBlock.set_string().
	\param p_text			The other possible way to allocate space for strings is by declaring p_text. Imagine the content of p_text
							as a text file with n = size rows that will be pushed into the tensor and the string buffer. The eoln character
							separates the cells. (cell_type == CELL_TYPE_JAZZ_STRING & p_text != nullptr) overrides any setting in fill_tensor.
							Also, either dim should be nullptr and set automatically or its resulting size must be the same as the number of
							lines in p_text.
	\param eoln				A single character that separates the cells in p_text and will not be pushed to the string buffer.

	NOTES: String buffer allocation should not be used to dynamically change attribute values. Attributes are immutable and should be changed
	only creating a new block with new = new_jazz_block(p_as_block = old, att = new_att).
	String buffer allocation should only be used for cell_type == CELL_TYPE_JAZZ_STRING and either with stringbuff_size or with p_text (and eoln).
	If stringbuff_size is used, JazzBlock.set_string() should be used afterwards. If p_text is used, the tensor is already filled and
	JazzBlock.set_string() **should not** be called after that.

	OWNERSHIP: If you create a one shot block using new_jazz_block(), you earn the responsibility to free it with free_jazz_block().
	This is not the normal way to create JazzBlocks, when you use a JazzBlockKeepr descendant, that object will allocate and free
	the JazzBlocks automatically. The same applies to JazzBlocks created in the stack of a bebop program which are also managed
	automatically.

	\return	The new JazzBlock or nullptr if failed.
*/
pJazzBlock new_jazz_block (int			  cell_type,
						   int			 *dim,
						   AllAttributes *att,
						   int			  fill_tensor,
						   bool			 *p_bool_filter,
						   int			  stringbuff_size,
						   const char	 *p_text,
						   char			  eoln)
{
	JazzBlockHeader hea;

	hea.cell_type = cell_type;

	int text_length = 0, num_lines = 0;

	if (p_text != nullptr) {
		if (cell_type != CELL_TYPE_JAZZ_STRING || fill_tensor != JAZZ_FILL_WITH_TEXTFILE)
			return nullptr;

		const char *pt = p_text;
		while (pt[0]) {
			if (pt[0] == eoln) {
				if (!pt[1])
					break;
				num_lines++;
			}
			pt++;
		}
		text_length = (uintptr_t) pt - (uintptr_t) p_text - num_lines;
		num_lines++;
	}

	if (dim == nullptr) {
		if (p_text == nullptr)
			return nullptr;

		JazzTensorDim idim;
		idim.dim[0] = num_lines;
		idim.dim[1] = 0;
#ifdef DEBUG				// Initialize idim for Valgrind.
		idim.dim[2] = 0;
		idim.dim[3] = 0;
		idim.dim[4] = 0;
		idim.dim[5] = 0;
#endif
		reinterpret_cast<pJazzBlock>(&hea)->set_dimensions(idim.dim);
	} else {
		reinterpret_cast<pJazzBlock>(&hea)->set_dimensions(dim);

		if (num_lines && (num_lines != hea.size))
			return nullptr;
	}

	hea.num_attributes = 0;

	hea.total_bytes = (uintptr_t) reinterpret_cast<pJazzBlock>(&hea)->p_string_buffer() - (uintptr_t) (&hea) + sizeof(JazzStringBuffer) + 4;

	if (att	== nullptr) {
		hea.total_bytes += 2*sizeof(int);
		hea.num_attributes++;
	} else {
		for (AllAttributes::iterator it = att->begin(); it != att->end(); ++it) {
			int len = it->second == nullptr ? 0 : strlen(it->second);
			if (len) hea.total_bytes += len + 1;
			hea.num_attributes++;
		}
		hea.total_bytes += 2*hea.num_attributes*sizeof(int);
	}

	hea.total_bytes += stringbuff_size + text_length + num_lines;

	pJazzBlock pjb = (pJazzBlock) malloc(hea.total_bytes);

	if (pjb == nullptr) {
#ifdef DEBUG
		num_alloc_failed++;
#endif
		return nullptr;
	}

#ifdef DEBUG
	num_alloc_ok++;
#endif

	memcpy(pjb, &hea, sizeof(JazzBlockHeader));

	pjb->num_attributes = 0;

	if (att	== nullptr) {
		AllAttributes void_att;
		void_att [BLOCK_ATTR_CONTAINERS_EMPTY] = nullptr;
		pjb->set_attributes(&void_att);
	} else {
		pjb->set_attributes(att);
	}

#ifdef DEBUG	// Initialize the RAM between the end of the tensor and the base of the attribute key vector for Valgrind.
	{
		char *pt1 = (char *) &pjb->tensor + (pjb->cell_type & 0xf)*pjb->size,
			 *pt2 = (char *) pjb->align_128bit((uintptr_t) pt1);

		while (pt1 < pt2) {
			pt1[0] = 0;
			pt1++;
		}
	}
#endif

	if (p_text != nullptr) {
		pjb->has_NA = false;
		pJazzStringBuffer psb = pjb->p_string_buffer();

		int offset = psb->last_idx;

		char *pt_out = &psb->buffer[offset];

		int row = 1, len = 0;
		const char *pt_in = p_text;

		pjb->tensor.cell_int[0] = offset;

		while (pt_in[0]) {
			offset++;
			if (pt_in[0] != eoln) {
				pt_out[0] = pt_in[0];
				len++;
			} else {
				if (!len)
					pjb->tensor.cell_int[row - 1] = JAZZ_STRING_EMPTY;

				if (!pt_in[1])
					break;

				pt_out[0] = 0;

				pjb->tensor.cell_int[row] = offset;

				len = 0;
				row++;
			}
			pt_out++;
			pt_in++;
		}
		if (!len)
			pjb->tensor.cell_int[row - 1] = JAZZ_STRING_EMPTY;

		pt_out[0] = 0;

		psb->last_idx			= offset + (pt_in[0] == 0);
		psb->stop_check_4_match = true;						// JazzBlock::get_string_offset() does not support match with (possibly) empty strings.
	} else {
		switch (fill_tensor) {
		case JAZZ_FILL_NEW_DONT_FILL:
			pjb->has_NA = pjb->cell_type != CELL_TYPE_BYTE;
			break;

		case JAZZ_FILL_NEW_WITH_ZERO:
			memset(&pjb->tensor, 0, (pjb->cell_type & 0xf)*pjb->size);
			pjb->has_NA = (pjb->cell_type == CELL_TYPE_JAZZ_STRING) || (pjb->cell_type == CELL_TYPE_JAZZ_TIME);
			break;

		case JAZZ_FILL_NEW_WITH_NA:
			pjb->has_NA = true;

			switch (cell_type) {
			case CELL_TYPE_BYTE_BOOLEAN:
				for (int i = 0; i < pjb->size; i++) pjb->tensor.cell_byte[i] = JAZZ_BOOLEAN_NA;
				break;

			case CELL_TYPE_INTEGER:
			case CELL_TYPE_FACTOR:
			case CELL_TYPE_GRADE:
				for (int i = 0; i < pjb->size; i++) pjb->tensor.cell_int[i] = JAZZ_INTEGER_NA;
				break;

			case CELL_TYPE_BOOLEAN:
				for (int i = 0; i < pjb->size; i++) pjb->tensor.cell_uint[i] = JAZZ_BOOLEAN_NA;
				break;

			case CELL_TYPE_SINGLE: {
				u_int una = reinterpret_cast<u_int*>(&JAZZ_SINGLE_NA)[0];

				for (int i = 0; i < pjb->size; i++) pjb->tensor.cell_uint[i] = una;
				break; }

			case CELL_TYPE_JAZZ_STRING:
				for (int i = 0; i < pjb->size; i++) pjb->tensor.cell_int[i] = JAZZ_STRING_NA;
				break;

			case CELL_TYPE_LONG_INTEGER:
				for (int i = 0; i < pjb->size; i++) pjb->tensor.cell_longint[i] = JAZZ_LONG_INTEGER_NA;
				break;

			case CELL_TYPE_JAZZ_TIME:
				for (int i = 0; i < pjb->size; i++) pjb->tensor.cell_longint[i] = JAZZ_TIME_POINT_NA;
				break;

			case CELL_TYPE_DOUBLE: {
				uint64_t una = reinterpret_cast<uint64_t*>(&JAZZ_DOUBLE_NA)[0];

				for (int i = 0; i < pjb->size; i++) pjb->tensor.cell_ulongint[i] = una;
				break; }

			default:
				free_jazz_block(pjb);
				return nullptr;		// No silent fail, JAZZ_FILL_NEW_WITH_NA is undefined for the type
			}
			break;

		case JAZZ_FILL_BOOLEAN_FILTER:
			pjb->has_NA = false;
			if (p_bool_filter == nullptr || reinterpret_cast<pJazzFilter>(pjb)->filter_type() != JAZZ_FILTER_TYPE_BOOLEAN) {
				free_jazz_block(pjb);
				return nullptr;		// No silent fail, cell_type and rank must match
			}
			memcpy(&pjb->tensor, p_bool_filter, pjb->size);
			break;

		case JAZZ_FILL_INTEGER_FILTER: {
			pjb->has_NA = false;
			if (p_bool_filter == nullptr || reinterpret_cast<pJazzFilter>(pjb)->filter_type() != JAZZ_FILTER_TYPE_INTEGER) {
				free_jazz_block(pjb);
				return nullptr;		// No silent fail, cell_type and rank must match
			}
			int j = 0;
			for (int i = 0; i < pjb->size; i ++) {
				if (p_bool_filter[i]) {
					pjb->tensor.cell_int[j] = i;
					j++;
				}
			}
			pjb->range.filter.length = j;

#ifdef DEBUG						// Initialize the RAM on top of the filter for Valgrind.
			for (int i = pjb->range.filter.length; i < pjb->size; i ++)
				pjb->tensor.cell_int[i] = 0;
#endif
			break; }

		default:
			free_jazz_block(pjb);
			return nullptr;		// No silent fail, fill_tensor is invalid
		}
	}
	return pjb;
}


/** Free a (one_shot) JazzBlock created with new_jazz_block()

	\param p_block The pointer to the block you previously created with new_jazz_block().

	OWNERSHIP: Never free a pointer you have not created with new_jazz_block(). The normal way to create JazzBlocks is using a JazzBlockKeepr
	descendant, that object will allocate and free	the JazzBlocks automatically. Also in the stack of a bebop program JazzBlocks are also
	managed automatically.
*/
void free_jazz_block(pJazzBlock &p_block)
{
#ifdef DEBUG
	num_free++;
#endif
	free(p_block);

	p_block = nullptr;
}


/** Constructor for class JazzBlockKeepr

	\param a_logger A running JazzLogger object that will be used to track all LOG_MISS, LOG_WARN and LOG_ERROR events if available.
It is safe to ignore this parameter, in that case the events will not be logged.

	This does not allocate any items, you must call alloc_keeprs () before using the object.
*/
JazzBlockKeepr::JazzBlockKeepr(jazz_utils::pJazzLogger a_logger)
{
	p_log = a_logger;

	_buffer_lock_	 = 0;
	keepr_item_size	 = item_size();
	num_allocd_items = 0;
	p_buffer_base	 = nullptr;
	p_first_item	 = nullptr;
	p_first_free	 = nullptr;
}


/** Destructor for class JazzBlockKeepr

	This automatically destoys all the JazzBocks and the corresponding JazzBlockKeeprItem descendants.
*/
JazzBlockKeepr::~JazzBlockKeepr()
{
	destroy_keeprs();
}


/** Allocate the buffer of JazzBlockKeeprItem descendant objects

	\param num_items The number of JazzBlockKeeprItem descendant objects
	\return True if successful. Logs with level LOG_ERROR if failed or called on an already allocated object.
	Fails if called when the buffer is already allocated.
*/
bool JazzBlockKeepr::alloc_keeprs(int num_items)
{
	if (num_allocd_items != 0 || num_items < 1) {
		log(LOG_ERROR, "JazzBlockKeepr::alloc_keeprs(): Wrong call.");

		return false;
	}

	p_buffer_base = (pJazzBlockKeeprItem) malloc(keepr_item_size*num_items);

	if (p_buffer_base == nullptr) {
		log(LOG_ERROR, "JazzBlockKeepr::alloc_keeprs(): malloc() failed.");

		return false;
	}

	memset(p_buffer_base, 0, keepr_item_size*num_items);

	pJazzBlockKeeprItem p_item = p_first_free = p_buffer_base;

	for (int i = 0; i < num_items - 1; i++) {
		void *pt = p_item + keepr_item_size;

		p_item->p_alloc_next = (pJazzBlockKeeprItem) pt;

		p_item = (pJazzBlockKeeprItem) pt;
	}

	num_allocd_items = num_items;

	return true;
}


/** Destroys all the JazzBocks and all the corresponding JazzBlockKeeprItem descendants.

	This releases all the memory allocated by the JazzBlockKeepr object and leaves it in a state where alloc_keeprs() can be called
to create a new buffer.

	Logs with level LOG_ERROR on any error condition such as items holding nonexistent blocks.
*/
void JazzBlockKeepr::destroy_keeprs()
{
}


/** Create a new JazzBlock as a selection (of possibly all) of an existing JazzBlock owned by a JazzBlockKeeprItem

	\param id64			A binary block ID. When blocks are created by giving a JazzBlockIdentifier, their binary ID is built automatically
						as the hash of their JazzBlockIdentifier (NOT the content of the block). That is the normal way of creating blocks
						and the only way that supports persisted blocks. Volatile blocks may have a "forced" binary id computed as the
						result of the function that creates them and the hashes of its dependencies. To support creating blocks (typically
						cached results of functions) with a give binary id, this form of new_jazz_block() comes in hand. Block items using
						this always have their JazzBlockIdentifier defined as a null string.
	\param p_as_block	An existing block from which everything is copied, possibly with a selection over its rows.
	\param p_row_filter A filter that is applicable to p_as_block. I.e., p_row_filter->can_filter(p_as_block) == true
						If p_row_filter == nullptr then p_as_block is copied into a newly allocated pointer.
						See parameter dim in the new_jazz_block() version that uses dim to understand how selection is applied.
	\param att			An alternative source of attributes. When this parameter in != nullptr, the new block will get its
						attributes from att instead of copying those in p_as_block->.

	OWNERSHIP: If you create a one shot block using new_jazz_block(), you earn the responsibility to free it with free_jazz_block().
	This is not the normal way to create JazzBlocks, when you use a JazzBlockKeepr descendant, that object will allocate and free
	the JazzBlocks automatically. The same applies to JazzBlocks created in the stack of a bebop program which are also managed
	automatically.

	\return	The address of the JazzBlockKeeprItem owning the new JazzBlock or nullptr if failed. Will not allocate a JazzBlockKeeprItem
	if allocating the JazzBlock fails.

	Logs with level LOG_ERROR on any error condition.
*/
pJazzBlockKeeprItem JazzBlockKeepr::new_jazz_block (const JazzBlockId64	id64,
													pJazzBlock			p_as_block,
													pJazzFilter			p_row_filter,
													AllAttributes	   *att)
{
}


/** Create a new JazzBlock (including a JazzFilter) from scratch owned by a JazzBlockKeeprItem

	\param id64				A binary block ID. When blocks are created by giving a JazzBlockIdentifier, their binary ID is built automatically
							as the hash of their JazzBlockIdentifier (NOT the content of the block). That is the normal way of creating blocks
							and the only way that supports persisted blocks. Volatile blocks may have a "forced" binary id computed as the
							result of the function that creates them and the hashes of its dependencies. To support creating blocks (typically
							cached results of functions) with a give binary id, this form of new_jazz_block() comes in hand. Block items using
							this always have their JazzBlockIdentifier defined as a null string.
	\param cell_type		The type for the tensor's cell types in [CELL_TYPE_BYTE..CELL_TYPE_DOUBLE]
	\param dim				This defines both the rank and the dimensions of the tensor. Note that, except for the first position a
							dimension of 0 and 1 is the same dim = {3, 1} is a vector of 3 elements with rank 1, exactly like {3, 0}.
							As a matter of convention, dim should always end with a 0 except when it is JAZZ_MAX_TENSOR_RANK long.
							For the first dimension 1 means one element and 0 means no element. Both have rank 1. The latter is the
							typical result of a selection where no row matches the condition. Blocks never have rank == 0 and zero-element
							blocks have the same rank as the block from which they were selected. When 0 rows are selected from a block
							of dim = {r, s, t} the resulting block is {0, s, t} with size == 0 and rank == 3.
							If dim == nullptr and p_text != nullptr, dim will be set automatically to the number of lines (see eoln) in p_text
							when cell_type == CELL_TYPE_JAZZ_STRING.
	\param att				The attributes to set when creating the block. They are be immutable. To change the attributes of a JazzBlock
							use the version of new_jazz_block() with parameter p_as_block.
	\param fill_tensor		How to fill the tensor. When creating anything that is not a JazzFilter, p_bool_filter is ignored and the options
							are: JAZZ_FILL_NEW_DONT_FILL (don't do anything with the tensor), JAZZ_FILL_NEW_WITH_ZERO (fill with binary zero
							no matter what the cell_type is), JAZZ_FILL_NEW_WITH_NA fill with the appropriate NA for the cell_type)
							When creating a filter, p_bool_filter must be a vector of length == size and the filter will be created as
							boolean (when fill_tensor == JAZZ_FILL_BOOLEAN_FILTER) or integer (when fill_tensor == JAZZ_FILL_INTEGER_FILTER)
	\param p_bool_filter	The vector of boolean (each true value means the corresponding row is selected) used when fill_tensor ==
							JAZZ_FILL_BOOLEAN_FILTER and fill_tensor == JAZZ_FILL_INTEGER_FILTER
	\param stringbuff_size	One of the possible ways to allocate space for strings is declaring this size. When this is non-zero a buffer
							will be allocated with this size plus whatever size is required by the strings in att. new_jazz_block() will
							only allocate the space and do nothing with it. The caller should assign strings to cells with JazzBlock.set_string().
	\param p_text			The other possible way to allocate space for strings is by declaring p_text. Imagine the content of p_text
							as a text file with n = size rows that will be pushed into the tensor and the string buffer. The eoln character
							separates the cells. (cell_type == CELL_TYPE_JAZZ_STRING & p_text != nullptr) overrides any setting in fill_tensor.
							Also, either dim should be nullptr and set automatically or its resulting size must be the same as the number of
							lines in p_text.
	\param eoln				A single character that separates the cells in p_text and will not be pushed to the string buffer.

	NOTES: String buffer allocation should not be used to dynamically change attribute values. Attributes are immutable and should be changed
	only creating a new block with new = new_jazz_block(p_as_block = old, att = new_att).
	String buffer allocation should only be used for cell_type == CELL_TYPE_JAZZ_STRING and either with stringbuff_size or with p_text (and eoln).
	If stringbuff_size is used, JazzBlock.set_string() should be used afterwards. If p_text is used, the tensor is already filled and
	JazzBlock.set_string() **should not** be called after that.

	OWNERSHIP: If you create a one shot block using new_jazz_block(), you earn the responsibility to free it with free_jazz_block().
	This is not the normal way to create JazzBlocks, when you use a JazzBlockKeepr descendant, that object will allocate and free
	the JazzBlocks automatically. The same applies to JazzBlocks created in the stack of a bebop program which are also managed
	automatically.

	\return	The address of the JazzBlockKeeprItem owning the new JazzBlock or nullptr if failed. Will not allocate a JazzBlockKeeprItem
	if allocating the JazzBlock fails.

	Logs with level LOG_ERROR on any error condition.
*/
pJazzBlockKeeprItem JazzBlockKeepr::new_jazz_block (const JazzBlockId64	id64,
													int					cell_type,
													JazzTensorDim	   *dim,
													AllAttributes	   *att,
													int					fill_tensor,
													bool			   *p_bool_filter,
													int					stringbuff_size,
													const char		   *p_text,
													char				eoln)
{
//TODO: Implement JazzBlockKeepr::new_jazz_block (2)
}


/** Create a new JazzBlock as a selection (of possibly all) of an existing JazzBlock owned by a JazzBlockKeeprItem

	\param p_id			A block ID. A string matching JAZZ_REGEX_VALIDATE_BLOCK_ID to identify the block globally and locally.
	\param p_as_block	An existing block from which everything is copied, possibly with a selection over its rows.
	\param p_row_filter A filter that is applicable to p_as_block. I.e., p_row_filter->can_filter(p_as_block) == true
						If p_row_filter == nullptr then p_as_block is copied into a newly allocated pointer.
						See parameter dim in the new_jazz_block() version that uses dim to understand how selection is applied.
	\param att			An alternative source of attributes. When this parameter in != nullptr, the new block will get its
						attributes from att instead of copying those in p_as_block->.

	OWNERSHIP: If you create a one shot block using new_jazz_block(), you earn the responsibility to free it with free_jazz_block().
	This is not the normal way to create JazzBlocks, when you use a JazzBlockKeepr descendant, that object will allocate and free
	the JazzBlocks automatically. The same applies to JazzBlocks created in the stack of a bebop program which are also managed
	automatically.

	\return	The address of the JazzBlockKeeprItem owning the new JazzBlock or nullptr if failed. Will not allocate a JazzBlockKeeprItem
	if allocating the JazzBlock fails.

	Logs with level LOG_ERROR on any error condition.
*/
pJazzBlockKeeprItem JazzBlockKeepr::new_jazz_block (const JazzBlockIdentifier *p_id,
													pJazzBlock			  	   p_as_block,
													pJazzFilter			  	   p_row_filter,
													AllAttributes			  *att)
{
//TODO: Implement JazzBlockKeepr::new_jazz_block (3)
}


/** Create a new JazzBlock (including a JazzFilter) from scratch owned by a JazzBlockKeeprItem

	\param p_id				A block ID. A string matching JAZZ_REGEX_VALIDATE_BLOCK_ID to identify the block globally and locally.
	\param cell_type		The type for the tensor's cell types in [CELL_TYPE_BYTE..CELL_TYPE_DOUBLE]
	\param dim				This defines both the rank and the dimensions of the tensor. Note that, except for the first position a
							dimension of 0 and 1 is the same dim = {3, 1} is a vector of 3 elements with rank 1, exactly like {3, 0}.
							As a matter of convention, dim should always end with a 0 except when it is JAZZ_MAX_TENSOR_RANK long.
							For the first dimension 1 means one element and 0 means no element. Both have rank 1. The latter is the
							typical result of a selection where no row matches the condition. Blocks never have rank == 0 and zero-element
							blocks have the same rank as the block from which they were selected. When 0 rows are selected from a block
							of dim = {r, s, t} the resulting block is {0, s, t} with size == 0 and rank == 3.
							If dim == nullptr and p_text != nullptr, dim will be set automatically to the number of lines (see eoln) in p_text
							when cell_type == CELL_TYPE_JAZZ_STRING.
	\param att				The attributes to set when creating the block. They are be immutable. To change the attributes of a JazzBlock
							use the version of new_jazz_block() with parameter p_as_block.
	\param fill_tensor		How to fill the tensor. When creating anything that is not a JazzFilter, p_bool_filter is ignored and the options
							are: JAZZ_FILL_NEW_DONT_FILL (don't do anything with the tensor), JAZZ_FILL_NEW_WITH_ZERO (fill with binary zero
							no matter what the cell_type is), JAZZ_FILL_NEW_WITH_NA fill with the appropriate NA for the cell_type)
							When creating a filter, p_bool_filter must be a vector of length == size and the filter will be created as
							boolean (when fill_tensor == JAZZ_FILL_BOOLEAN_FILTER) or integer (when fill_tensor == JAZZ_FILL_INTEGER_FILTER)
	\param p_bool_filter	The vector of boolean (each true value means the corresponding row is selected) used when fill_tensor ==
							JAZZ_FILL_BOOLEAN_FILTER and fill_tensor == JAZZ_FILL_INTEGER_FILTER
	\param stringbuff_size	One of the possible ways to allocate space for strings is declaring this size. When this is non-zero a buffer
							will be allocated with this size plus whatever size is required by the strings in att. new_jazz_block() will
							only allocate the space and do nothing with it. The caller should assign strings to cells with JazzBlock.set_string().
	\param p_text			The other possible way to allocate space for strings is by declaring p_text. Imagine the content of p_text
							as a text file with n = size rows that will be pushed into the tensor and the string buffer. The eoln character
							separates the cells. (cell_type == CELL_TYPE_JAZZ_STRING & p_text != nullptr) overrides any setting in fill_tensor.
							Also, either dim should be nullptr and set automatically or its resulting size must be the same as the number of
							lines in p_text.
	\param eoln				A single character that separates the cells in p_text and will not be pushed to the string buffer.

	NOTES: String buffer allocation should not be used to dynamically change attribute values. Attributes are immutable and should be changed
	only creating a new block with new = new_jazz_block(p_as_block = old, att = new_att).
	String buffer allocation should only be used for cell_type == CELL_TYPE_JAZZ_STRING and either with stringbuff_size or with p_text (and eoln).
	If stringbuff_size is used, JazzBlock.set_string() should be used afterwards. If p_text is used, the tensor is already filled and
	JazzBlock.set_string() **should not** be called after that.

	OWNERSHIP: If you create a one shot block using new_jazz_block(), you earn the responsibility to free it with free_jazz_block().
	This is not the normal way to create JazzBlocks, when you use a JazzBlockKeepr descendant, that object will allocate and free
	the JazzBlocks automatically. The same applies to JazzBlocks created in the stack of a bebop program which are also managed
	automatically.

	\return	The address of the JazzBlockKeeprItem owning the new JazzBlock or nullptr if failed. Will not allocate a JazzBlockKeeprItem
	if allocating the JazzBlock fails.

	Logs with level LOG_ERROR on any error condition.
*/
pJazzBlockKeeprItem JazzBlockKeepr::new_jazz_block (const JazzBlockIdentifier *p_id,
													int						   cell_type,
													JazzTensorDim			  *dim,
													AllAttributes			  *att,
													int						   fill_tensor,
													bool					  *p_bool_filter,
													int						   stringbuff_size,
													const char				  *p_text,
													char					   eoln)
{
//TODO: Implement JazzBlockKeepr::new_jazz_block (4)
}


/** Aaa

//TODO: Document JazzBlockKeepr::new_keepr_item
*/
pJazzBlockKeeprItem JazzBlockKeepr::new_keepr_item ()
{
//TODO: Implement JazzBlockKeepr::new_keepr_item
}


/** Destroy a JazzBlock and free its owning JazzBlockKeeprItem

	\param p_item The JazzBlockKeeprItem owning the JazzBlock that will be destroyed.
*/
void JazzBlockKeepr::free_jazz_block(pJazzBlockKeeprItem p_item)
{
//TODO: Implement JazzBlockKeepr::free_jazz_block
}


/** Aaa

//TODO: Document AATBlockQueue::AATBlockQueue
*/
AATBlockQueue::AATBlockQueue(jazz_utils::pJazzLogger a_logger)
{
//TODO: Implement AATBlockQueue::AATBlockQueue
}


/** Aaa

//TODO: Document AATBlockQueue::~AATBlockQueue
*/
AATBlockQueue::~AATBlockQueue()
{
//TODO: Implement AATBlockQueue::~AATBlockQueue
}


/** Aaa

//TODO: Document AATBlockQueue::alloc_keeprs
*/
bool AATBlockQueue::alloc_keeprs  (int num_items)
{
//TODO: Implement AATBlockQueue::alloc_keeprs
}


/** Aaa

//TODO: Document AATBlockQueue::destroy_keeprs
*/
void AATBlockQueue::destroy_keeprs()
{
//TODO: Implement AATBlockQueue::destroy_keeprs
}


/** Aaa

//TODO: Document AATBlockQueue::new_jazz_block (1)
*/
pJazzQueueItem AATBlockQueue::new_jazz_block (const JazzBlockId64 id64,
											  pJazzBlock		  p_as_block,
											  pJazzFilter		  p_row_filter,
											  AllAttributes	   	 *att,
											  uint64_t			  time_to_build)
{
//TODO: Implement AATBlockQueue::new_jazz_block (1)
}


/** Aaa

//TODO: Document AATBlockQueue::new_jazz_block (2)
*/
pJazzQueueItem AATBlockQueue::new_jazz_block (const JazzBlockId64 id64,
											  int				  cell_type,
											  JazzTensorDim	   	 *dim,
											  AllAttributes	   	 *att,
											  int				  fill_tensor,
											  bool				 *p_bool_filter,
											  int				  stringbuff_size,
											  const char		 *p_text,
											  char				  eoln,
											  uint64_t			  time_to_build)
{
//TODO: Implement AATBlockQueue::new_jazz_block (2)
}


/** Create a new JazzBlock as a selection (of possibly all) of an existing JazzBlock owned by a JazzQueueItem

	\param p_id			 A block ID. A string matching JAZZ_REGEX_VALIDATE_BLOCK_ID to identify the block globally and locally.
	\param p_as_block	 An existing block from which everything is copied, possibly with a selection over its rows.
	\param p_row_filter	 A filter that is applicable to p_as_block. I.e., p_row_filter->can_filter(p_as_block) == true
						 If p_row_filter == nullptr then p_as_block is copied into a newly allocated pointer.
						 See parameter dim in the new_jazz_block() version that uses dim to understand how selection is applied.
	\param att			 An alternative source of attributes. When this parameter in != nullptr, the new block will get its
						 attributes from att instead of copying those in p_as_block->.
	\param time_to_build The time to build the object in microseconds. (this typically includes the evaluation of the function who built it.)
						 If that value is known, it may be used to optimize the priority of the block in the queue.

	OWNERSHIP: If you create a one shot block using new_jazz_block(), you earn the responsibility to free it with free_jazz_block().
	This is not the normal way to create JazzBlocks, when you use a JazzBlockKeepr descendant, that object will allocate and free
	the JazzBlocks automatically. The same applies to JazzBlocks created in the stack of a bebop program which are also managed
	automatically.

	\return	The address of the JazzQueueItem owning the new JazzBlock or nullptr if failed. Will not allocate a JazzQueueItem
	if allocating the JazzBlock fails.
*/
pJazzQueueItem AATBlockQueue::new_jazz_block (const JazzBlockIdentifier *p_id,
											  pJazzBlock			  	 p_as_block,
											  pJazzFilter			  	 p_row_filter,
											  AllAttributes				*att,
											  uint64_t					 time_to_build)
{
//TODO: Implement AATBlockQueue::new_jazz_block (3)
}


/** Create a new JazzBlock (including a JazzFilter) from scratch owned by a JazzQueueItem

	\param p_id				A block ID. A string matching JAZZ_REGEX_VALIDATE_BLOCK_ID to identify the block globally and locally.
	\param cell_type		The type for the tensor's cell types in [CELL_TYPE_BYTE..CELL_TYPE_DOUBLE]
	\param dim				This defines both the rank and the dimensions of the tensor. Note that, except for the first position a
							dimension of 0 and 1 is the same dim = {3, 1} is a vector of 3 elements with rank 1, exactly like {3, 0}.
							As a matter of convention, dim should always end with a 0 except when it is JAZZ_MAX_TENSOR_RANK long.
							For the first dimension 1 means one element and 0 means no element. Both have rank 1. The latter is the
							typical result of a selection where no row matches the condition. Blocks never have rank == 0 and zero-element
							blocks have the same rank as the block from which they were selected. When 0 rows are selected from a block
							of dim = {r, s, t} the resulting block is {0, s, t} with size == 0 and rank == 3.
							If dim == nullptr and p_text != nullptr, dim will be set automatically to the number of lines (see eoln) in p_text
							when cell_type == CELL_TYPE_JAZZ_STRING.
	\param att				The attributes to set when creating the block. They are be immutable. To change the attributes of a JazzBlock
							use the version of new_jazz_block() with parameter p_as_block.
	\param fill_tensor		How to fill the tensor. When creating anything that is not a JazzFilter, p_bool_filter is ignored and the options
							are: JAZZ_FILL_NEW_DONT_FILL (don't do anything with the tensor), JAZZ_FILL_NEW_WITH_ZERO (fill with binary zero
							no matter what the cell_type is), JAZZ_FILL_NEW_WITH_NA fill with the appropriate NA for the cell_type)
							When creating a filter, p_bool_filter must be a vector of length == size and the filter will be created as
							boolean (when fill_tensor == JAZZ_FILL_BOOLEAN_FILTER) or integer (when fill_tensor == JAZZ_FILL_INTEGER_FILTER)
	\param p_bool_filter	The vector of boolean (each true value means the corresponding row is selected) used when fill_tensor ==
							JAZZ_FILL_BOOLEAN_FILTER and fill_tensor == JAZZ_FILL_INTEGER_FILTER
	\param stringbuff_size	One of the possible ways to allocate space for strings is declaring this size. When this is non-zero a buffer
							will be allocated with this size plus whatever size is required by the strings in att. new_jazz_block() will
							only allocate the space and do nothing with it. The caller should assign strings to cells with JazzBlock.set_string().
	\param p_text			The other possible way to allocate space for strings is by declaring p_text. Imagine the content of p_text
							as a text file with n = size rows that will be pushed into the tensor and the string buffer. The eoln character
							separates the cells. (cell_type == CELL_TYPE_JAZZ_STRING & p_text != nullptr) overrides any setting in fill_tensor.
							Also, either dim should be nullptr and set automatically or its resulting size must be the same as the number of
							lines in p_text.
	\param eoln				A single character that separates the cells in p_text and will not be pushed to the string buffer.
	\param time_to_build	The time to build the object in microseconds. (this typically includes the evaluation of the function who built it.)
							If that value is known, it may be used to optimize the priority of the block in the queue.

	NOTES: String buffer allocation should not be used to dynamically change attribute values. Attributes are immutable and should be changed
	only creating a new block with new = new_jazz_block(p_as_block = old, att = new_att).
	String buffer allocation should only be used for cell_type == CELL_TYPE_JAZZ_STRING and either with stringbuff_size or with p_text (and eoln).
	If stringbuff_size is used, JazzBlock.set_string() should be used afterwards. If p_text is used, the tensor is already filled and
	JazzBlock.set_string() **should not** be called after that.

	OWNERSHIP: If you create a one shot block using new_jazz_block(), you earn the responsibility to free it with free_jazz_block().
	This is not the normal way to create JazzBlocks, when you use a JazzBlockKeepr descendant, that object will allocate and free
	the JazzBlocks automatically. The same applies to JazzBlocks created in the stack of a bebop program which are also managed
	automatically.

	\return	The address of the JazzQueueItem owning the new JazzBlock or nullptr if failed. Will not allocate a JazzQueueItem
	if allocating the JazzBlock fails.
*/
pJazzQueueItem AATBlockQueue::new_jazz_block (const JazzBlockIdentifier *p_id,
											  int						 cell_type,
											  JazzTensorDim				*dim,
											  AllAttributes				*att,
											  int						 fill_tensor,
											  bool						*p_bool_filter,
											  int						 stringbuff_size,
											  const char				*p_text,
											  char						 eoln,
											  uint64_t					 time_to_build)
{
//TODO: Implement AATBlockQueue::new_jazz_block (4)
}


/** Aaa

//TODO: Document JazzBlockKeepr::new_keepr_item
*/
pJazzQueueItem AATBlockQueue::new_keepr_item()
{
//TODO: Implement JazzBlockKeepr::new_keepr_item
}


/** Destroy a JazzBlock and free its owning JazzQueueItem

	\param p_item The JazzQueueItem owning the JazzBlock that will be destroyed.
*/
void AATBlockQueue::free_jazz_block(pJazzQueueItem p_item)
{
//TODO: Implement AATBlockQueue::free_jazz_block
}


/** Return the JazzQueueItem with the highest priority value in the AATBlockQueue

	\param lock_it Locks the JazzQueueItem to avoid it being selected by another (possibly racing) highest_priority_item() call.
	\return		   A pointer to JazzQueueItem holding the block or nullptr if there are no items in the AATBlockQueue.

This public method is thread safe, unlike the corresponding private method highest_priority(). The JazzQueueItem
has to be explicitly removed with free_jazz_block() or unlocked with "JazzQueueItem.is_locked = false;" to make it findable
again in case it is not removed.
*/
pJazzQueueItem AATBlockQueue::highest_priority_item (bool lock_it)
{
//TODO: Implement AATBlockQueue::highest_priority_item
}


/** Return the JazzQueueItem with the lowest priority value in the AATBlockQueue

	\param lock_it Locks the JazzQueueItem to avoid it being selected by another (possibly racing) lowest_priority_item() call.
	\return		   A pointer to JazzQueueItem holding the block or nullptr if there are no items in the AATBlockQueue.

This public method is thread safe, unlike the corresponding private method lowest_priority(). The JazzQueueItem
has to be explicitly removed with free_jazz_block() or unlocked with "JazzQueueItem.is_locked = false;" to make it findable
again in case it is not removed.
*/
pJazzQueueItem AATBlockQueue::lowest_priority_item (bool lock_it)
{
//TODO: Implement AATBlockQueue::lowest_priority_item
}


/** Evaluate the priority of a JazzQueueItem

	\param p_item A pointer to the JazzQueueItem whose priority is to be set.

	Each time a new JazzQueueItem is added to the AATBlockQueue this virtual method will be called. The method does not
return anything, so it should set p_item->priority = some_computation(). Typically, the computation will involve the recency,
the size, times_used and time_to_build, etc.
*/
void AATBlockQueue::set_item_priority(pJazzQueueItem p_item)
{
	double prio = fmin(fmax(1.0, p_item->time_to_build/1000), 50) + discrete_recency;	// Number of ms in {1..50}

	discrete_recency = discrete_recency + 0.0001;										// Increasing 1 every 10000 blocks

	p_item->priority = prio*(p_item->times_used + 1);
}

/*

// pJazzQueueItem insert(pJazzQueueItem pN, pJazzQueueItem pT):
// -----------------------------------

// input: N, the value to be inserted, and T, the root of the tree to insert it into.
// output: A balanced version T including X.

pJazzQueueItem insert(pJazzQueueItem pN, pJazzQueueItem pT)
{

// Do the normal binary tree insertion procedure.  Set the result of the
// recursive call to the correct child in case a new node was created or the
// root of the subtree changes.

	if (pT == NULL)
	{
		pN->level = 1;
		pN->p_alloc_prev = NULL;
		pN->p_alloc_next = NULL;

		return pN;
	}
	else
	{
		if (pN->priority < pT->priority) pT->p_alloc_prev = insert(pN, pT->p_alloc_prev);
		else							 pT->p_alloc_next = insert(pN, pT->p_alloc_next);
	}

// Perform skew and then split.	 The conditionals that determine whether or
// not a rotation will occur or not are inside of the procedures, as given above.

	pT = skew(pT);
	pT = split(pT);

	return pT;
};


// pJazzQueueItem remove(pJazzQueueItem pN, pJazzQueueItem pT):
// -----------------------------------

// input: N, the node to remove, and T, the root of the tree from which it should be deleted.
// output: T, balanced, without the node N.

pJazzQueueItem remove_hi(pJazzQueueItem pN, pJazzQueueItem pT)
{
	if (pN == pT)
	{
		if (!pT->p_alloc_prev)
		{
			pT = pT->p_alloc_next;
			if (!pT) return NULL;
		}
		else pT = pT->p_alloc_prev;
	}
	else
	{
		if (pN->priority < pT->priority) pT->p_alloc_prev = remove_hi(pN, pT->p_alloc_prev);
		else							 pT->p_alloc_next = remove_hi(pN, pT->p_alloc_next);
	};

// Rebalance the tree.	Decrease the level of all nodes in this level if
// necessary, and then skew and split all nodes in the new level.

	decrease_level(pT);
	pT = skew(pT);
	if (pT->p_alloc_next)
	{
		pT->p_alloc_next = skew(pT->p_alloc_next);
		if (pT->p_alloc_next->p_alloc_next) pT->p_alloc_next->p_alloc_next = skew(pT->p_alloc_next->p_alloc_next);
	};
	pT = split(pT);
	if (pT->p_alloc_next) pT->p_alloc_next = split(pT->p_alloc_next);

	return pT;
};


pJazzQueueItem remove_lo(pJazzQueueItem pN, pJazzQueueItem pT)
{
	if (pN == pT)
	{
		if (!pT->p_alloc_prev)
		{
			pT = pT->p_alloc_next;
			if (!pT) return NULL;
		}
		else pT = pT->p_alloc_prev;
	}
	else
	{
		if (pN->priority <= pT->priority) pT->p_alloc_prev = remove_lo(pN, pT->p_alloc_prev);
		else							  pT->p_alloc_next = remove_lo(pN, pT->p_alloc_next);
	};

// Rebalance the tree.	Decrease the level of all nodes in this level if
// necessary, and then skew and split all nodes in the new level.

	decrease_level(pT);
	pT = skew(pT);
	if (pT->p_alloc_next)
	{
		pT->p_alloc_next = skew(pT->p_alloc_next);
		if (pT->p_alloc_next->p_alloc_next) pT->p_alloc_next->p_alloc_next = skew(pT->p_alloc_next->p_alloc_next);
	};
	pT = split(pT);
	if (pT->p_alloc_next) pT->p_alloc_next = split(pT->p_alloc_next);

	return pT;
};


ModelBuffer::ModelBuffer()
{
	p_buffer_base  = NULL;
	p_queue_root = NULL;
	p_first_free = NULL;
	num_allocd_items  = 0;
};


ModelBuffer::~ModelBuffer()
{
	if (p_buffer_base) delete [] p_buffer_base;
};


bool ModelBuffer::AllocModels(int numModels)
{
	if (p_buffer_base) delete [] p_buffer_base;

	p_buffer_base  = NULL;
	p_queue_root = NULL;
	p_first_free = NULL;
	num_allocd_items  = 0;

	if (!numModels) return true;

	p_buffer_base = new	 (nothrow) Model_InGrQS [numModels];

	if (!p_buffer_base) return false;

	num_allocd_items = numModels;

	memset(p_buffer_base, 0, sizeof(Model_InGrQS)*num_allocd_items);

	p_queue_root = NULL;
	p_first_free = p_buffer_base;

	int i;

	for (i = 0; i < num_allocd_items - 1; i++) p_buffer_base[i].p_alloc_next = &p_buffer_base[i + 1];

	return true;
};


pJazzQueueItem ModelBuffer::GetFreeModel()
{
	if (p_first_free)
	{
		pJazzQueueItem pM = p_first_free;
		p_first_free = pM->p_alloc_next;
		return pM;
	};

	pJazzQueueItem pLP = lowest_priority(p_queue_root);

	p_queue_root = remove_lo(pLP, p_queue_root);

	return pLP;
};


void ModelBuffer::PushModelToPriorityQueue(pJazzQueueItem pM)
{
	p_queue_root = insert(pM, p_queue_root);
};


pJazzQueueItem ModelBuffer::highest_priority_item()
{
	pJazzQueueItem pHP = highest_priority(p_queue_root);

	if (pHP)
	{
		p_queue_root = remove_hi(pHP, p_queue_root);

		pHP->p_alloc_prev = NULL;
		pHP->p_alloc_next = p_first_free;
		p_first_free = pHP;
	};

	return pHP;
};

*/


/** Delete a block and remove its JazzBlockKeeprItem descendant searching by JazzBlockIdentifier (block name hash)

	\param p_id The JazzBlockIdentifier of the block to be searched for deletion

	Logs with level LOG_MISS if the block is not found.
*/
void JazzCache::free_jazz_block (const JazzBlockIdentifier *p_id)
{
	pJazzQueueItem p_item = find_jazz_block (p_id);

	if (p_item == nullptr) {
		log_printf(LOG_MISS, "Block %s not found in JazzCache::free_jazz_block()", p_id);

		return;
	}

	AATBlockQueue::free_jazz_block(p_item);
}


/** Delete a block and remove its JazzBlockKeeprItem descendant searching by JazzBlockId64 (block name)

	\param id64 The JazzBlockId64 of the block to be searched for deletion

	Logs with level LOG_MISS if the block is not found.
*/
void JazzCache::free_jazz_block (JazzBlockId64 id64)
{
	pJazzQueueItem p_item = find_jazz_block (id64);

	if (p_item == nullptr) {
		log_printf(LOG_MISS, "Block with hash %16x not found in JazzCache::free_jazz_block()", id64);

		return;
	}

	AATBlockQueue::free_jazz_block(p_item);
}


} // namespace jazz_containers


#if defined CATCH_TEST
#include "src/jazz_elements/tests/test_containers.ctest"
#endif
