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

#include "oprofile.h"

EXPORT_NO_SYMBOLS;

static char *op_version = VERSION_STRING;
MODULE_AUTHOR("John Levon (moz@compsoc.man.ac.uk)");
MODULE_DESCRIPTION("Continuous Profiling Module");
MODULE_LICENSE("GPL");

#ifdef CONFIG_SMP
MODULE_PARM(allow_unload, "i");
MODULE_PARM_DESC(allow_unload, "Allow module to be unloaded on SMP");
static int allow_unload;
#endif
 
/* sysctl settables */
static int op_hash_size=OP_DEFAULT_HASH_SIZE;
static int op_buf_size=OP_DEFAULT_BUF_SIZE;
static int sysctl_dump;
static int kernel_only;
static int op_ctr_on[OP_MAX_COUNTERS];
static int op_ctr_um[OP_MAX_COUNTERS];
static int op_ctr_count[OP_MAX_COUNTERS];
static int op_ctr_val[OP_MAX_COUNTERS];
static int op_ctr_kernel[OP_MAX_COUNTERS];
static int op_ctr_user[OP_MAX_COUNTERS];

pid_t pid_filter;
pid_t pgrp_filter;

/* the MSRs we need */
static uint perfctr_msr[OP_MAX_COUNTERS];
static uint eventsel_msr[OP_MAX_COUNTERS];

/* number of counters physically present */
uint op_nr_counters = 2;

/* whether we enable for each counter (athlon) or globally (intel) */
int separate_running_bit;
 
static u32 prof_on __cacheline_aligned;

static int op_major;
int cpu_type;

static volatile uint oprof_opened __cacheline_aligned;
static DECLARE_WAIT_QUEUE_HEAD(oprof_wait);

u32 oprof_ready[NR_CPUS] __cacheline_aligned;
static struct _oprof_data oprof_data[NR_CPUS];
static uint op_irq_stats[NR_CPUS] __cacheline_aligned;

extern spinlock_t map_lock;

/* ---------------- NMI handler ------------------ */

/* FIXME: this whole handler would probably be better in straight asm */
static void evict_op_entry(struct _oprof_data *data, struct op_sample *ops)
{
	memcpy(&data->buffer[data->nextbuf], ops, sizeof(struct op_sample));
	if (++data->nextbuf != (data->buf_size - OP_PRE_WATERMARK)) {
		if (data->nextbuf == data->buf_size)
			data->nextbuf = 0;
		return;
	}
	oprof_ready[op_cpu_id()] = 1;
}

inline static void fill_op_entry(struct op_sample *ops, struct pt_regs *regs, int ctr)
{
	ops->eip = regs->eip;
	ops->pid = current->pid;
	ops->count = (1U << OP_BITS_COUNT)*ctr + 1;
}

inline static void op_do_profile(struct _oprof_data *data, struct pt_regs *regs, int ctr)
{
	uint h = op_hash(regs->eip, current->pid, ctr);
	uint i;

	for (i=0; i < OP_NR_ENTRY; i++) {
		if (!op_miss(data->entries[h].samples[i])) {
			data->entries[h].samples[i].count++;
			set_perfctr(data->ctr_count[ctr], ctr);
			return;
		} else if (op_full_count(data->entries[h].samples[i].count)) {
			goto full_entry;
		} else if (!data->entries[h].samples[i].count)
			goto new_entry;
	}

	evict_op_entry(data, &data->entries[h].samples[data->next]);
	fill_op_entry(&data->entries[h].samples[data->next], regs, ctr);
	data->next = (data->next + 1) % OP_NR_ENTRY;
out:
	set_perfctr(data->ctr_count[ctr], ctr);
	return;
full_entry:
	evict_op_entry(data, &data->entries[h].samples[i]);
new_entry:
	fill_op_entry(&data->entries[h].samples[i],regs,ctr);
	goto out;
}

static void op_check_ctr(struct _oprof_data *data, struct pt_regs *regs, int ctr)
{
	ulong l,h;
	get_perfctr(l, h, ctr);
	if (ctr_overflowed(l)) {
		op_do_profile(data, regs, ctr);
		op_irq_stats[op_cpu_id()]++;
	} 
}

