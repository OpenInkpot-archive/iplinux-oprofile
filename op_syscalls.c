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

#include <linux/sched.h>
#include <linux/unistd.h>
#include <linux/mman.h>
#include <linux/file.h>
#include <linux/wrapper.h>

#include <asm/io.h>

#include "oprofile.h"

extern u32 oprof_ready[NR_CPUS];
extern wait_queue_head_t oprof_wait;
extern pid_t pid_filter;
extern pid_t pgrp_filter;
extern u32 prof_on;

/* FIXME: can remove this code altogether if UP oopser makes mainline, and insist on
 * CONFIG_X86_UP_APIC
 */
#ifndef CONFIG_X86_UP_APIC

#ifndef APIC_DEFAULT_PHYS_BASE
#define APIC_DEFAULT_PHYS_BASE 0xfee00000
#endif
static void set_pte_phys(ulong vaddr, ulong phys)
{
	pgprot_t prot;
	pgd_t *pgd;
	pmd_t *pmd;
	pte_t *pte;

	pgd = pgd_offset_k(vaddr);
	pmd = pmd_offset(pgd, vaddr);
	pte = pte_offset(pmd, vaddr);
	prot = PAGE_KERNEL;
	if (test_bit(X86_FEATURE_PGE, &boot_cpu_data.x86_capability))
		pgprot_val(prot) |= _PAGE_GLOBAL;
	set_pte(pte, mk_pte_phys(phys, prot));
	__flush_tlb_one(vaddr);
}

void my_set_fixmap(void)
{
	ulong address = __fix_to_virt(FIX_APIC_BASE);

	set_pte_phys (address, APIC_DEFAULT_PHYS_BASE);
}
#endif /* CONFIG_X86_UP_APIC */

/* Given PGD from the address space's page table, return the kernel
 * virtual mapping of the physical memory mapped at ADR.
 */
static inline unsigned long uvirt_to_kva(pgd_t *pgd, unsigned long adr)
{
	unsigned long ret = 0UL;
	pmd_t *pmd;
	pte_t *ptep, pte;

	if (!pgd_none(*pgd)) {
		pmd = pmd_offset(pgd, adr);
		if (!pmd_none(*pmd)) {
			ptep = pte_offset(pmd, adr);
			pte = *ptep;
			if(pte_present(pte)) {
				ret = (unsigned long) page_address(pte_page(pte));
				ret |= (adr & (PAGE_SIZE - 1));
			}
		}
	}
	return ret;
}

/* Here we want the physical address of the memory.
 * This is used when initializing the contents of the
 * area and marking the pages as reserved.
 */
static inline unsigned long kvirt_to_pa(unsigned long adr)
{
	unsigned long va, kva, ret;

	va = VMALLOC_VMADDR(adr);
	kva = uvirt_to_kva(pgd_offset_k(va), va);
	ret = __pa(kva);
	return ret;
}

static void * rvmalloc(signed long size)
{
	void * mem;
	unsigned long adr, page;

	mem=vmalloc_32(size);
	if (!mem)
		return NULL;

	memset(mem, 0, size);
	
	adr=(unsigned long) mem;
	while (size > 0) {
		page = kvirt_to_pa(adr);
		mem_map_reserve(virt_to_page(__va(page)));
		adr += PAGE_SIZE;
		size -= PAGE_SIZE;
	}
	return mem;
}

static void rvfree(void * mem, signed long size)
{
	unsigned long adr, page;

	if (!mem)
		return;
		
	adr=(unsigned long) mem;
	while (size > 0) {
		page = kvirt_to_pa(adr);
		mem_map_unreserve(virt_to_page(__va(page)));
		adr += PAGE_SIZE;
		size -= PAGE_SIZE;
	}
	vfree(mem);
}

static uint dname_top;
static struct qstr **dname_stack;
static uint hash_map_open;
static struct op_hash *hash_map;

void oprof_put_note(struct op_sample *samp);
void oprof_put_mapping(struct op_mapping *mapping);

/* --------- device routines ------------- */

int is_map_ready(void)
{
	return hash_map_open;
}

int oprof_init_hashmap(void)
{
	dname_stack = kmalloc(DNAME_STACK_MAX * sizeof(struct qstr *), GFP_KERNEL);
	if (!dname_stack)
		return -EFAULT;
	dname_top = 0;

	hash_map = rvmalloc(PAGE_ALIGN(OP_HASH_MAP_SIZE));
	if (!hash_map)
		return -EFAULT;
	return 0;
}

