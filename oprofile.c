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

/* FIXME: data->next rotation ? */
/* FIXME: with generation numbers we can place mappings in
   every buffer. we still need one IPI, but we shouldn't need
   to wait for it (?) and it avoids mapping-in-other-CPU problem */

#include "oprofile.h"

EXPORT_NO_SYMBOLS;

static char *op_version = VERSION_STRING;
MODULE_AUTHOR("John Levon (moz@compsoc.man.ac.uk)");
MODULE_DESCRIPTION("Continuous Profiling Module");

/* sysctl settables */
static int op_hash_size;
static int op_buf_size;
static int sysctl_dump;
static int op_ctr0_on[NR_CPUS];
static int op_ctr1_on[NR_CPUS];
static int op_ctr0_um[NR_CPUS];
static int op_ctr1_um[NR_CPUS];
static int op_ctr0_count[NR_CPUS];
static int op_ctr1_count[NR_CPUS];
static int op_ctr0_val[NR_CPUS];
static int op_ctr1_val[NR_CPUS];
static int op_ctr0_kernel[NR_CPUS];
static int op_ctr0_user[NR_CPUS];
static int op_ctr1_kernel[NR_CPUS];
static int op_ctr1_user[NR_CPUS];
pid_t pid_filter;
pid_t pgrp_filter;

u32 prof_on __cacheline_aligned;

static int op_major;
static int cpu_type;

static volatile uint oprof_opened __cacheline_aligned;
static DECLARE_WAIT_QUEUE_HEAD(oprof_wait);

u32 oprof_ready[NR_CPUS] __cacheline_aligned;
static struct _oprof_data oprof_data[NR_CPUS];

extern spinlock_t map_lock;
extern u32 nextmapbuf;

/* ---------------- NMI handler ------------------ */

/* FIXME: this whole handler would probably be better in straight asm */
static void evict_op_entry(struct _oprof_data *data, struct op_sample *ops)
{
	memcpy(&data->buffer[data->nextbuf],ops,sizeof(struct op_sample));
	if (++data->nextbuf!=(data->buf_size-OP_PRE_WATERMARK)) {
		if (data->nextbuf==data->buf_size)
			data->nextbuf=0;
		return;
	}
	oprof_ready[smp_processor_id()] = 1;
}

inline static void fill_op_entry(struct op_sample *ops, struct pt_regs *regs, int ctr)
{
	ops->eip = regs->eip;
	ops->pid = current->pid;
	ops->count = OP_COUNTER*ctr + 1;
}

#define op_full_count(c) (((c)&OP_MAX_COUNT)==OP_MAX_COUNT)

/* no check for ctr needed as one of the three will differ in the hash */
#define op_miss(ops)  \
	((ops).eip!=regs->eip || \
	(ops).pid!=current->pid || \
	op_full_count((ops).count))

/* the top half of pid is likely to remain static,
   so it's masked off. the ctr bit is used to separate
   the two counters */
#define op_hash(eip,pid,ctr) \
	((((((eip&0xff000)>>3)^eip)^(pid&0xff))^(eip<<9)) \
	^ (ctr<<8)) & (data->hash_size-1)

inline static void op_do_profile(struct _oprof_data *data, struct pt_regs *regs, int ctr)
{
	uint h = op_hash(regs->eip,current->pid,ctr);
	uint i;

	/* FIXME: can we remove new sample check by pretending to be full ? */
	for (i=0; i < OP_NR_ENTRY; i++) {
		if (!op_miss(data->entries[h].samples[i])) {
			data->entries[h].samples[i].count++;
			set_perfctr(data->ctr_count[ctr],ctr);
			return;
		} else if (op_full_count(data->entries[h].samples[i].count)) {
			goto full_entry;
		} else if (!data->entries[h].samples[i].count)
			goto new_entry;
	}

	evict_op_entry(data,&data->entries[h].samples[data->next]);
	fill_op_entry(&data->entries[h].samples[data->next],regs,ctr);
	data->next = (data->next+1) % OP_NR_ENTRY;
out:
	set_perfctr(data->ctr_count[ctr],ctr);
	return;
full_entry:
	evict_op_entry(data,&data->entries[h].samples[i]);
	data->entries[h].samples[i].count = OP_COUNTER*ctr + 1;
	goto out;
new_entry:
	fill_op_entry(&data->entries[h].samples[i],regs,ctr);
	goto out;
}

static int op_check_ctr(struct _oprof_data *data, struct pt_regs *regs, int ctr)
{
	ulong l,h;
	get_perfctr(l,h,ctr);
	if (ctr_overflowed(l)) {
		op_do_profile(data,regs,ctr);
		return 1;
	}
	return 0;
}