asmlinkage void op_do_nmi(struct pt_regs *regs)
{
	struct _oprof_data *data = &oprof_data[op_cpu_id()];
	int i;

	if (pid_filter && current->pid != pid_filter)
		return;
	if (pgrp_filter && current->pgrp != pgrp_filter)
		return;

	for (i = 0 ; i < op_nr_counters ; ++i) {
		op_check_ctr(data, regs, i);
	}
}

/* ---------------- PMC setup ------------------ */

static void pmc_fill_in(uint *val, u8 kernel, u8 user, u8 event, u8 um)
{
	/* enable interrupt generation */
	*val |= (1<<20);
	/* enable/disable chosen OS and USR counting */
	(user)   ? (*val |= (1<<16))
		 : (*val &= ~(1<<16));

	(kernel) ? (*val |= (1<<17))
		 : (*val &= ~(1<<17));

	/* what are we counting ? */
	*val |= event;
	*val |= (um<<8);
}

static void pmc_setup(void *dummy)
{
	uint low, high;
	int i;

	// first, let's use the right MSRs
	switch (cpu_type) {
		case CPU_ATHLON:
			eventsel_msr[0] = MSR_K7_PERFCTL0;
			eventsel_msr[1] = MSR_K7_PERFCTL1;
			eventsel_msr[2] = MSR_K7_PERFCTL2;
			eventsel_msr[3] = MSR_K7_PERFCTL3;
			perfctr_msr[0] = MSR_K7_PERFCTR0;
			perfctr_msr[1] = MSR_K7_PERFCTR1;
			perfctr_msr[2] = MSR_K7_PERFCTR2;
			perfctr_msr[3] = MSR_K7_PERFCTR3;
			break;
		default:
			eventsel_msr[0] = MSR_IA32_EVNTSEL0;
			eventsel_msr[1] = MSR_IA32_EVNTSEL1;
			perfctr_msr[0] = MSR_IA32_PERFCTR0;
			perfctr_msr[1] = MSR_IA32_PERFCTR1;
			break;
	}

	/* IA Vol. 3 Figure 15-3 */

	/* Stop and clear all counter: IA32 use bit 22 of eventsel_msr0 to
	 * enable/disable all counter, AMD use separate bit 22 in each msr,
	 * all bits are cleared except the reserved bits 21 */
	for (i = 0 ; i < op_nr_counters ; ++i) {
		rdmsr(eventsel_msr[i], low, high);
		wrmsr(eventsel_msr[i], low & (1 << 21), high);

		/* avoid a false detection of ctr overflow in NMI handler */
		wrmsr(perfctr_msr[i], -1, -1);
	}

	/* setup each counter */
	for (i = 0 ; i < op_nr_counters ; ++i) {
		if (op_ctr_val[i]) {
			rdmsr(eventsel_msr[i], low, high);

			low &= 1 << 21;  /* do not touch the reserved bit */
			set_perfctr(op_ctr_count[i], i);

			pmc_fill_in(&low, op_ctr_kernel[i], op_ctr_user[i], 
				op_ctr_val[i], op_ctr_um[i]);

			wrmsr(eventsel_msr[i], low, high);
		}
	}
	
	/* Here all setup is made except the start/stop bit 22, counter
	 * disabled contains zeros in the eventsel msr except the reserved bit
	 * 21 */
}

inline static void pmc_start_P6(void)
{
	uint low,high;

	rdmsr(eventsel_msr[0], low, high);
	wrmsr(eventsel_msr[0], low | (1 << 22), high);
}

inline static void pmc_start_Athlon(void)
{
	uint low,high;
	int i;

	for (i = 0 ; i < op_nr_counters ; ++i) {
		if (op_ctr_count[i]) {
			rdmsr(eventsel_msr[i], low, high);
			wrmsr(eventsel_msr[i], low | (1 << 22), high);
		}
	}
}

static void pmc_start(void *info)
{
	if (info && (*((uint *)info) != op_cpu_id()))
		return;

	/* assert: all enable counter are setup except the bit start/stop,
	 * all counter disable contains zeroes (except perhaps the reserved
	 * bit 21), counter disable contains -1 sign extended in msr count */

	/* enable all needed counter */
	if (separate_running_bit == 0)
		pmc_start_P6();
	else
		pmc_start_Athlon();
}