void oprof_free_hashmap(void)
{
	kfree(dname_stack);
	rvfree(hash_map, PAGE_ALIGN(OP_HASH_MAP_SIZE));
}

int oprof_hash_map_open(void)
{
	if (test_and_set_bit(0, &hash_map_open))
		return -EBUSY;

	return 0;
}

int oprof_hash_map_release(void)
{
	if (!hash_map_open)
		return -EFAULT;

	clear_bit(0,&hash_map_open);
	return 0;
}

int oprof_hash_map_mmap(struct file *file, struct vm_area_struct *vma)
{
	ulong start = (ulong)vma->vm_start;
	ulong page, pos;
	ulong size = (ulong)(vma->vm_end-vma->vm_start);

	if (size > PAGE_ALIGN(OP_HASH_MAP_SIZE) || (vma->vm_flags & VM_WRITE) || vma->vm_pgoff)
		return -EINVAL;

	pos = (ulong)hash_map;
	while (size > 0) {
		page = kvirt_to_pa(pos);
		if (remap_page_range(start, page, PAGE_SIZE, PAGE_SHARED))
			return -EAGAIN;
		start += PAGE_SIZE;
		pos += PAGE_SIZE;
		size -= PAGE_SIZE;
	}
	return 0;
}

/* ------------ system calls --------------- */

struct mmap_arg_struct {
        unsigned long addr;
        unsigned long len;
        unsigned long prot;
        unsigned long flags;
        unsigned long fd;
        unsigned long offset;
};

asmlinkage static int (*old_sys_fork)(struct pt_regs);
asmlinkage static int (*old_sys_vfork)(struct pt_regs);
asmlinkage static int (*old_sys_clone)(struct pt_regs);
asmlinkage static int (*old_sys_execve)(struct pt_regs);
asmlinkage static int (*old_old_mmap)(struct mmap_arg_struct *);
asmlinkage static long (*old_sys_mmap2)(ulong, ulong, ulong, ulong, ulong, ulong);
asmlinkage static long (*old_sys_init_module)(const char *, struct module *);
asmlinkage static long (*old_sys_exit)(int);

spinlock_t map_lock = SPIN_LOCK_UNLOCKED;

inline static uint name_hash(const char *name, uint len)
{
	uint hash=0;

	while (len--)
		hash = ((hash << 4) | (hash >> 28)) ^ name[len];

	return hash % OP_HASH_MAP_NR;
}

/* empty ascending dname stack */
inline static void push_dname(struct qstr *dname)
{
	dname_stack[dname_top] = dname;
	if (dname_top != DNAME_STACK_MAX)
		dname_top++;
}

inline static struct qstr *pop_dname(void)
{
	if (dname_top == 0)
		return NULL;

	return dname_stack[--dname_top];
}

/* called with map_lock held */
static short do_hash(struct dentry *dentry, struct vfsmount *vfsmnt, struct dentry *root, struct vfsmount *rootmnt)
{
	struct dentry *d = dentry;
	struct vfsmount *v = vfsmnt;
	struct qstr *dname;
	short value = -1;
	short firsthash;
	short probe = 3;
	short parent = 0;

	/* wind the dentries onto the stack pages */
	for (;;) {
		if (d->d_name.len > OP_HASH_LINE)
			goto too_large;

		/* deleted ? */
		if (!IS_ROOT(d) && list_empty(&d->d_hash))
			goto out;

		/* the root */
		if (d == root && v == rootmnt)
			break;

		if (d == v->mnt_root || IS_ROOT(d)) {
			if (v->mnt_parent == v)
				break;
			/* cross the mount point */
			d = v->mnt_mountpoint;
			v = v->mnt_parent;
		}

		push_dname(&d->d_name);

		d = d->d_parent;
	}

	/* here we are at the bottom, unwind and hash */

	while ((dname = pop_dname())) {
		firsthash = value = name_hash(dname->name, dname->len);

	retry:
		/* new entry ? */
		if (hash_map[value].name[0] == '\0') {
			strcpy(hash_map[value].name, dname->name);
			hash_map[value].parent = parent;
			goto next;
		}

		/* existing entry ? */
		if (streqn(hash_map[value].name, dname->name, dname->len)
			&& hash_map[value].parent == parent)
			goto next;

		/* nope, find another place in the table */
		value = (value + probe) % OP_HASH_MAP_NR;
		probe *= probe;
		if (value == firsthash)
			goto fulltable;

		goto retry;
	next:
		parent = value;
	}

out:
	dname_top = 0;
	return value;
fulltable:
	printk(KERN_ERR "oprofile: component hash table full :(\n");
	value = -1;
	goto out;
too_large:
	printk(KERN_ERR "oprofile: component %s length too large (%d > %d) !\n",
		dentry->d_name.name, dentry->d_name.len, OP_HASH_LINE);
	goto out;
}