asmlinkage void op_do_nmi(struct pt_regs * regs)
{
	struct _oprof_data *data = &oprof_data[smp_processor_id()];
	uint low,high;
	int overflowed=0;

#ifdef PID_FILTER
	if (pid_filter && current->pid!=pid_filter)
		return;
	if (pgrp_filter && current->pgrp!=pgrp_filter)
		return;
#endif

	/* disable counters */
	rdmsr(P6_MSR_EVNTSEL0,low,high);
	wrmsr(P6_MSR_EVNTSEL0,low&~(1<<22),high);

	if (data->ctrs&OP_CTR_0)
		overflowed = op_check_ctr(data,regs,0);
	if (data->ctrs&OP_CTR_1)
		overflowed |= op_check_ctr(data,regs,1);
		
	/* enable counters */
	rdmsr(P6_MSR_EVNTSEL0,low,high);
	wrmsr(P6_MSR_EVNTSEL0,low|(1<<22),high);

	if (!overflowed)
		DO_NMI(regs, 0);
}

/* ---------------- NMI handler setup ------------ */

void mask_LVT_NMIs(void)
{
	ulong v;

	/* LVT0,1,PC can generate NMIs on APIC */
	v = apic_read(APIC_LVT0);
	apic_write(APIC_LVT0, v | APIC_LVT_MASKED);
	v = apic_read(APIC_LVT1);
	apic_write(APIC_LVT1, v | APIC_LVT_MASKED);
        v = apic_read(APIC_LVTPC);
        apic_write(APIC_LVTPC, v | APIC_LVT_MASKED);
}

void unmask_LVT_NMIs(void)
{
	ulong v;

	v = apic_read(APIC_LVT0);
	apic_write(APIC_LVT0, v & ~APIC_LVT_MASKED);
	v = apic_read(APIC_LVT1);
	apic_write(APIC_LVT1, v & ~APIC_LVT_MASKED);
        v = apic_read(APIC_LVTPC);
        apic_write(APIC_LVTPC, v & ~APIC_LVT_MASKED);
}

static ulong idt_addr;
static ulong kernel_nmi;

static void install_nmi(void)
{
	volatile struct _descr descr = { 0, 0,};
	volatile struct _idt_descr *de;

	mask_LVT_NMIs();
	store_idt(descr);
	idt_addr = descr.base;
	de = (struct _idt_descr *)idt_addr;
	/* NMI handler is at idt_table[2] */
	de += 2;
	/* see Intel Vol.3 Figure 5-2, interrupt gate */
	kernel_nmi = (de->a & 0xffff) | (de->b & 0xffff0000);

	_set_gate(de,14,0,&op_nmi);
	unmask_LVT_NMIs();
}

static void restore_nmi(void)
{
	mask_LVT_NMIs();
	_set_gate(((char *)(idt_addr))+16,14,0,kernel_nmi);
	unmask_LVT_NMIs();
}

/* ---------------- APIC setup ------------------ */

static void disable_local_P6_APIC(void *dummy)
{
	ulong v;
	uint l,h;

	/* FIXME: maybe this should go at end of function ? */
	/* first disable via MSR */
	/* IA32 V3, 7.4.2 */
	rdmsr(MSR_APIC_BASE, l, h);
	wrmsr(MSR_APIC_BASE, l&~(1<<11), h);

	/*
	 * Careful: we have to set masks only first to deassert
	 * any level-triggered sources.
	 */
	v = apic_read(APIC_LVTT);
	apic_write(APIC_LVTT, v | APIC_LVT_MASKED);
	v = apic_read(APIC_LVT0);
	apic_write(APIC_LVT0, v | APIC_LVT_MASKED);
	v = apic_read(APIC_LVT1);
	apic_write(APIC_LVT1, v | APIC_LVT_MASKED);
	v = apic_read(APIC_LVTERR);
	apic_write(APIC_LVTERR, v | APIC_LVT_MASKED);
        v = apic_read(APIC_LVTPC);
        apic_write(APIC_LVTPC, v | APIC_LVT_MASKED);

	/*
	 * Clean APIC state for other OSs:
	 */
	apic_write(APIC_LVTT, APIC_LVT_MASKED);
	apic_write(APIC_LVT0, APIC_LVT_MASKED);
	apic_write(APIC_LVT1, APIC_LVT_MASKED);
	apic_write(APIC_LVTERR, APIC_LVT_MASKED);
        apic_write(APIC_LVTPC, APIC_LVT_MASKED);

	v = apic_read(APIC_SPIV);
	v &= ~(1<<8);
	apic_write(APIC_SPIV, v);

	printk(KERN_INFO "oprofile: disabled local APIC.\n");
}

