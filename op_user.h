/* $Id$ */
/* COPYRIGHT (C) 2000 THE VICTORIA UNIVERSITY OF MANCHESTER and John Levon
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef OP_USER_H
#define OP_USER_H

/* stuff shared between user-space and the module */

#include "version.h"

#ifndef u8
#define u8 unsigned char
#endif
#ifndef u16
#define u16 unsigned short
#endif
#ifndef u32
#define u32 unsigned int
#endif
#ifndef uint
#define uint unsigned int
#endif
#ifndef ulong
#define ulong unsigned long
#endif
#ifndef fd_t
#define fd_t int
#endif

/* event check returns */
#define OP_EVENTS_OK		0x0
#define OP_EVT_NOT_FOUND	0x1
#define OP_EVT_NO_UM		0x2
#define OP_EVT_CTR_NOT_ALLOWED	0x4

/* supported cpu type */
#define CPU_NO_GOOD	-1
#define CPU_PPRO	0
#define CPU_PII		1
#define CPU_PIII	2
#define CPU_ATHLON	3
#define MAX_CPU_TYPE	4

#ifndef NR_CPUS 
#define NR_CPUS 32
#endif 

/* change these, you change them in op_start as well,
 * you hear ?
 */
/* 262144 * 8 = 2097152 bytes default */
#define OP_DEFAULT_BUF_SIZE 262144
/* 65536 * 32 = 2097152 bytes default */
#define OP_DEFAULT_HASH_SIZE 65536

/* kernel image entries are offset by this many entries */
#define OPD_KERNEL_OFFSET 524288

/* maximum nr. of counters, up to 4 for Athlon (18 for P4). The primary use
 * of this variable is for static/local array dimension. Never use it in loop
 * or in array index access/index checking. Don't change it without updating
 * OP_BITS_CTR! */
#define OP_MAX_COUNTERS	4

/* the number of bits neccessary to store OP_MAX_COUNTERS values */
#define OP_BITS_CTR	2

/* the number of reserved bits in count, + 1 is for the notification bit */
#define OP_BITS (OP_BITS_CTR + 1)

/* The number of bits available to store count */
#define OP_BITS_COUNT	(16 - OP_BITS)

/* counter nr mask */
#define OP_CTR_MASK	((~0U << (OP_BITS_COUNT + 1)) >> 1)

/* top OP_BITS bits of count are used as follows: */
/* is this actually a notification ? */
#define OP_NOTE		(1U << (OP_BITS_COUNT + OP_BITS_CTR))
/* which perf counter the sample is from */
#define OP_COUNTER(x)	(((x) & OP_CTR_MASK) >> OP_BITS_COUNT)

#define OP_COUNT_MASK	((1U << OP_BITS_COUNT) - 1U)

/* mapping notification types */
/* fork(),vfork(),clone() */
#define OP_FORK (OP_NOTE|(1U<<0))
/* mapping */
#define OP_MAP (OP_NOTE|(1U<<14))
/* execve() */
#define OP_EXEC (OP_NOTE|(1U<<14) | (1U<<13))
/* init_module() */
#define OP_DROP_MODULES (OP_NOTE | (1U<<1))
/* exit() */
#define OP_EXIT (OP_NOTE | (1U<<2))

#define IS_OP_MAP(v) ( \
	((v) & OP_NOTE) && \
	((v) & (1U<<14)) )

#define IS_OP_EXEC(v) ( \
	((v) & OP_NOTE) && \
	((v) & (1U<<14)) && \
	((v) & (1U<<13)) )

/* note that pid_t is 32 bits, but only 16 are used
   currently, so to save cache, we use u16 */
struct op_sample {
	u16 count;
	u16 pid;
	u32 eip;
} __attribute__((__packed__, __aligned__(8)));

#ifndef __ok_unused
#define __ok_unused __attribute((__unused))
#endif

/* nr. entries in hash map, prime
 * this is the maximum number of name components allowed
 * This is the maximal value we have bits for
 */
#define OP_HASH_MAP_NR 4093

/* size of hash map entries */
#define OP_HASH_LINE 128

struct op_hash {
	char name[OP_HASH_LINE];
	u16 parent;
} __attribute__((__packed__));

/* temporary mapping structure */
struct op_mapping {
	u16 pid;
	u32 addr;
	u32 len;
	u32 offset;
	short hash;
	int is_execve; 
};

/* size of hash map in bytes */
#define OP_HASH_MAP_SIZE (OP_HASH_MAP_NR * sizeof(struct op_hash))

/* op_events.c */
int op_min_count(u8 ctr_type, int cpu_type);
int op_check_events(int ctr, u8 ctr_type, u8 ctr_um, int cpu_type);
const char* op_get_cpu_type_str(int cpu_type);
/* not used currently */
int op_check_events_str(int ctr, char *ctr_type, u8 ctr_um, int cpu_type, u8 *ctr_t);
void op_get_event_desc(int cpu_type, u8 type, u8 um, char **typenamep, char **typedescp, char **umdescp);
int op_get_cpu_type(void);

#endif /* OP_USER_H */
