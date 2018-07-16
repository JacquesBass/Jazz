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

#include "src/jazz_elements/jazz_stdcore.h"

namespace jazz_stdcore
{

struct R_binary						///< The header of any R object
{
	unsigned short signature;		///< The constant two chars: "X\\n"
	int			   format_version;	///< The constant 0x00, 0x00, 0x00, 0x02
	int			   writer;			///< R 3.2.5	0x00, 0x03, 0x02, 0x05
	int			   min_reader;		///< R 2.3.0	0x00, 0x02, 0x03, 0x00
	int			   R_type;			///< The SEXP type (E.g., LGLSXP for boolean)
	int			   R_length;		///< Number of elements (R booleans are 32 bit long)
}__attribute__((packed));


struct RStr_header					///< The header of each individual R string
{
	int signature;					///< The constant four bytes: 00 04 00 09
	int n_char;						///< Number of characters in each string.
};


union pRStr_stream					///< A pointer to the R stream usable as both RStr_header_int and char.
{
	char 		*p_char;
	RStr_header *p_head;
};


/** Internal binary const related with R serialization.
*/
#define R_SIG_RBINARY_SIGNATURE		0x0a58
#define R_SIG_RBINARY_FORMATVERSION	0x2000000
#define R_SIG_RBINARY_WRITER		0x5020300
#define R_SIG_RBINARY_MINREADER		0x30200
#define R_SIG_CHARSXP_HEA_TO_R		0x09000400
#define R_SIG_CHARSXP_HEA_FROM_R	0x09000000
#define R_SIG_CHARSXP_MASK_FROM_R	0xff000000
#define R_SIG_CHARSXP_NA			0x09000000
#define R_SIG_CHARSXP_NA_LENGTH		0xFFFFffff
#define R_SIG_ONE					0x1000000
#define R_SIG_NA_LOGICAL			0x80


/** NA converted as output.
*/
#define LENGTH_NA_AS_TEXT	3		///< The length of NA_AS_TEXT
#define NA_AS_TEXT			"NA\n"	///< The output produced by translate_block_TO_TEXT() for all NA values.


/**
//TODO: Document JazzCoreTypecasting()
*/
JazzCoreTypecasting::JazzCoreTypecasting(jazz_utils::pJazzLogger a_logger)	: JazzObject(a_logger)
{
//TODO: Implement JazzCoreTypecasting
}


/**
//TODO: Document ~JazzCoreTypecasting()
*/
JazzCoreTypecasting::~JazzCoreTypecasting()
{
//TODO: Implement ~JazzCoreTypecasting
}


/** Create a block_C_BOOL, block_C_R_INTEGER, block_C_R_REAL or block_C_OFFS_CHARS from a block_C_R_RAW of type BLOCKTYPE_RAW_R_RAW binary
compatible with a serialized R object.

	\param p_source	 An R object serialized as a block_C_R_RAW of type BLOCKTYPE_RAW_R_RAW
	\param p_dest Address of a pJazzBlock allocated by this function to store the block.

	\return		 true if successful, false and log(LOG_MISS, "further details") if not.

	This function does nothing with the p_source object except copying it.
	The returned p_dest is owned by the caller and must be JAZZFREE()ed when no longer necessary.
*/
bool JazzCoreTypecasting::FromR (pJazzBlock p_source, pJazzBlock &p_dest)
{
	R_binary * p_head = (R_binary *) &reinterpret_cast<pRawBlock>(p_source)->data;

	if (p_head->signature != sw_RBINARY_SIGNATURE || p_head->format_version != sw_RBINARY_FORMATVERSION)
	{
		jCommons.log(LOG_MISS, "jzzBLOCKCONV::translate_block_FROM_R() : Wrong signature || format_version.");

		return false;
	}

	int type = htonl(p_head->R_type), R_len = htonl(p_head->R_length);

	switch (type)
	{
		case LGLSXP:
			{
				bool ok = JAZZALLOC(p_dest, RAM_ALLOC_C_BOOL, R_len);
				if (!ok)
				{
					jCommons.log(LOG_MISS, "jzzBLOCKCONV::translate_block_FROM_R() : JAZZALLOC(RAM_ALLOC_C_BOOL) failed.");

					return false;
				}
				int	 *			p_data_src  = (int *)		   &p_head[1];
				unsigned char * p_data_dest = (unsigned char *) &reinterpret_cast<pBoolBlock>(p_dest)->data;
				for (int i = 0; i < R_len; i++)
				{
					if (p_data_src[i])
					{
						if (p_data_src[i] == sw_ONE) p_data_dest[i] = 1;
						else						p_data_dest[i] = JAZZC_NA_BOOL;
					}
					else							p_data_dest[i] = 0;
				}
			}
			break;

		case INTSXP:
			{
				bool ok = JAZZALLOC(p_dest, RAM_ALLOC_C_INTEGER, R_len);
				if (!ok)
				{
					jCommons.log(LOG_MISS, "jzzBLOCKCONV::translate_block_FROM_R() : JAZZALLOC(RAM_ALLOC_C_INTEGER) failed.");

					return false;
				}
				int * p_data_src	 = (int *) &p_head[1];
				int * p_data_dest = (int *) &reinterpret_cast<pIntBlock>(p_dest)->data;
				for (int i = 0; i < R_len; i++)
					p_data_dest[i] = htonl(p_data_src[i]);
			}
			break;

		case STRSXP:
			{
				// Allocation: R serializes all strings without final zero and with 8 trailing bytes -> string_buffer + Nx(4 bytes + 2 trailing)
				bool ok = JAZZALLOC(p_dest, RAM_ALLOC_C_OFFS_CHARS, p_source->size - sizeof(R_binary) + sizeof(string_buffer) + 8 - 2*R_len);
				if (!ok)
				{
					jCommons.log(LOG_MISS, "jzzBLOCKCONV::translate_block_FROM_R() : JAZZALLOC(RAM_ALLOC_C_OFFS_CHARS) failed.");

					return false;
				}
				if (!R_len)
				{
					memset(&reinterpret_cast<pCharBlock>(p_dest)->data, 0, p_dest->size);
					return true;
				}

				if (!format_C_OFFS_CHARS((pCharBlock) p_dest, R_len))
				{
					JAZZFREE(p_dest, AUTOTYPEBLOCK(p_dest));

					jCommons.log(LOG_MISS, "jzzBLOCKCONV::translate_block_FROM_R() : format_C_OFFS_CHARS() failed.");

					return false;
				}
				pRStr_stream src;
				src.pchar = (char *) &p_head[1];
				for (int i = 0; i < R_len; i++)
				{
					RStr_header rh = *src.phead++;

					if (rh.signature == sw_CHARSXP_NA && rh.nchar == sw_CHARSXP_NA_LENGTH)
					{
						reinterpret_cast<pCharBlock>(p_dest)->data[i] = JAZZC_NA_STRING;
					}
					else
					{
						if ((rh.signature & sw_CHARSXP_MASK_FROM_R) == sw_CHARSXP_HEA_FROM_R)
						{
							int nchar = htonl(rh.nchar);
							if (nchar >= MAX_STRING_LENGTH || nchar < 0)
							{
								JAZZFREE(p_dest, AUTOTYPEBLOCK(p_dest));

								jCommons.log(LOG_MISS, "jzzBLOCKCONV::translate_block_FROM_R() : String too long.");

								return false;
							}
							if (nchar == 0)
							{
								reinterpret_cast<pCharBlock>(p_dest)->data[i] = JAZZC_EMPTY_STRING;
							}
							else
							{
								reinterpret_cast<pCharBlock>(p_dest)->data[i] = get_string_idx_C_OFFS_CHARS((pCharBlock) p_dest, src.pchar, nchar);

								src.pchar += nchar;
							}
						}
						else
						{
							JAZZFREE(p_dest, AUTOTYPEBLOCK(p_dest));

							jCommons.log_printf(LOG_MISS, "jzzBLOCKCONV::translate_block_FROM_R() : Unexpected signature = %4X", rh.signature);

							return false;
						}
					}
				}
			}
			break;

		case REALSXP:
			{
				bool ok = JAZZALLOC(p_dest, RAM_ALLOC_C_REAL, R_len);
				if (!ok)
				{
					jCommons.log(LOG_MISS, "jzzBLOCKCONV::translate_block_FROM_R() : JAZZALLOC(RAM_ALLOC_C_REAL) failed.");

					return false;
				}
				int * p_data_src	 = (int *) &p_head[1];
				int * p_data_dest = (int *) &reinterpret_cast<pRealBlock>(p_dest)->data;
				for (int i = 0; i < R_len; i++)
				{
					p_data_dest[1] = ntohl(*p_data_src++);
					p_data_dest[0] = ntohl(*p_data_src++);

					p_data_dest += 2;
				}
			}
			break;

		default:
		{
			jCommons.log(LOG_MISS, "jzzBLOCKCONV::translate_block_FROM_R() : Wrong type.");

			return false;
		}
	}

	return true;
}


/** Create a block_C_R_RAW of type BLOCKTYPE_RAW_R_RAW binary compatible with a serialized R object from a block_C_BOOL, block_C_R_INTEGER,
block_C_R_REAL or block_C_OFFS_CHARS.

	\param p_source	 The source block (A block_C_BOOL, block_C_R_INTEGER, block_C_R_REAL or block_C_OFFS_CHARS).
	\param p_dest Address of a pJazzBlock allocated by this function to store the block.

	\return		 true if successful, false and log(LOG_MISS, "further details") if not.

	This function does nothing with the p_source object except copying it.
	The returned p_dest is owned by the caller and must be JAZZFREE()ed when no longer necessary.
*/
bool JazzCoreTypecasting::ToR (pJazzBlock p_source, pJazzBlock &p_dest)
{
	int size = sizeof(R_binary);

	switch (p_source->type)
	{
		case BLOCKTYPE_C_BOOL:
		case BLOCKTYPE_C_FACTOR:
		case BLOCKTYPE_C_GRADE:
		case BLOCKTYPE_C_INTEGER:
			size += p_source->length*sizeof(int);
			break;

		case BLOCKTYPE_C_OFFS_CHARS:
			for (int i = 0; i < p_source->length; i++)
				size += 2*sizeof(int) + strlen(PCHAR(reinterpret_cast<pCharBlock>(p_source), i));
			break;

		case BLOCKTYPE_C_TIMESEC:
		case BLOCKTYPE_C_REAL:
			size += p_source->length*sizeof(double);
			break;

		default:
			jCommons.log(LOG_MISS, "translate_block_TO_R(): Unsupported type.");

			return false;
	}

	bool ok = JAZZALLOC(p_dest, RAM_ALLOC_C_RAW, size);
	if (!ok)
	{
		jCommons.log(LOG_MISS, "translate_block_TO_R(): JAZZALLOC failed.");

		return false;

	}
	reinterpret_cast_block(p_dest, BLOCKTYPE_RAW_R_RAW);
	R_binary * pt = (R_binary *) &reinterpret_cast<pRawBlock>(p_dest)->data;

	pt->signature	   = sw_RBINARY_SIGNATURE;
	pt->format_version = sw_RBINARY_FORMATVERSION;
	pt->writer		   = sw_RBINARY_WRITER;
	pt->min_reader	   = sw_RBINARY_MINREADER;
	pt->R_length	   = ntohl(p_source->length);

	switch (p_source->type)
	{
		case BLOCKTYPE_C_BOOL:
			{
				int	 * p_data_dest = (int *) &pt[1];
				unsigned char * p_data_src  = (unsigned char *) &reinterpret_cast<pBoolBlock>(p_source)->data;
				pt->R_type = ntohl(LGLSXP);
				for (int i = 0; i < p_source->length; i++)
				{
					if (p_data_src[i])
					{
						if (p_data_src[i] == 1) p_data_dest[i] = sw_ONE;
						else				   p_data_dest[i] = sw_NA_LOGICAL;
					}
					else					   p_data_dest[i] = 0;
				}
			}
			break;

		case BLOCKTYPE_C_FACTOR:
		case BLOCKTYPE_C_GRADE:
		case BLOCKTYPE_C_INTEGER:
			{
				int * p_data_dest = (int *) &pt[1];
				int * p_data_src	 = (int *) &reinterpret_cast<pIntBlock>(p_source)->data;
				pt->R_type = ntohl(INTSXP);
				for (int i = 0; i < p_source->length; i++)
					p_data_dest[i] = ntohl(p_data_src[i]);
			}
			break;

		case BLOCKTYPE_C_OFFS_CHARS:
			{
				pRStr_stream dest;
				dest.pchar = (char *) &pt[1];
				RStr_header sth;
				pt->R_type = ntohl(STRSXP);
				for (int i = 0; i < p_source->length; i++)
				{
					if (reinterpret_cast<pCharBlock>(p_source)->data[i] != JAZZC_NA_STRING)
					{
						char * p_data_src = PCHAR(reinterpret_cast<pCharBlock>(p_source), i);
						int len = strlen(p_data_src);
						sth.nchar = ntohl(len);
						sth.signature = sw_CHARSXP_HEA_TO_R;
						*dest.phead++ = sth;
						for (int j = 0; j < len; j ++)
							*dest.pchar++ = p_data_src[j];
					}
					else
					{
						sth.nchar	  = sw_CHARSXP_NA_LENGTH;
						sth.signature = sw_CHARSXP_NA;
						*dest.phead++ = sth;
					}
				}
			}
			break;

		case BLOCKTYPE_C_TIMESEC:
		case BLOCKTYPE_C_REAL:
			{
				int * p_data_dest = (int *) &pt[1];
				int * p_data_src	 = (int *) &reinterpret_cast<pRealBlock>(p_source)->data;
				pt->R_type = ntohl(REALSXP);
				for (int i = 0; i < p_source->length; i++)
				{
					p_data_dest[1] = ntohl(*p_data_src++);
					p_data_dest[0] = ntohl(*p_data_src++);

					p_data_dest += 2;
				}
			}
	}

	return true;
}


/** Create a block_C_BOOL, block_C_R_INTEGER, block_C_R_REAL or block_C_OFFS_CHARS from a block_C_R_RAW of type BLOCKTYPE_RAW_MIME_CSV,
BLOCKTYPE_RAW_MIME_JSON, BLOCKTYPE_RAW_MIME_TSV or BLOCKTYPE_RAW_MIME_XML.

	\param p_source	 The source block (A block_C_R_RAW of type BLOCKTYPE_RAW_MIME_CSV, BLOCKTYPE_RAW_MIME_JSON, BLOCKTYPE_RAW_MIME_TSV or
				 BLOCKTYPE_RAW_MIME_XML).
	\param p_dest Address of a pJazzBlock allocated by this function to store the block.
	\param type	 The output type (BLOCKTYPE_C_BOOL, BLOCKTYPE_C_OFFS_CHARS, BLOCKTYPE_C_R_FACTOR, BLOCKTYPE_C_R_GRADE, BLOCKTYPE_C_R_INTEGER,
				 BLOCKTYPE_C_R_TIMESEC or BLOCKTYPE_C_R_REAL).
	\param fmt	 The (sscanf() compatible) format for converting the data. Each row of text must be convertible to one element of the appropriate
				 data type. E.g., " %lf," will convert " 1.23," to a double == 1.23

	\return		 true if successful, false and log(LOG_MISS, "further details") if not.

	This function does nothing with the p_source object except copying it.
	The returned p_dest is owned by the caller and must be JAZZFREE()ed when no longer necessary.
*/
bool JazzCoreTypecasting::FromText (pJazzBlock p_source, pJazzBlock &p_dest, int type, char *fmt)
{
	bool copy = fmt[0] == 0;

	if (copy && type != BLOCKTYPE_C_OFFS_CHARS)
	{
		jCommons.log(LOG_MISS, "jzzBLOCKCONV::translate_block_FROM_TEXT() : fmt == '' is only valid for strings.");

		return false;
	}

	char buff[MAX_STRING_LENGTH + 4];
	char * pchar = (char *) &reinterpret_cast<pRawBlock>(p_source)->data;

	int bytes  = p_source->size;
	int length = 0;
	int nchar;

	while (bytes > 0)
	{
		length++;
		nchar = (uintptr_t) strchrnul(pchar, '\n') - (uintptr_t) pchar + 1;
		nchar = min(nchar, bytes);

		if (nchar >= MAX_STRING_LENGTH)
		{
			jCommons.log(LOG_MISS, "jzzBLOCKCONV::translate_block_FROM_TEXT() : String too long.");

			return false;
		}

		pchar += nchar;
		bytes -= nchar;
	}

	pchar = (char *) &reinterpret_cast<pRawBlock>(p_source)->data;
	bytes = p_source->size;

	switch (type)
	{
		case BLOCKTYPE_C_BOOL:
			{
				bool ok = JAZZALLOC(p_dest, RAM_ALLOC_C_BOOL, length);
				if (!ok)
				{
					jCommons.log(LOG_MISS, "jzzBLOCKCONV::translate_block_FROM_TEXT() : JAZZALLOC(RAM_ALLOC_C_BOOL) failed.");

					return false;
				}
				unsigned char * p_data_dest = (unsigned char *) &reinterpret_cast<pBoolBlock>(p_dest)->data;
				int i = 0;
				while (bytes > 0)
				{
					nchar = (uintptr_t) strchrnul(pchar, '\n') - (uintptr_t) pchar + 1;
					nchar = min(nchar, bytes);

					strncpy(buff, pchar, nchar);
					buff[nchar] = 0;
					if (sscanf(buff, fmt, &p_data_dest[i]) != 1)
						p_data_dest[i] = JAZZC_NA_BOOL;

					pchar += nchar;
					bytes -= nchar;
					i++;
				}
			}
			break;

		case BLOCKTYPE_C_OFFS_CHARS:
			{
				// Allocation: string_buffer + Nx(4 bytes + 2 trailing)
				bool ok = JAZZALLOC(p_dest, RAM_ALLOC_C_OFFS_CHARS, p_source->size + sizeof(string_buffer) + 8 + 6*length);
				if (!ok)
				{
					jCommons.log(LOG_MISS, "jzzBLOCKCONV::translate_block_FROM_TEXT() : JAZZALLOC(RAM_ALLOC_C_OFFS_CHARS) failed.");

					return false;
				}
				if (!length)
				{
					memset(&reinterpret_cast<pCharBlock>(p_dest)->data, 0, p_dest->size);
					return true;
				}
				if (!format_C_OFFS_CHARS((pCharBlock) p_dest, length))
				{
					JAZZFREE(p_dest, AUTOTYPEBLOCK(p_dest));

					jCommons.log(LOG_MISS, "jzzBLOCKCONV::translate_block_FROM_TEXT() : format_C_OFFS_CHARS() failed.");

					return false;
				}
				char buff2[MAX_STRING_LENGTH + 4];
				int i = 0;
				while (bytes > 0)
				{
					nchar = (uintptr_t) strchrnul(pchar, '\n') - (uintptr_t) pchar + 1;
					nchar = min(nchar, bytes);

					strncpy(buff, pchar, nchar);
					buff[nchar] = 0;
					if (copy)
					{
						int nc = nchar == bytes ? nchar : nchar - 1;
						reinterpret_cast<pCharBlock>(p_dest)->data[i] = get_string_idx_C_OFFS_CHARS((pCharBlock) p_dest, buff, nc);
					}
					else
					{
						if (sscanf(buff, fmt, buff2) == 1)
						{
							reinterpret_cast<pCharBlock>(p_dest)->data[i] = get_string_idx_C_OFFS_CHARS((pCharBlock) p_dest, buff2, strlen(buff2));
						}
						else
						{
							reinterpret_cast<pCharBlock>(p_dest)->data[i] = JAZZC_NA_STRING;
						}
					}

					pchar += nchar;
					bytes -= nchar;
					i++;
				}
			}
			break;

		case BLOCKTYPE_C_FACTOR:
		case BLOCKTYPE_C_GRADE:
		case BLOCKTYPE_C_INTEGER:
			{
				bool ok = JAZZALLOC(p_dest, RAM_ALLOC_C_INTEGER, length);
				if (!ok)
				{
					jCommons.log(LOG_MISS, "jzzBLOCKCONV::translate_block_FROM_TEXT() : JAZZALLOC(RAM_ALLOC_C_INTEGER) failed.");

					return false;
				}
				p_dest->type = type;
				int * p_data_dest = (int *) &reinterpret_cast<pIntBlock>(p_dest)->data;
				int i = 0;
				while (bytes > 0)
				{
					nchar = (uintptr_t) strchrnul(pchar, '\n') - (uintptr_t) pchar + 1;
					nchar = min(nchar, bytes);

					strncpy(buff, pchar, nchar);
					buff[nchar] = 0;
					if (sscanf(buff, fmt, &p_data_dest[i]) != 1)
						p_data_dest[i] = JAZZC_NA_INTEGER;

					pchar += nchar;
					bytes -= nchar;
					i++;
				}
			}
			break;

		case BLOCKTYPE_C_TIMESEC:
		case BLOCKTYPE_C_REAL:
			{
				bool ok = JAZZALLOC(p_dest, RAM_ALLOC_C_REAL, length);
				if (!ok)
				{
					jCommons.log(LOG_MISS, "jzzBLOCKCONV::translate_block_FROM_TEXT() : JAZZALLOC(RAM_ALLOC_C_REAL) failed.");

					return false;
				}
				p_dest->type = type;
				double * p_data_dest = (double *) &reinterpret_cast<pRealBlock>(p_dest)->data;
				int i = 0;
				while (bytes > 0)
				{
					nchar = (uintptr_t) strchrnul(pchar, '\n') - (uintptr_t) pchar + 1;
					nchar = min(nchar, bytes);

					strncpy(buff, pchar, nchar);
					buff[nchar] = 0;
					if (sscanf(buff, fmt, &p_data_dest[i]) != 1)
						p_data_dest[i] = JAZZC_NA_DOUBLE;

					pchar += nchar;
					bytes -= nchar;
					i++;
				}
			}
			break;

		default:
			jCommons.log(LOG_MISS, "jzzBLOCKCONV::translate_block_FROM_TEXT() : Unexpected block type.");

			return false;
	}

	return true;
}


/** Create a block_C_R_RAW of type BLOCKTYPE_RAW_MIME_CSV, BLOCKTYPE_RAW_MIME_JSON, BLOCKTYPE_RAW_MIME_TSV or BLOCKTYPE_RAW_MIME_XML from a
block_C_BOOL, block_C_R_INTEGER, block_C_R_REAL or block_C_OFFS_CHARS.

	\param p_source	 The source block (A block_C_BOOL, block_C_R_INTEGER, block_C_R_REAL or block_C_OFFS_CHARS).
	\param p_dest Address of a pJazzBlock allocated by this function to store the block.
	\param fmt	 The (sprintf() compatible) format for converting the data, must be compatible with the source type.

	\return		 true if successful, false and log(LOG_MISS, "further details") if not.

	This function does nothing with the p_source object except copying it.
	The returned p_dest is owned by the caller and must be JAZZFREE()ed when no longer necessary.
*/
bool JazzCoreTypecasting::ToText (pJazzBlock p_source, pJazzBlock &p_dest, const char *fmt)
{
	char buff_item[MAX_STRING_LENGTH];
	int size = 0;

	switch (p_source->type)
	{
		case BLOCKTYPE_C_BOOL:
			for (int i = 0; i < p_source->length; i++)
			{
				if (reinterpret_cast<pBoolBlock>(p_source)->data[i] == JAZZC_NA_BOOL)
				{
					size += LENGTH_NA_AS_TEXT;
				}
				else
				{
					sprintf(buff_item, fmt, reinterpret_cast<pBoolBlock>(p_source)->data[i]);
					size += strlen(buff_item);
				}
			}
			break;

		case BLOCKTYPE_C_OFFS_CHARS:
			for (int i = 0; i < p_source->length; i++)
			{
				if (reinterpret_cast<pCharBlock>(p_source)->data[i] == JAZZC_NA_STRING)
				{
					size += LENGTH_NA_AS_TEXT;
				}
				else
				{
					sprintf(buff_item, fmt, PCHAR(reinterpret_cast<pCharBlock>(p_source), i));
					size += strlen(buff_item);
				}
			}
			break;

		case BLOCKTYPE_C_FACTOR:
		case BLOCKTYPE_C_GRADE:
		case BLOCKTYPE_C_INTEGER:
			for (int i = 0; i < p_source->length; i++)
			{
				if (reinterpret_cast<pIntBlock>(p_source)->data[i] == JAZZC_NA_INTEGER)
				{
					size += LENGTH_NA_AS_TEXT;
				}
				else
				{
					sprintf(buff_item, fmt, reinterpret_cast<pIntBlock>(p_source)->data[i]);
					size += strlen(buff_item);
				}
			}
			break;

		case BLOCKTYPE_C_TIMESEC:
		case BLOCKTYPE_C_REAL:
			for (int i = 0; i < p_source->length; i++)
			{
				if (R_IsNA(reinterpret_cast<pRealBlock>(p_source)->data[i]))
				{
					size += LENGTH_NA_AS_TEXT;
				}
				else
				{
					sprintf(buff_item, fmt, reinterpret_cast<pRealBlock>(p_source)->data[i]);
					size += strlen(buff_item);
				}
			}
			break;

		default:
			jCommons.log(LOG_MISS, "translate_block_TO_TEXT(): Unexpected block type.");

			return false;
	}

	bool ok = JAZZALLOC(p_dest, RAM_ALLOC_C_RAW, size + 8);	// Needed by sprintf() to add the extra 0.
	p_dest->size = size;

	if (!ok)
	{
		jCommons.log(LOG_MISS, "translate_block_TO_TEXT(): JAZZALLOC failed.");

		return false;

	}
	reinterpret_cast_block(p_dest, BLOCKTYPE_RAW_STRINGS);
	char * pt = (char *) &reinterpret_cast<pRawBlock>(p_dest)->data;

	switch (p_source->type)
	{
		case BLOCKTYPE_C_BOOL:
			for (int i = 0; i < p_source->length; i++)
			{
				if (reinterpret_cast<pBoolBlock>(p_source)->data[i] == JAZZC_NA_BOOL)
				{
					strcpy(pt, NA_AS_TEXT);
					pt += LENGTH_NA_AS_TEXT;
				}
				else
				{
					sprintf(pt, fmt, reinterpret_cast<pBoolBlock>(p_source)->data[i]);
					pt += strlen(pt);
				}
			}
			break;

		case BLOCKTYPE_C_OFFS_CHARS:
			for (int i = 0; i < p_source->length; i++)
			{
				if (reinterpret_cast<pCharBlock>(p_source)->data[i] == JAZZC_NA_STRING)
				{
					strcpy(pt, NA_AS_TEXT);
					pt += LENGTH_NA_AS_TEXT;
				}
				else
				{
					sprintf(pt, fmt, PCHAR(reinterpret_cast<pCharBlock>(p_source), i));
					pt += strlen(pt);
				}
			}
			break;

		case BLOCKTYPE_C_TIMESEC:
		case BLOCKTYPE_C_REAL:
			for (int i = 0; i < p_source->length; i++)
			{
				if (R_IsNA(reinterpret_cast<pRealBlock>(p_source)->data[i]))
				{
					strcpy(pt, NA_AS_TEXT);
					pt += LENGTH_NA_AS_TEXT;
				}
				else
				{
					sprintf(pt, fmt, reinterpret_cast<pRealBlock>(p_source)->data[i]);
					pt += strlen(pt);
				}
			}
			break;

		default:
			for (int i = 0; i < p_source->length; i++)
			{
				if (reinterpret_cast<pIntBlock>(p_source)->data[i] == JAZZC_NA_INTEGER)
				{
					strcpy(pt, NA_AS_TEXT);
					pt += LENGTH_NA_AS_TEXT;
				}
				else
				{
					sprintf(pt, fmt, reinterpret_cast<pIntBlock>(p_source)->data[i]);
					pt += strlen(pt);
				}
			}
	}
	pt[0] = 0;

	return true;
}

} // namespace jazz_stdcore


#if defined CATCH_TEST
#include "src/jazz_elements/tests/test_stdcore.ctest"
#endif