static void __init smp_apic_setup(void *dummy)
{
	uint val;

	/* set up LVTPC as we need it */
	/* IA32 V3, Figure 7.8 */
	val = apic_read(APIC_LVTPC);
	/* allow PC overflow interrupts */
	val &= ~(1<<16);
	/* set delivery to NMI */
	val |= (1<<10);
	val &= ~(1<<9);
	val &= ~(1<<8);
	apic_write(APIC_LVTPC, val);
}

static int __init apic_setup(void)
{
	uint msr_low, msr_high;
	uint val;

	/* FIXME: davej says it might be possible to use PCI to find
	   SMP systems with one CPU */
	if (smp_num_cpus>1) {
		smp_apic_setup(NULL);
		return 0;
	}

	/* map the real local APIC back in */
	/* current kernels fake this on boot setup for UP */
	my_set_fixmap();

	/* FIXME: NMI delivery for SMP ? */

	/* enable local APIC via MSR. Forgetting this is a fun way to
	   lock the box */
	/* IA32 V3, 7.4.2 */
	rdmsr(MSR_APIC_BASE, msr_low, msr_high);
	wrmsr(MSR_APIC_BASE, msr_low|(1<<11), msr_high);

	/* check for a good APIC */
	/* IA32 V3, 7.4.15 */
	val = apic_read(APIC_LVR);
	if (!APIC_INTEGRATED(GET_APIC_VERSION(val)))	
		goto not_local_p6_apic;

	/* LVT0,LVT1,LVTT,LVTPC */
	if (GET_APIC_MAXLVT(apic_read(APIC_LVR))!=4)
		goto not_local_p6_apic;

	__cli();

	/* enable APIC locally */
	/* IA32 V3, 7.4.14.1 */
	val = apic_read(APIC_SPIV);
	apic_write(APIC_SPIV, val|(1<<8));

	/* FIXME: examine this stuff */
	val = 0x00008700;
	apic_write(APIC_LVT0, val);

	val = 0x00000400;
	apic_write(APIC_LVT1, val);

	/* clear error register */
	/* IA32 V3, 7.4.17 */
	apic_write(APIC_ESR, 0);

	/* mask error interrupt */
	/* IA32 V3, Figure 7.8 */
	val = apic_read(APIC_LVTERR);
	val |= 0x00010000;
	apic_write(APIC_LVTERR, val);

	/* setup timer vector */
	/* IA32 V3, 7.4.8 */
	apic_write(APIC_LVTT, 0x0001031);

	/* Divide configuration register */
	val = APIC_TDR_DIV_1;
	apic_write(APIC_TDCR, val);

	smp_apic_setup(NULL);

	__sti();

	printk(KERN_INFO "oprofile: enabled local APIC\n");
	return 0;

not_local_p6_apic:
	printk(KERN_ERR "oprofile: no local P6 APIC\n");
	/* IA32 V3, 7.4.2 */
	rdmsr(MSR_APIC_BASE, msr_low, msr_high);
	wrmsr(MSR_APIC_BASE, msr_low&~(1<<11), msr_high);
	return -ENODEV;
}

/* ---------------- PMC setup ------------------ */

static void pmc_fill_in(uint *val, u8 kernel, u8 user, u8 event, u8 um)
{
	/* enable interrupt generation */
	*val |= (1<<20);
	/* enable chosen OS and USR counting */
	if (user) 
		*val |= (1<<16);
	if (kernel) 
		*val |= (1<<17);
	/* what are we counting ? */
	*val |= event;
	*val |= (um<<8);
}

static void pmc_setup(void *dummy)
{
	uint low,high;
	uint cpu = smp_processor_id();

	/* IA Vol. 3 Figure 14-3 */

	rdmsr(P6_MSR_EVNTSEL0,low,high);
	/* clear */
	low &= (1<<21);

	if (op_ctr0_val[cpu]) {
		set_perfctr(op_ctr0_count[cpu],0);
		pmc_fill_in(&low, op_ctr0_kernel[cpu], op_ctr0_user[cpu], op_ctr0_val[cpu], op_ctr0_um[cpu]);
	}

	wrmsr(P6_MSR_EVNTSEL0,low,0);

	rdmsr(P6_MSR_EVNTSEL1,low,high);
	/* clear */
	low &= (3<<21);

	if (op_ctr1_val[cpu]) {
		set_perfctr(op_ctr1_count[cpu],1);
		pmc_fill_in(&low, op_ctr1_kernel[cpu], op_ctr1_user[cpu], op_ctr1_val[cpu], op_ctr1_um[cpu]);
	}

	wrmsr(P6_MSR_EVNTSEL1,low,high);
}

