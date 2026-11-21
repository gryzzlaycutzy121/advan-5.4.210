// SPDX-License-Identifier: GPL-2.0
/*
 * berry_lkvm.ko -- "pengamat lkvm" BFKRT (KVM guest run observer).
 *
 * lkvm (Linux KVM tool, userspace) menjalankan VM tamu lewat /dev/kvm:
 *   open("/dev/kvm") -> ioctl(KVM_CREATE_VM) -> KVM_CREATE_VCPU ->
 *   KVM_SET_* -> KVM_RUN (loop). Modul ini memasang kprobe pada gerbang
 *   kernel dari jalur itu dan merekam hidupnya:
 *     - kvm_arch_vcpu_ioctl_run  : masuk/keluar dari loop KVM_RUN
 *                                  (tamu benar-benar dieksekusi)
 *     - kvm_vm_ioctl             : perintah level-VM (create/destroy/…)
 *
 * Hanya MEREKAM (seperti modul BFKRT lain): tidak mengubah perilaku KVM.
 * Analisis (urutan, durasi, perintah) dilakukan userspace.
 *
 * Kendali /proc/bfkrt_lkvm:
 *   echo on  > ...  mulai merekam (default: on)
 *   echo off > ...  berhenti
 *   echo clr > ...  buang catatan
 *   cat      ...    catatan + ringkasan
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/ktime.h>
#include <linux/string.h>
#include "bfkrt_ko.h"

#define EV_RUN_ENTER 1
#define EV_RUN_EXIT  2
#define EV_VM_IOCTL  3

#define LK_RING 1024

struct lk_ev {
	u64  ts;
	u32  pid;
	u32  cmd;
	u16  type;
	char  comm[16];
};
static struct lk_ev lk_ring[LK_RING];
static int lk_head, lk_count;
static DEFINE_SPINLOCK(lk_lock);
static int lk_on = 1;

static void lk_push(u16 type, u32 cmd)
{
	struct lk_ev *e;
	unsigned long flags;

	if (!lk_on)
		return;
	spin_lock_irqsave(&lk_lock, flags);
	e = &lk_ring[lk_head];
	lk_head = (lk_head + 1) % LK_RING;
	if (lk_count < LK_RING)
		lk_count++;
	e->ts   = ktime_get_ns();
	e->pid  = current->pid;
	e->cmd  = cmd;
	e->type = type;
	memcpy(e->comm, current->comm, sizeof(e->comm) - 1);
	e->comm[sizeof(e->comm) - 1] = '\0';
	spin_unlock_irqrestore(&lk_lock, flags);
}

/* int kvm_arch_vcpu_ioctl_run(struct kvm_vcpu *vcpu) -- x0 */
static int run_pre(struct kprobe *p, struct pt_regs *regs)
{
	lk_push(EV_RUN_ENTER, 0);
	return 0;
}
static int run_post(struct kprobe *p, struct pt_regs *regs, unsigned long flags)
{
	lk_push(EV_RUN_EXIT, 0);
	return 0;
}
/* long kvm_vm_ioctl(struct file *filp, unsigned int ioctl, unsigned long arg) -- x1 = cmd */
static int vm_pre(struct kprobe *p, struct pt_regs *regs)
{
	lk_push(EV_VM_IOCTL, (u32)regs->regs[1]);
	return 0;
}

static struct kprobe kp_run = {
	.symbol_name = "kvm_arch_vcpu_ioctl_run",
	.pre_handler = run_pre,
	.post_handler = run_post,
};
static struct kprobe kp_vm = {
	.symbol_name = "kvm_vm_ioctl",
	.pre_handler = vm_pre,
};

static int lk_show(struct seq_file *m, void *v)
{
	int i, n;
	unsigned long flags;

	seq_printf(m, "modul : berry_lkvm (KVM run observer)\n");
	seq_printf(m, "state : %s\n", lk_on ? "on" : "off");
	seq_printf(m, "kejadian: %d (ring %d)\n", lk_count, LK_RING);

	spin_lock_irqsave(&lk_lock, flags);
	n = lk_count;
	for (i = 0; i < n; i++) {
		int idx = (lk_head - n + i + LK_RING) % LK_RING;
		struct lk_ev *e = &lk_ring[idx];
		const char *ty = e->type == EV_RUN_ENTER ? "RUN_ENTER" :
				 e->type == EV_RUN_EXIT  ? "RUN_EXIT " : "VM_IOCTL ";
		seq_printf(m, "%s %12llu pid=%d cmd=0x%x comm=%s\n",
			   ty, (unsigned long long)e->ts, e->pid, e->cmd, e->comm);
	}
	spin_unlock_irqrestore(&lk_lock, flags);
	return 0;
}
static int lk_open(struct inode *inode, struct file *file)
{
	return single_open(file, lk_show, NULL);
}
static ssize_t lk_write(struct file *f, const char __user *buf,
			size_t count, loff_t *ppos)
{
	char tmp[16];
	size_t n;

	memset(tmp, 0, sizeof(tmp));
	n = count < sizeof(tmp) - 1 ? count : sizeof(tmp) - 1;
	if (copy_from_user(tmp, buf, n))
		return -EFAULT;
	strim(tmp);
	if (!strcmp(tmp, "on"))
		lk_on = 1;
	else if (!strcmp(tmp, "off"))
		lk_on = 0;
	else if (!strcmp(tmp, "clr")) {
		unsigned long flags;
		spin_lock_irqsave(&lk_lock, flags);
		lk_head = lk_count = 0;
		spin_unlock_irqrestore(&lk_lock, flags);
	}
	return count;
}
static const struct file_operations lk_ops = {
	.owner   = THIS_MODULE,
	.open    = lk_open,
	.read    = seq_read,
	.write   = lk_write,
	.llseek  = seq_lseek,
	.release = single_release,
};

static int __init bfkrt_init(void)
{
	int r1, r2;

	r1 = register_kprobe(&kp_run);
	r2 = register_kprobe(&kp_vm);
	if (r1)
		pr_info("lkvm: kprobe kvm_arch_vcpu_ioctl_run gagal (%d)\n", r1);
	if (r2)
		pr_info("lkvm: kprobe kvm_vm_ioctl gagal (%d)\n", r2);
	if (r1 && r2)
		pr_warn("lkvm: kedua kprobe gagal, modul pasif\n");

	if (!proc_create(BFKRT_PROC_LKVM, 0644, NULL, &lk_ops)) {
		pr_err("lkvm: gagal buat /proc/%s\n", BFKRT_PROC_LKVM);
		return -ENOMEM;
	}
	pr_info("lkvm: siap (run_probe=%s vm_probe=%s)\n",
		r1 ? "gagal" : "ok", r2 ? "gagal" : "ok");
	return 0;
}

static void __exit bfkrt_exit(void)
{
	if (kp_run.addr)
		unregister_kprobe(&kp_run);
	if (kp_vm.addr)
		unregister_kprobe(&kp_vm);
	remove_proc_entry(BFKRT_PROC_LKVM, NULL);
	pr_info("lkvm: dilepas\n");
}

module_init(bfkrt_init);
module_exit(bfkrt_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("BFKRT");
MODULE_DESCRIPTION("Berry lkvm observer -- rekam KVM guest run (kvm_arch_vcpu_ioctl_run) dan VM ioctl");