/* called with map_lock held */
static short do_path_hash(struct dentry *dentry, struct vfsmount *vfsmnt)
{
	short value;
	struct vfsmount *rootmnt;
	struct dentry *root;

	read_lock(&current->fs->lock);
	rootmnt = mntget(current->fs->rootmnt);
	root = dget(current->fs->root);
	read_unlock(&current->fs->lock);

	spin_lock(&dcache_lock);

	value = do_hash(dentry, vfsmnt, root, rootmnt);

	spin_unlock(&dcache_lock);
	dput(root);
	mntput(rootmnt);
	return value;
}

/* called with map_lock held */
static void oprof_output_map(ulong addr, ulong len,
	ulong offset, struct file *file, int is_execve)
{
	struct op_mapping mapping;

	if (!prof_on)
		return;

	/* don't bother with /dev/zero mappings etc. */
	if (!len)
		return;

	mapping.pid = current->pid;
	mapping.addr = addr;
	mapping.len = len;
	mapping.offset = offset;
	mapping.is_execve = is_execve;
	mapping.hash = do_path_hash(file->f_dentry, file->f_vfsmnt);
	if (mapping.hash == -1)
		return;
	oprof_put_mapping(&mapping);
}

static int oprof_output_maps(struct task_struct *task)
{
	int size=0;
	int is_execve = 1;
	struct mm_struct *mm;
	struct vm_area_struct *map;

	/* we don't need to worry about mm_users here, since there is at
	   least one user (current), and if there's other code using this
	   mm, then mm_users must be at least 2; we should never have to
	   mmput() here. */

	if (!(mm = task->mm))
		goto out;


	take_mmap_sem(mm);
	spin_lock(&map_lock);
	for (map = mm->mmap; map; map = map->vm_next) {
		if (!(map->vm_flags & VM_EXEC) || !map->vm_file)
			continue;

		oprof_output_map(map->vm_start, map->vm_end-map->vm_start,
			map->vm_pgoff << PAGE_SHIFT, map->vm_file, is_execve);
		is_execve = 0;
	}
	spin_unlock(&map_lock);
	release_mmap_sem(mm);

out:
	return size;
}

asmlinkage static int my_sys_execve(struct pt_regs regs)
{
	char *filename;
	int ret;

	MOD_INC_USE_COUNT;

	filename = getname((char *)regs.ebx);
	if (IS_ERR(filename)) {
		MOD_DEC_USE_COUNT;
		return PTR_ERR(filename);
	}
	ret = do_execve(filename, (char **)regs.ecx, (char **)regs.edx, &regs);

	if (!ret) {
		current->ptrace &= ~PT_DTRACE;

		if ((!pid_filter || pid_filter == current->pid) &&
		    (!pgrp_filter || pgrp_filter == current->pgrp))
			oprof_output_maps(current);
	}
	putname(filename);
	MOD_DEC_USE_COUNT;
        return ret;
}

static void out_mmap(ulong addr, ulong len, ulong prot, ulong flags,
	ulong fd, ulong offset)
{
	struct file *file;

	MOD_INC_USE_COUNT;

	file = fget(fd);
	if (!file)
		return;

	spin_lock(&map_lock);
	oprof_output_map(addr, len, offset, file, 0);
	spin_unlock(&map_lock);

	fput(file);
}

asmlinkage static int my_sys_mmap2(ulong addr, ulong len,
	ulong prot, ulong flags, ulong fd, ulong pgoff)
{
	int ret;

	MOD_INC_USE_COUNT;

	ret = old_sys_mmap2(addr, len, prot, flags, fd, pgoff);

	if ((pid_filter && current->pid != pid_filter) ||
	    (pgrp_filter && current->pgrp != pgrp_filter))
		goto out;

	if ((prot&PROT_EXEC) && ret >= 0)
		out_mmap(ret, len, prot, flags, fd, pgoff << PAGE_SHIFT);
	goto out;
out:
	MOD_DEC_USE_COUNT;
	return ret;
}