static void pmc_start(void *info)
{
	uint low,high;

	if (info && (*((uint *)info)!=smp_processor_id()))
		return;

 	/* enable counters */
	rdmsr(P6_MSR_EVNTSEL0,low,high);
	wrmsr(P6_MSR_EVNTSEL0,low|(1<<22),high);
}

static void pmc_stop(void *info)
{
	uint low,high;

	if (info && (*((uint *)info)!=smp_processor_id()))
		return;

	/* disable counters */
	rdmsr(P6_MSR_EVNTSEL0,low,high);
	wrmsr(P6_MSR_EVNTSEL0,low&~(1<<22),high);
}

inline static void pmc_select_start(uint cpu)
{
	if (cpu==smp_processor_id())
		pmc_start(NULL);
	else
		smp_call_function(pmc_start,&cpu,0,1);
}

inline static void pmc_select_stop(uint cpu)
{
	if (cpu==smp_processor_id())
		pmc_stop(NULL);
	else
		smp_call_function(pmc_stop,&cpu,0,1);
}

/* ---------------- driver routines ------------------ */

u32 diethreaddie;
pid_t threadpid;
DECLARE_MUTEX_LOCKED(threadstopsem);

/* we have to have another thread because we can't
 * do wake_up() from NMI due to no locking
 */
int oprof_thread(void *arg)
{
	int i;

	threadpid = current->pid;

	lock_kernel();
	daemonize(); 
	sprintf(current->comm, "oprof-thread");
	siginitsetinv(&current->blocked, sigmask(SIGKILL));
        spin_lock(&current->sigmask_lock);
        flush_signals(current);
        spin_unlock(&current->sigmask_lock);
	current->policy = SCHED_OTHER;
	current->nice = -20;
	unlock_kernel();

	for (;;) {
		for (i=0; i<smp_num_cpus; i++) {
			if (oprof_ready[i])
				wake_up(&oprof_wait);
		}
		current->state = TASK_INTERRUPTIBLE;
		/* FIXME: determine best value here */
		schedule_timeout(HZ/20);

		if (diethreaddie)
			break;
	}

	up(&threadstopsem);
	return 0;
}

void oprof_start_thread(void)
{
	if (kernel_thread(oprof_thread, NULL, CLONE_FS|CLONE_FILES|CLONE_SIGHAND)<0)
		printk(KERN_ERR "oprofile: couldn't spawn wakeup thread.\n");
}

void oprof_stop_thread(void)
{
	diethreaddie = 1;
	kill_proc(SIGKILL, threadpid, 1);
	down(&threadstopsem);
}

spinlock_t note_lock __cacheline_aligned = SPIN_LOCK_UNLOCKED;

void oprof_put_note(struct op_sample *samp)
{
	struct _oprof_data *data = &oprof_data[0];

	if (!prof_on)
		return;

	/* FIXME: IPIs are expensive */
	spin_lock(&note_lock);
	pmc_select_stop(0);

	memcpy(&data->buffer[data->nextbuf],samp,sizeof(struct op_sample));
	if (++data->nextbuf==(data->buf_size-OP_PRE_WATERMARK)) {
		oprof_ready[0] = 1;
		// FIXME: this ok under a spinlock ?
		wake_up(&oprof_wait);
	} else if (data->nextbuf==data->buf_size)
		data->nextbuf=0;

	pmc_select_start(0);
	spin_unlock(&note_lock);
}

static int oprof_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	struct op_sample *mybuf;
	uint i;
	uint num;
	ssize_t max;
	int forced_wakeup;

	if (!capable(CAP_SYS_PTRACE))
		return -EPERM;

	if (!prof_on) {
		kill_proc(SIGKILL, current->pid, 1);
		return -EINTR;
	}

	switch (MINOR(file->f_dentry->d_inode->i_rdev)) {
		case 2: return oprof_map_read(buf,count,ppos);
		case 1: return -EINVAL;
		default:
	}

	max = sizeof(struct op_sample)*op_buf_size;

	if (*ppos || count!=sizeof(struct op_sample)+max)
		return -EINVAL;

	mybuf = vmalloc(sizeof(struct op_sample)+max);
	if (!mybuf)
		return -EFAULT;