inline static void pmc_stop_P6(void)
{
	uint low,high;

	rdmsr(eventsel_msr[0], low, high);
	wrmsr(eventsel_msr[0], low & ~(1 << 22), high);
}

inline static void pmc_stop_Athlon(void)
{
	uint low,high;
	int i;

	for (i = 0 ; i < op_nr_counters ; ++i) {
		if (op_ctr_count[i]) {
			rdmsr(eventsel_msr[i], low, high);
			wrmsr(eventsel_msr[i], low & ~(1 << 22), high);
		}
	}
}

static void pmc_stop(void *info)
{
	if (info && (*((uint *)info) != op_cpu_id()))
		return;

	/* disable counters */
	if (separate_running_bit == 0)
		pmc_stop_P6();
	else
		pmc_stop_Athlon();
}

inline static void pmc_select_start(uint cpu)
{
	if (cpu == op_cpu_id())
		pmc_start(NULL);
	else
		smp_call_function(pmc_start, &cpu, 0, 1);
}

inline static void pmc_select_stop(uint cpu)
{
	if (cpu == op_cpu_id())
		pmc_stop(NULL);
	else
		smp_call_function(pmc_stop, &cpu, 0, 1);
}

/* ---------------- driver routines ------------------ */

static u32 diethreaddie;
static pid_t threadpid;

DECLARE_COMPLETION(threadstop);

/* we have to have another thread because we can't
 * do wake_up() from NMI due to no locking
 */
static int oprof_thread(void *arg)
{
	int i;

	daemonize();
	sprintf(current->comm, "oprof-thread");
	siginitsetinv(&current->blocked, sigmask(SIGKILL));
	spin_lock(&current->sigmask_lock);
	flush_signals(current);
	spin_unlock(&current->sigmask_lock);
	current->policy = SCHED_OTHER;
	current->nice = -20;

	for (;;) {
		for (i=0; i < smp_num_cpus; i++) {
			if (oprof_ready[i])
				wake_up(&oprof_wait);
		}
		current->state = TASK_INTERRUPTIBLE;
		/* FIXME: determine best value here */
		schedule_timeout(HZ/10);

		if (diethreaddie)
			break;
	}

	complete_and_exit(&threadstop,0);
	return 0;
}

static void oprof_start_thread(void)
{
	init_completion(&threadstop);
	diethreaddie = 0;
	threadpid = kernel_thread(oprof_thread, NULL, CLONE_FS|CLONE_FILES|CLONE_SIGHAND);
	if (threadpid < 0) {
		printk(KERN_ERR "oprofile: couldn't spawn wakeup thread.\n");
		threadpid = 0;
	}
}

static void oprof_stop_thread(void)
{
	diethreaddie = 1;
	kill_proc(SIGKILL, threadpid, 1);
	wait_for_completion(&threadstop);
}

#define wrap_nextbuf() do { \
	if (++data->nextbuf == (data->buf_size - OP_PRE_WATERMARK)) { \
		oprof_ready[0] = 1; \
		wake_up(&oprof_wait); \
	} else if (data->nextbuf == data->buf_size) \
		data->nextbuf = 0; \
	} while (0)

spinlock_t note_lock __cacheline_aligned = SPIN_LOCK_UNLOCKED;

void oprof_put_mapping(struct op_mapping *map)
{
	struct _oprof_data *data = &oprof_data[0];

	if (!prof_on)
		return;

	/* FIXME: IPI :( */
	spin_lock(&note_lock);
	pmc_select_stop(0);

	data->buffer[data->nextbuf].eip = map->addr;
	data->buffer[data->nextbuf].pid = map->pid;
	data->buffer[data->nextbuf].count =
		((map->is_execve) ? OP_EXEC : OP_MAP)
		| map->hash;
	wrap_nextbuf();
	data->buffer[data->nextbuf].eip = map->len;
	data->buffer[data->nextbuf].pid = map->offset & 0xffff;
	data->buffer[data->nextbuf].count = map->offset >> 16;
	wrap_nextbuf();
	
	pmc_select_start(0);
	spin_unlock(&note_lock);
}