asmlinkage static int my_old_mmap(struct mmap_arg_struct *arg)
{
	int ret;

	MOD_INC_USE_COUNT;

	ret = old_old_mmap(arg);

	if ((pid_filter && current->pid != pid_filter) ||
	    (pgrp_filter && current->pgrp != pgrp_filter))
		goto out;

	if (ret>=0) {
		struct mmap_arg_struct a;

		if (copy_from_user(&a, arg, sizeof(a)))
			goto out;

		if (a.prot&PROT_EXEC)
			out_mmap(ret, a.len, a.prot, a.flags, a.fd, a.offset);
	}
out:
	MOD_DEC_USE_COUNT;
	return ret;
}

inline static void oprof_report_fork(u16 old, u32 new)
{
	struct op_sample samp;

	if (pgrp_filter && pgrp_filter != current->pgrp)
		return;

	samp.count = OP_FORK;
	samp.pid = old;
	samp.eip = new;
	oprof_put_note(&samp);
}

asmlinkage static int my_sys_fork(struct pt_regs regs)
{
	u16 pid = (u16)current->pid;
	int ret;

	MOD_INC_USE_COUNT;

	ret = old_sys_fork(regs);
	if (ret)
		oprof_report_fork(pid,ret);
	MOD_DEC_USE_COUNT;
	return ret;
}

asmlinkage static int my_sys_vfork(struct pt_regs regs)
{
	u16 pid = (u16)current->pid;
	int ret;

	MOD_INC_USE_COUNT;
	ret = old_sys_vfork(regs);
	if (ret)
		oprof_report_fork(pid,ret);
	MOD_DEC_USE_COUNT;
	return ret;
}

asmlinkage static int my_sys_clone(struct pt_regs regs)
{
	u16 pid = (u16)current->pid;
	int ret;

	MOD_INC_USE_COUNT;
	ret = old_sys_clone(regs);
	if (ret)
		oprof_report_fork(pid,ret);
	MOD_DEC_USE_COUNT;
	return ret;
}

asmlinkage static long my_sys_init_module(const char *name_user, struct module *mod_user)
{
	long ret;

	MOD_INC_USE_COUNT;

	ret = old_sys_init_module(name_user, mod_user);

	if (ret >= 0) {
		struct op_sample samp;

		samp.count = OP_DROP_MODULES;
		samp.pid = 0;
		samp.eip = 0;
		oprof_put_note(&samp);
	}
	MOD_DEC_USE_COUNT;
	return ret;
}

asmlinkage static long my_sys_exit(int error_code)
{
	struct op_sample samp;

	MOD_INC_USE_COUNT;

	if ((pid_filter && current->pid != pid_filter) ||
	    (pgrp_filter && current->pgrp != pgrp_filter))
		goto out;

	samp.count = OP_EXIT;
	samp.pid = current->pid;
	samp.eip = 0;
	oprof_put_note(&samp);

	goto out;
out:
	MOD_DEC_USE_COUNT;
	return old_sys_exit(error_code);
}

extern void *sys_call_table[];

void op_save_syscalls(void)
{
	old_sys_fork = sys_call_table[__NR_fork];
	old_sys_vfork = sys_call_table[__NR_vfork];
	old_sys_clone = sys_call_table[__NR_clone];
	old_sys_execve = sys_call_table[__NR_execve];
	old_old_mmap = sys_call_table[__NR_mmap];
	old_sys_mmap2 = sys_call_table[__NR_mmap2];
	old_sys_init_module = sys_call_table[__NR_init_module];
	old_sys_exit = sys_call_table[__NR_exit];
}

void op_intercept_syscalls(void)
{
	sys_call_table[__NR_fork] = my_sys_fork;
	sys_call_table[__NR_vfork] = my_sys_vfork;
	sys_call_table[__NR_clone] = my_sys_clone;
	sys_call_table[__NR_execve] = my_sys_execve;
	sys_call_table[__NR_mmap] = my_old_mmap;
	sys_call_table[__NR_mmap2] = my_sys_mmap2;
	sys_call_table[__NR_init_module] = my_sys_init_module;
	sys_call_table[__NR_exit] = my_sys_exit;
}

void op_replace_syscalls(void)
{
	sys_call_table[__NR_fork] = old_sys_fork;
	sys_call_table[__NR_vfork] = old_sys_vfork;
	sys_call_table[__NR_clone] = old_sys_clone;
	sys_call_table[__NR_execve] = old_sys_execve;
	sys_call_table[__NR_mmap] = old_old_mmap;
	sys_call_table[__NR_mmap2] = old_sys_mmap2;
	sys_call_table[__NR_init_module] = old_sys_init_module;
	sys_call_table[__NR_exit] = old_sys_exit;
}