again:
	for (i=0; i<smp_num_cpus; i++) {
		if (oprof_ready[i]) {
			forced_wakeup = oprof_ready[i]-1;
			oprof_ready[i]=0;
			goto doit;
		}
	}

	interruptible_sleep_on(&oprof_wait);
	if (signal_pending(current)) {
		vfree(mybuf);
		return -EINTR;
	}
	goto again;

	/* FIXME: what if a signal occurs now ? What is returned to
	 * the read() routine ?
	 */
doit:
	pmc_select_stop(i);
	spin_lock(&note_lock);

	num = oprof_data[i].nextbuf;
	/* might have overflowed from map buffer or ejection buffer */
	if (num < oprof_data[i].buf_size-OP_PRE_WATERMARK && !forced_wakeup) {
		printk(KERN_ERR "oprofile: Detected overflow of size %d. You must increase the "
				"hash table size or reduce the interrupt frequency\n", num);
		num = oprof_data[i].buf_size;
	} else
		oprof_data[i].nextbuf=0;

	count = num*sizeof(struct op_sample);

	if (count)
		memcpy(mybuf,oprof_data[i].buffer,count);

	spin_unlock(&note_lock);
	pmc_select_start(i);

	if (count && copy_to_user(buf, mybuf, count))
		count = -EFAULT;

	vfree(mybuf);
	return count;
}

static int oprof_start(void);
static int oprof_stop(void);

static int oprof_open(struct inode *ino, struct file *file)
{
	if (!capable(CAP_SYS_PTRACE))
		return -EPERM;

	switch (MINOR(file->f_dentry->d_inode->i_rdev)) {
		case 2: return oprof_map_open();
		case 1: return oprof_hash_map_open();
		default:
			/* make sure the other devices are open */ 
			if (!is_map_ready())
				return -EINVAL;
	}

	if (test_and_set_bit(0,&oprof_opened))
		return -EBUSY;

	return oprof_start();
}

static int oprof_release(struct inode *ino, struct file *file)
{
	switch (MINOR(file->f_dentry->d_inode->i_rdev)) {
		case 2: return oprof_map_release();
		case 1: return oprof_hash_map_release();
		default:
	}

	if (!oprof_opened)
		return -EFAULT;

	clear_bit(0,&oprof_opened);

	return oprof_stop();
}

static int oprof_mmap(struct file *file, struct vm_area_struct *vma)
{
	if (MINOR(file->f_dentry->d_inode->i_rdev)==1)
		return oprof_hash_map_mmap(file,vma);
	return -EINVAL;
}

/* called under spinlock, cannot sleep */
static void oprof_free_mem(uint num)
{
	uint i;
	for (i=0; i < num; i++) {
		if (oprof_data[i].entries)
			vfree(oprof_data[i].entries);
		if (oprof_data[i].buffer)
			vfree(oprof_data[i].buffer);
		oprof_data[i].entries = NULL;
		oprof_data[i].buffer = NULL;
	}
}

static int oprof_init_data(void)
{
	uint i;
	ulong hash_size,buf_size;
	struct _oprof_data *data;

	for (i=0; i < smp_num_cpus; i++) {
		data = &oprof_data[i];
		hash_size = (sizeof(struct op_entry)*op_hash_size);
		buf_size = (sizeof(struct op_sample)*op_buf_size);

		data->entries = vmalloc(hash_size);
		if (!data->entries) {
			printk("oprofile: failed to allocate hash table of %lu bytes\n",hash_size);
			oprof_free_mem(i);
			return -EFAULT;
		}

		data->buffer = vmalloc(buf_size);
		if (!data->buffer) {
			printk("oprofile: failed to allocate eviction buffer of %lu bytes\n",buf_size);
			vfree(data->entries);
			oprof_free_mem(i);
			return -EFAULT;
		}

		memset(data->entries,0,hash_size);
		memset(data->buffer,0,buf_size);

		data->hash_size = op_hash_size;
		data->buf_size = op_buf_size;
	}

	return 0;
}