void oprof_put_note(struct op_sample *samp)
{
	struct _oprof_data *data = &oprof_data[0];

	if (!prof_on)
		return;

	/* FIXME: IPIs are expensive */
	spin_lock(&note_lock);
	pmc_select_stop(0);

	memcpy(&data->buffer[data->nextbuf], samp, sizeof(struct op_sample));
	wrap_nextbuf();

	pmc_select_start(0);
	spin_unlock(&note_lock);
}

uint cpu_num;

static int is_ready(void)
{
	for (cpu_num=0; cpu_num < smp_num_cpus; cpu_num++) {
		if (oprof_ready[cpu_num])
			return 1;
	}
	return 0;
}

static int check_buffer_amount(struct _oprof_data * data)
{
	int size = data->buf_size; 
	int num = data->nextbuf;
	if (num < size - OP_PRE_WATERMARK && oprof_ready[cpu_num] != 2) {
		printk(KERN_ERR "oprofile: Detected overflow of size %d. You must increase the "
				"hash table size or reduce the interrupt frequency (%d)\n",
				num, oprof_ready[cpu_num]);
		num = size;
	} else
		data->nextbuf=0;
	return num;
}

static int oprof_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	struct op_sample *mybuf;
	uint num;
	ssize_t max;

	if (!capable(CAP_SYS_PTRACE))
		return -EPERM;

	if (!prof_on) {
		kill_proc(SIGKILL, current->pid, 1);
		return -EINTR;
	}

	if (MINOR(file->f_dentry->d_inode->i_rdev) != 0)
		return -EINVAL;

	max = sizeof(struct op_sample) * op_buf_size;

	if (*ppos || count != max)
		return -EINVAL;

	mybuf = vmalloc(max);
	if (!mybuf)
		return -EFAULT;

	wait_event_interruptible(oprof_wait, is_ready());

	if (signal_pending(current)) {
		vfree(mybuf);
		return -EINTR;
	}

	/* FIXME: what if a signal occurs now ? What is returned to
	 * the read() routine ?
	 */

	pmc_select_stop(cpu_num);
	spin_lock(&note_lock);

	/* buffer might have overflowed */
	num = check_buffer_amount(&oprof_data[cpu_num]);
 
	oprof_ready[cpu_num] = 0;

	count = num * sizeof(struct op_sample);

	/* FIXME: can eliminate this bounce buffer when note_lock dies */
	if (count)
		memcpy(mybuf, oprof_data[cpu_num].buffer, count);

	spin_unlock(&note_lock);
	pmc_select_start(cpu_num);

	if (count && copy_to_user(buf, mybuf, count))
		count = -EFAULT;

	vfree(mybuf);
	return count;
}

static int oprof_start(void);
static int oprof_stop(void);

static int oprof_open(struct inode *ino, struct file *file)
{
	int err;

	if (!capable(CAP_SYS_PTRACE))
		return -EPERM;

	switch (MINOR(file->f_dentry->d_inode->i_rdev)) {
		case 1: return oprof_hash_map_open();
		case 0:
			/* make sure the other devices are open */
			if (is_map_ready())
				break;
		default:
			return -EINVAL;
	}

	if (test_and_set_bit(0, &oprof_opened))
		return -EBUSY;

	err = oprof_start();
	if (err)
		clear_bit(0, &oprof_opened);
	return err;
}

static int oprof_release(struct inode *ino, struct file *file)
{
	switch (MINOR(file->f_dentry->d_inode->i_rdev)) {
		case 1: return oprof_hash_map_release();
		case 0: break;
		default: return -EINVAL;
	}

	if (!oprof_opened)
		return -EFAULT;

	clear_bit(0, &oprof_opened);

	return oprof_stop();
}