static int parms_ok(void)
{
	int ret;
	uint cpu;
	struct _oprof_data *data;

	op_check_range(op_hash_size,256,262144,"op_hash_size value %d not in range\n");
	op_check_range(op_buf_size,512,262144,"op_buf_size value %d not in range\n");

	for (cpu=0; cpu < smp_num_cpus; cpu++) {
		data = &oprof_data[cpu];
		data->ctrs |= OP_CTR_0*(!!op_ctr0_on[cpu]);
		data->ctrs |= OP_CTR_1*(!!op_ctr1_on[cpu]);

		/* FIXME: maybe we should allow no set on a CPU ? */
		if (!data->ctrs) {
			printk("oprofile: neither counter enabled for CPU%d\n",cpu);
			return 0;
		}

		/* make sure the buffer and hash table have been set up */
		if (!data->buffer || !data->entries)
			return 0;

		if (data->ctrs&OP_CTR_0) {
			if (!op_ctr0_user[cpu] && !op_ctr0_kernel[cpu]) {
				printk("oprofile: neither kernel nor user set for enabled counter 0 on CPU %d\n", cpu);
				return 0;
			}
			op_check_range(op_ctr0_count[cpu],500,OP_MAX_PERF_COUNT,"ctr0 count value %d not in range\n");
			data->ctr_count[0]=op_ctr0_count[cpu];
		}
		if (data->ctrs&OP_CTR_1) {
			if (!op_ctr1_user[cpu] && !op_ctr1_kernel[cpu]) {
				printk("oprofile: neither kernel nor user set for enabled counter 1 on CPU %d\n", cpu);
				return 0;
			}
			op_check_range(op_ctr1_count[cpu],500,OP_MAX_PERF_COUNT,"ctr1 count value %d not in range\n");
			data->ctr_count[1]=op_ctr1_count[cpu];
		}

		/* hw_ok() has set cpu_type */
		ret = op_check_events(op_ctr0_val[cpu], op_ctr1_val[cpu], op_ctr0_um[cpu], op_ctr1_um[cpu], cpu_type);

		if (ret&OP_CTR0_NOT_FOUND) printk("oprofile: ctr0: no such event\n");
		if (ret&OP_CTR1_NOT_FOUND) printk("oprofile: ctr1: no such event\n");
		if (ret&OP_CTR0_NO_UM) printk("oprofile: ctr0: invalid unit mask\n");
		if (ret&OP_CTR1_NO_UM) printk("oprofile: ctr1: invalid unit mask\n");
		if (ret&OP_CTR0_NOT_ALLOWED) printk("oprofile: ctr0 can't count this event\n");
		if (ret&OP_CTR1_NOT_ALLOWED) printk("oprofile: ctr1 can't count this event\n");
		if (ret&OP_CTR0_PII_EVENT) printk("oprofile: ctr0: event only available on PII\n");
		if (ret&OP_CTR1_PII_EVENT) printk("oprofile: ctr1: event only available on PII\n");
		if (ret&OP_CTR0_PIII_EVENT) printk("oprofile: ctr0: event only available on PIII\n");
		if (ret&OP_CTR1_PIII_EVENT) printk("oprofile: ctr1: event only available on PIII\n");

		if (ret)
			return 0;
	}

	return 1;
}

DECLARE_MUTEX(sysctlsem);

static int oprof_start(void)
{
	int err = 0;
	
	down(&sysctlsem);

	if ((err = oprof_init_data()))
		goto out;

	if (!parms_ok()) {
		oprof_free_mem(smp_num_cpus);
		err = -EINVAL;
		goto out;
	}
		
	if ((smp_call_function(pmc_setup,NULL,0,1))) {
		oprof_free_mem(smp_num_cpus);
		err = -EINVAL;
		goto out;
	}

	pmc_setup(NULL);
	
	install_nmi();

	op_intercept_syscalls();

	oprof_start_thread();
	smp_call_function(pmc_start,NULL,0,1);
	
	pmc_start(NULL);
	prof_on = 1;
	
out:
	up(&sysctlsem);
	return err;
}

static int oprof_stop(void)
{
	uint i;

	if (!prof_on)
		return -EINVAL;

	down(&sysctlsem);

	/* here we need to :
	 * bring back the old system calls
	 * stop the wake-up thread
	 * stop the perf counter
	 * bring back the old NMI handler
	 * reset the map buffer stuff and ready values
	 *
	 * Nothing will be able to write into the map buffer because
	 * we check explicitly for prof_on and synchronise via the spinlocks
	 */

	op_replace_syscalls();

	oprof_stop_thread();

	prof_on = 0;

	smp_call_function(pmc_stop,NULL,0,1);
	pmc_stop(NULL);
	restore_nmi();

	spin_lock(&map_lock);
	spin_lock(&note_lock);

	nextmapbuf = 0;

	for (i=0; i < smp_num_cpus; i++) {
		struct _oprof_data *data = &oprof_data[i];
		oprof_ready[i] = 0;
		data->nextbuf = data->next = 0;
		oprof_free_mem(smp_num_cpus);
	}

	spin_unlock(&note_lock);
	spin_unlock(&map_lock);

	up(&sysctlsem);
	return 0;
}