static int oprof_mmap(struct file *file, struct vm_area_struct *vma)
{
	if (MINOR(file->f_dentry->d_inode->i_rdev) == 1)
		return oprof_hash_map_mmap(file, vma);
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
		hash_size = (sizeof(struct op_entry) * op_hash_size);
		buf_size = (sizeof(struct op_sample) * op_buf_size);

		data->entries = vmalloc(hash_size);
		if (!data->entries) {
			printk(KERN_ERR "oprofile: failed to allocate hash table of %lu bytes\n",hash_size);
			oprof_free_mem(i);
			return -EFAULT;
		}

		data->buffer = vmalloc(buf_size);
		if (!data->buffer) {
			printk(KERN_ERR "oprofile: failed to allocate eviction buffer of %lu bytes\n",buf_size);
			vfree(data->entries);
			oprof_free_mem(i);
			return -EFAULT;
		}

		memset(data->entries, 0, hash_size);
		memset(data->buffer, 0, buf_size);

		data->hash_size = op_hash_size;
		data->buf_size = op_buf_size;
	}

	return 0;
}

static int parms_ok(void)
{
	int ret;
	int i;
	uint cpu;
	int ok;
	struct _oprof_data *data;
	int enabled = 0;

	op_check_range(op_hash_size, 256, 262144, 
		"op_hash_size value %d not in range (%d %d)\n");
	op_check_range(op_buf_size, 1024, 1048576, 
		"op_buf_size value %d not in range (%d %d)\n");

	for (i = 0; i < op_nr_counters ; i++) {
		if (op_ctr_on[i]) {
			int min_count = op_min_count(op_ctr_val[i], cpu_type);

			if (!op_ctr_user[i] && !op_ctr_kernel[i]) {
				printk(KERN_ERR "oprofile: neither kernel nor user "
					"set for counter %d\n", i);
				return 0;
			}
			op_check_range(op_ctr_count[i], min_count, 
				OP_MAX_PERF_COUNT, 
				"ctr count value %d not in range (%d %ld)\n");
 
			enabled = 1;
		}
	}

	if (!enabled) {
		printk(KERN_ERR "oprofile: no counters have been enabled.\n");
		return 0;
	}

	/* hw_ok() has set cpu_type */
	ok = 1;
	for (i = 0 ; i < op_nr_counters ; ++i) {
		ret = op_check_events(i, op_ctr_val[i], op_ctr_um[i], cpu_type);

		if (ret & OP_EVT_NOT_FOUND)
			printk(KERN_ERR "oprofile: ctr%d: %d: no such event for cpu %d\n", i, op_ctr_val[i], cpu_type);

		if (ret & OP_EVT_NO_UM)
			printk(KERN_ERR "oprofile: ctr%d: 0x%.2x: invalid unit mask for cpu %d\n", i, op_ctr_um[i], cpu_type);

		if (ret & OP_EVT_CTR_NOT_ALLOWED)
			printk(KERN_ERR "oprofile: ctr%d: %d: can't count event for this counter\n", i, op_ctr_val[i]);

		if (ret != OP_EVENTS_OK)
			ok = 0;
	}
	
	if (!ok)
		return 0;

	for (cpu=0; cpu < smp_num_cpus; cpu++) {
		data = &oprof_data[cpu];

		/* make sure the buffer and hash table have been set up */
		if (!data->buffer || !data->entries)
			return 0;

		for (i = 0 ; i < op_nr_counters ; ++i) {
			if (op_ctr_on[i]) {
				data->ctr_count[i] = op_ctr_count[i];
			} else {
				data->ctr_count[i] = 0;
			}
		}
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

	if ((smp_call_function(pmc_setup, NULL, 0, 1))) {
		oprof_free_mem(smp_num_cpus);
		err = -EINVAL;
		goto out;
	}

	pmc_setup(NULL);

	install_nmi();

	if (!kernel_only)
		op_intercept_syscalls();

	oprof_start_thread();
 
	smp_call_function(pmc_start, NULL, 0, 1);
	pmc_start(NULL);
 
	prof_on = 1;

out:
	up(&sysctlsem);
	return err;
}

static int oprof_stop(void)
{
	uint i;
	int err = -EINVAL;

	down(&sysctlsem);

	if (!prof_on)
		goto out;

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

	smp_call_function(pmc_stop, NULL, 0, 1);
	pmc_stop(NULL);
	restore_nmi();

	spin_lock(&map_lock);
	spin_lock(&note_lock);

	for (i=0; i < smp_num_cpus; i++) {
		struct _oprof_data *data = &oprof_data[i];
		oprof_ready[i] = 0;
		data->nextbuf = data->next = 0;
		oprof_free_mem(smp_num_cpus);
	}

	spin_unlock(&note_lock);
	spin_unlock(&map_lock);
	err = 0; 

out:
	up(&sysctlsem);
	return err;
}

static struct file_operations oprof_fops = {
	owner: THIS_MODULE,
	open: oprof_open,
	release: oprof_release,
	read: oprof_read,
	mmap: oprof_mmap,
};

/*
 * /proc/sys/dev/oprofile/
 *                        bufsize
 *                        hashsize
 *                        dump
 *                        kernel_only
 *                        pid_filter
 *                        pgrp_filter
 *                        0/
 *                          event
 *                          enabled
 *                          count
 *                          unit_mask
 *                          kernel
 *                          user
 *                        1/
 *                          event
 *                          enabled
 *                          count
 *                          unit_mask
 *                          kernel
 *                          user
 */

static int lproc_dointvec(ctl_table *table, int write, struct file *filp, void *buffer, size_t *lenp)
{
	int err;

	down(&sysctlsem);
	err = proc_dointvec(table, write, filp, buffer, lenp);
	up(&sysctlsem);

	return err;	
}

static void dump_one(struct _oprof_data *data, struct op_sample *ops, uint cpu)
{
	if (!ops->count)
		return;

	memcpy(&data->buffer[data->nextbuf], ops, sizeof(struct op_sample));

	ops->count = 0;

	if (++data->nextbuf != (data->buf_size - OP_PRE_WATERMARK)) {
		if (data->nextbuf == data->buf_size)
			data->nextbuf=0;
		return;
	}
	oprof_ready[cpu] = 1;
}

static int sysctl_do_dump(ctl_table *table, int write, struct file *filp, void *buffer, size_t *lenp)
{
	uint cpu;
	int err = -EINVAL;
	int i,j;

	down(&sysctlsem);
	
	if (!prof_on)
		goto out;

	for (cpu=0; cpu < smp_num_cpus; cpu++)
		printk("oprofile: CPU%u: %u interrupts\n", cpu, op_irq_stats[cpu]);
 
	if (!write) {
		err = proc_dointvec(table, write, filp, buffer, lenp);
		goto out;
	}
 
	/* clean out the hash table as far as possible */
	for (cpu=0; cpu < smp_num_cpus; cpu++) {
		struct _oprof_data * data = &oprof_data[cpu];
		pmc_select_stop(cpu);
		for (i=0; i < data->hash_size; i++) {
			for (j=0; j < OP_NR_ENTRY; j++)
				dump_one(data, &data->entries[i].samples[j], cpu);
			if (oprof_ready[cpu])
				break;
		}
		oprof_ready[cpu] = 2;
		pmc_select_start(cpu);
	}
	wake_up(&oprof_wait);
	err = 0;
out:
	up(&sysctlsem);
	return err;
}

static int nr_oprof_static = 6;

static ctl_table oprof_table[] = {
	{ 1, "bufsize", &op_buf_size, sizeof(int), 0600, NULL, &lproc_dointvec, NULL, },
	{ 1, "hashsize", &op_hash_size, sizeof(int), 0600, NULL, &lproc_dointvec, NULL, },
	{ 1, "dump", &sysctl_dump, sizeof(int), 0600, NULL, &sysctl_do_dump, NULL, },
	{ 1, "kernel_only", &kernel_only, sizeof(int), 0600, NULL, &lproc_dointvec, NULL, },
	{ 1, "pid_filter", &pid_filter, sizeof(pid_t), 0600, NULL, &lproc_dointvec, NULL, },
	{ 1, "pgrp_filter", &pgrp_filter, sizeof(pid_t), 0600, NULL, &lproc_dointvec, NULL, },
	{ 0, }, { 0, }, { 0, }, { 0, }, 
	{ 0, }, 
};

static ctl_table oprof_root[] = {
	{1, "oprofile", NULL, 0, 0700, oprof_table},
 	{0,},
};

static ctl_table dev_root[] = {
	{CTL_DEV, "dev", NULL, 0, 0555, oprof_root},
	{0,},
};

static char *names[] = { "0", "1", "2", "3", "4", };

static struct ctl_table_header *sysctl_header;

/* NOTE: we do *not* support sysctl() syscall */

static int __init init_sysctl(void)
{
	ctl_table *next = &oprof_table[nr_oprof_static];
	ctl_table *tab;
	int i,j;

	/* FIXME: no proper numbers, or verifiers (where possible) */

	for (i=0; i < op_nr_counters; i++) {
		next->ctl_name = 1;
		next->procname = names[i];
		next->mode = 0700;

		if (!(tab = kmalloc(sizeof(ctl_table)*7, GFP_KERNEL)))
			goto cleanup;
		next->child = tab;

		memset(tab, 0, sizeof(ctl_table)*7);
		tab[0] = ((ctl_table){ 1, "enabled", &op_ctr_on[i], sizeof(int), 0600, NULL, lproc_dointvec, NULL, });
		tab[1] = ((ctl_table){ 1, "event", &op_ctr_val[i], sizeof(int), 0600, NULL, lproc_dointvec, NULL,  });
		tab[2] = ((ctl_table){ 1, "count", &op_ctr_count[i], sizeof(int), 0600, NULL, lproc_dointvec, NULL, });
		tab[3] = ((ctl_table){ 1, "unit_mask", &op_ctr_um[i], sizeof(int), 0600, NULL, lproc_dointvec, NULL, });
		tab[4] = ((ctl_table){ 1, "kernel", &op_ctr_kernel[i], sizeof(int), 0600, NULL, lproc_dointvec, NULL, });
		tab[5] = ((ctl_table){ 1, "user", &op_ctr_user[i], sizeof(int), 0600, NULL, lproc_dointvec, NULL, });
		next++;
	}

	sysctl_header = register_sysctl_table(dev_root, 0);
	return 0;

cleanup:
	next = &oprof_table[nr_oprof_static];
	for (j = 0; j < i; j++) {
		kfree(next->child);
		next++;
	}
	return -EFAULT;
}

static void __exit cleanup_sysctl(void)
{
	int i;
	ctl_table *next = &oprof_table[nr_oprof_static];
	unregister_sysctl_table(sysctl_header);
	
	i = smp_num_cpus;
	while (i-- > 0) {
		kfree(next->child);
		next++;
	}
	return;
}

static int can_unload(void)
{
	int can = -EBUSY;
	down(&sysctlsem);

	if (smp_can_unload() && !prof_on && !GET_USE_COUNT(THIS_MODULE))
		can = 0;
	up(&sysctlsem);
	return can;
}

int __init oprof_init(void)
{
	int err;

	printk(KERN_INFO "%s\n", op_version);

	/* FIXME: we should save out the old values for the pmcs, then put them back
	 * upon exit. This way the NMI oopser can work after unloading oprofile */ 
 
	find_intel_smp();

	if ((err = apic_setup()))
		return err;

	if ((err = init_sysctl()))
		return err;

	if ((err = smp_call_function(lvtpc_apic_setup, NULL, 0, 1)))
		goto out_err;

 	err = op_major = register_chrdev(0, "oprof", &oprof_fops);
	if (err<0)
		goto out_err2;

	err = oprof_init_hashmap();
	if (err < 0) {
		printk("oprofile: couldn't allocate hash map !\n"); 
		unregister_chrdev(op_major, "oprof");
		goto out_err2;
	}

	/* module might not be unloadable */
	THIS_MODULE->can_unload = can_unload;

	/* do this now so we don't have to track save/restores later */
	op_save_syscalls();
 
	printk("oprofile: oprofile loaded, major %u\n", op_major);
	return 0;

out_err2:
	smp_call_function(lvtpc_apic_restore, NULL, 0, 1);
	lvtpc_apic_restore(NULL);
out_err:
	cleanup_sysctl();
	return err;
}

void __exit oprof_exit(void)
{
	oprof_free_hashmap();
	unregister_chrdev(op_major, "oprof");
	smp_call_function(lvtpc_apic_restore, NULL, 0, 1);
	lvtpc_apic_restore(NULL);
	cleanup_sysctl();
	// currently no need to reset APIC state
}

/*
 * "The most valuable commodity I know of is information."
 *      - Gordon Gekko
 */