static struct file_operations oprof_fops = {
	owner: THIS_MODULE,
	open: oprof_open,
	release: oprof_release,
	read: oprof_read,
	mmap: oprof_mmap,
};

static int can_unload(void)
{
	return -EBUSY; /* nope */
}

static int __init hw_ok(void)
{
	if (current_cpu_data.x86_vendor != X86_VENDOR_INTEL ||
	    current_cpu_data.x86 < 6) {
		printk("oprofile: not an Intel P6 processor. Sorry.\n");
		return 0;
	}

	/* 0 if PPro, 1 if PII, 2 if PIII */
	cpu_type = (current_cpu_data.x86_model > 5) ? 2 :
		(current_cpu_data.x86_model > 2);
	return 1;
}

static int init_sysctl(void);
static void cleanup_sysctl(void);

int __init oprof_init(void)
{
	int err;

	if ((err = init_sysctl()))
		return err;

	if (!hw_ok())
		return -EINVAL;

	printk(KERN_INFO "%s\n",op_version);

	if ((err = apic_setup()))
		return err;

	if ((err = smp_call_function(smp_apic_setup,NULL,0,1)))
		goto out_err;

 	err = op_major = register_chrdev(0,"oprof",&oprof_fops);
	if (err<0)
		goto out_err;

	err = oprof_init_hashmap();
	if (err<0) {
		unregister_chrdev(op_major,"oprof");
		goto out_err;
	}

	/* module is not unloadable */
	THIS_MODULE->can_unload = can_unload;
		
	printk("oprofile: oprofile loaded, major %u\n",op_major);
	return 0;

out_err:
	smp_call_function(disable_local_P6_APIC,NULL,0,1);
	disable_local_P6_APIC(NULL);
	return err;
}

void __exit oprof_exit(void)
{
	/* FIXME: what to do here ? will this ever happen ? */

	cleanup_sysctl();

	oprof_free_hashmap();

	op_replace_syscalls();

	unregister_chrdev(op_major,"oprof");

	if (smp_call_function(disable_local_P6_APIC,NULL,0,1))
		return;
	disable_local_P6_APIC(NULL);

	return;
}

module_init(oprof_init);
module_exit(oprof_exit);

/*
 * /proc/sys/dev/oprofile/
 *                    bufsize
 *                    hashsize
 *                    dump
 *                    pid_filter
 *                    pgrp_filter
 *                    0/
 *                      0/
 *                        event
 *                        enabled 
 *                        count
 *                        unit_mask
 *                        kernel
 *                        user
 *                      1/
 *                        event
 *                        enabled
 *                        count
 *                        unit_mask
 *                        kernel
 *                        user
 *                    1/
 *                    ...
 */

static int lproc_dointvec(ctl_table *table, int write, struct file *filp, void *buffer, size_t *lenp)
{
	int err;

	down(&sysctlsem);
	err = proc_dointvec(table, write, filp, buffer, lenp);
	up(&sysctlsem);

	return err;	
}

static int sysctl_do_dump(ctl_table *table, int write, struct file *filp, void *buffer, size_t *lenp)
{
	uint cpu; 
	int err = -EINVAL;

	down(&sysctlsem);
	
	if (write) {
		if (!prof_on)
			goto out;

		for (cpu=0; cpu < smp_num_cpus; cpu++)
			oprof_ready[cpu]=2;
		wake_up(&oprof_wait);
		err = 0;
		goto out;
	}
	
	err = proc_dointvec(table, write, filp, buffer, lenp);
	
out:
	up(&sysctlsem);
	return err;
}

static ctl_table oprof_table[] = {
	{ 1, "bufsize", &op_buf_size, sizeof(int), 0600, NULL, &lproc_dointvec, NULL, },
	{ 1, "hashsize", &op_hash_size, sizeof(int), 0600, NULL, &lproc_dointvec, NULL, },
	{ 1, "dump", &sysctl_dump, sizeof(int), 0600, NULL, &sysctl_do_dump, NULL, },
	{ 1, "pid_filter", &pid_filter, sizeof(pid_t), 0600, NULL, &lproc_dointvec, NULL, },
	{ 1, "pgrp_filter", &pgrp_filter, sizeof(pid_t), 0600, NULL, &lproc_dointvec, NULL, },
	{0,}, {0,}, {0,}, {0,}, {0,}, {0,}, {0,}, {0,}, {0,}, {0,}, {0,}, {0,}, {0,}, {0,}, {0,}, {0,},
	{0,}, {0,}, {0,}, {0,}, {0,}, {0,}, {0,}, {0,}, {0,}, {0,}, {0,}, {0,}, {0,}, {0,}, {0,}, {0,},
	{0,},
};

static int nr_oprof_static = 5;

static ctl_table oprof_root[] = {
	{1, "oprofile", NULL, 0, 0700, oprof_table},
 	{0,},
};

static ctl_table dev_root[] = {
	{CTL_DEV, "dev", NULL, 0, 0555, oprof_root},
	{0,},
};

static char *names[] = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14",
	"15", "16", "17", "18", "19", "20", "21", "22", "23", "24", "25", "26", "27", "28", "29", "30", "31", };

static struct ctl_table_header *sysctl_header;

/* NOTE: we do *not* support sysctl() syscall */

int __init init_sysctl(void)
{
	ctl_table *next = &oprof_table[nr_oprof_static];
	ctl_table *cpu_table;
	ctl_table *curr;
	int i;

	/* FIXME: leaks if kmalloc() fails */

	/* FIXME: no proper numbers, or verifiers (where possible) */

	/* FIXME: events should be a string for convenience */

	/* iterate over each CPU */
	for (i=0; i < smp_num_cpus; i++) {
		next->ctl_name = 1;
		next->procname = names[i];
		next->mode = 0700;

		if (!(cpu_table = kmalloc(sizeof(ctl_table)*3, GFP_KERNEL)))
			return -EFAULT;
		memset(cpu_table, 0, sizeof(ctl_table)*3);
		cpu_table[0] = ((ctl_table){ 1, "0", NULL, 0, 0700, NULL, NULL, NULL, });
		cpu_table[1] = ((ctl_table){ 1, "1", NULL, 0, 0700, NULL, NULL, NULL, });
		next->child = cpu_table;

		/* counter 0 */
		if (!(curr = kmalloc(sizeof(ctl_table)*7, GFP_KERNEL)))
			return -EFAULT;
		memset(curr, 0, sizeof(ctl_table)*7);
		curr[0] = ((ctl_table){ 1, "enabled", &op_ctr0_on[i], sizeof(int), 0600, NULL, lproc_dointvec, NULL, });
		curr[1] = ((ctl_table){ 1, "event", &op_ctr0_val[i], sizeof(int), 0600, NULL, lproc_dointvec, NULL,  });
		curr[2] = ((ctl_table){ 1, "count", &op_ctr0_count[i], sizeof(int), 0600, NULL, lproc_dointvec, NULL, });
		curr[3] = ((ctl_table){ 1, "unit_mask", &op_ctr0_um[i], sizeof(int), 0600, NULL, lproc_dointvec, NULL, });
		curr[4] = ((ctl_table){ 1, "kernel", &op_ctr0_kernel[i], sizeof(int), 0600, NULL, lproc_dointvec, NULL, });
		curr[5] = ((ctl_table){ 1, "user", &op_ctr0_user[i], sizeof(int), 0600, NULL, lproc_dointvec, NULL, });
		cpu_table[0].child = curr;
		
		/* counter 1 */
		if (!(curr = kmalloc(sizeof(ctl_table)*7, GFP_KERNEL)))
			return -EFAULT;
		memset(curr, 0, sizeof(ctl_table)*7);
		curr[0] = ((ctl_table){ 1, "enabled", &op_ctr1_on[i], sizeof(int), 0600, NULL, lproc_dointvec, NULL, });
		curr[1] = ((ctl_table){ 1, "event", &op_ctr1_val[i], sizeof(int), 0600, NULL, lproc_dointvec, NULL,  });
		curr[2] = ((ctl_table){ 1, "count", &op_ctr1_count[i], sizeof(int), 0600, NULL, lproc_dointvec, NULL, });
		curr[3] = ((ctl_table){ 1, "unit_mask", &op_ctr1_um[i], sizeof(int), 0600, NULL, lproc_dointvec, NULL, });
		curr[4] = ((ctl_table){ 1, "kernel", &op_ctr1_kernel[i], sizeof(int), 0600, NULL, lproc_dointvec, NULL, });
		curr[5] = ((ctl_table){ 1, "user", &op_ctr1_user[i], sizeof(int), 0600, NULL, lproc_dointvec, NULL, });
		cpu_table[1].child = curr;
		next++;
	}

	sysctl_header = register_sysctl_table(dev_root, 0);
	return 0;
}

void __exit cleanup_sysctl(void)
{
	int i;
	ctl_table *next = &oprof_table[nr_oprof_static];
	unregister_sysctl_table(sysctl_header);
	
	for (i=0; i < smp_num_cpus; i++) {
		kfree(next->child[0].child);
		kfree(next->child[1].child);
		kfree(next->child);
		next ++;
	}
	return;
}
