// SPDX-License-Identifier: GPL-2.0
/*
 * berry_symdump.ko -- "pembongkar simbol" BFKRT (strong symbol reveal).
 *
 * Mengapa "kuat": kernel Android menyembunyikan alamat simbol dari userspace
 * (kptr_restrict) dan menyaring daftar simbol (KALLSYMS_ALL). Modul ini hidup
 * DI DALAM kernel, jadi ia membaca tabel kallsyms asli langsung:
 *   - kallsyms_num_syms      : jumlah simbol
 *   - kallsyms_names         : nama terkompresi
 *   - kallsyms_offsets       : offset relatif (KALLSYMS_BASE_RELATIVE)
 *   - kallsyms_relative_base : base untuk offset
 *   - kallsyms_token_table   : tabel token (dekompresi)
 *   - kallsyms_token_index   : indeks token
 * Lalu mendekode SEMUA simbol (termasuk yang static, yang tak ada di
 * /proc/kallsyms) dan mengungkap alamat aslinya (melewati kptr_restrict).
 *
 * Kendali /proc/bfkrt_syms:
 *   echo dump  > ...   dekode + cetak SEMUA simbol ke dmesg
 *   echo kvm   > ...   cetak hanya simbol ber-awalan "kvm_"/"__kvm_" ke dmesg
 *   echo <nama> > ...  resolve satu nama (via kallsyms_lookup_name) ke dmesg
 *   cat        ...     ringkasan + hasil dekode (stream)
 *
 * Jika tabel kallsyms tidak bisa ditemukan (mis. KALLSYMS_ALL mati), modul
 * otomatis jatuh ke mode "lookup": resolve daftar ekspor dari symlist.h
 * satu per satu lewat kallsyms_lookup_name (lebih lambat tapi tetap valid).
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include "bfkrt_ko.h"
#include "symlist.h"

/* ---------- resolver kallsyms_lookup_name via kprobe ---------- */
static unsigned long (*bf_kln)(const char *name);
static int kln_pre(struct kprobe *p, struct pt_regs *regs)
{
	bf_kln = (unsigned long (*)(const char *))p->addr;
	return 0;
}
static struct kprobe kln_kp = {
	.symbol_name = "kallsyms_lookup_name",
	.pre_handler = kln_pre,
};

/* ---------- mode tabel langsung (KALLSYMS_ALL aktif) ---------- */
struct bf_kall {
	u32        *num_syms;
	const char *names;
	const s32  *offsets;
	u64         rel_base;
	const char *token_table;
	const u16  *token_index;
	unsigned int have;
};
static struct bf_kall k;

/* dekode satu simbol (port dari kernel/kallsyms.c kallsyms_expand_symbol) */
static unsigned int bf_expand(unsigned int off, char *out, int maxlen)
{
	int len, skipped_first = 0;
	const unsigned char *data;
	const char *tptr;
	int rlen = 0;

	data = (const unsigned char *)k.names + off;
	len = *data++;
	off += len + 1;

	out[0] = '\0';
	while (len) {
		tptr = k.token_table + k.token_index[*data++];
		len--;
		while (*tptr) {
			if (skipped_first) {
				if (rlen < maxlen - 1)
					out[rlen++] = *tptr;
			} else {
				skipped_first = 1;
				/* karakter pertama = tipe simbol */
				out[0] = *tptr;
			}
			tptr++;
		}
	}
	out[rlen] = '\0';
	return off;
}

static unsigned long bf_sym_addr(int idx)
{
	return (unsigned long)k.rel_base + (unsigned long)k.offsets[idx];
}

/* cetak satu simbol; return 0 jika berhasil dicetak */
static int bf_emit(int idx, const char *filter)
{
	char name[256];
	unsigned long a;

	bf_expand(idx, name, sizeof(name));
	if (filter && *filter && !str_has_prefix(name + 1, filter))
		return 0;
	a = bf_sym_addr(idx);
	pr_info("0x%016lx %c %s\n", a, name[0], name + 1);
	return 1;
}

static int bf_dump_all(const char *filter)
{
	u32 n, i;
	int c = 0;

	if (!k.have) {
		pr_info("symdump: tabel kallsyms tidak ditemukan (KALLSYMS_ALL off?), "
			"pakai mode lookup\n");
		return -1;
	}
	n = *k.num_syms;
	pr_info("symdump: mulai dekode %u simbol (filter '%s')\n", n, filter ? filter : "");
	for (i = 0; i < n; i++)
		c += bf_emit(i, filter);
	pr_info("symdump: selesai, %d simbol dicetak dari %u\n", c, n);
	return 0;
}

/* ---------- mode lookup (fallback) ---------- */
static int bf_lookup_dump(const char *filter)
{
	int i, c = 0;
	unsigned long a;

	for (i = 0; i < BF_SYM_TOTAL; i++) {
		if (filter && *filter && !str_has_prefix(bf_syms[i], filter))
			continue;
		a = bf_kln(bf_syms[i]);
		if (a) {
			pr_info("0x%016lx %s\n", a, bf_syms[i]);
			c++;
		}
	}
	pr_info("symdump: lookup selesai, %d simbol dari %d\n", c, BF_SYM_TOTAL);
	return 0;
}

/* ---------- proc ---------- */
static int syms_show(struct seq_file *m, void *v)
{
	seq_printf(m, "modul     : berry_symdump\n");
	seq_printf(m, "resolver  : %s\n", bf_kln ? "aktif" : "tidak ada");
	seq_printf(m, "tabel     : %s\n", k.have ? "langsung (KALLSYMS_ALL)" : "lookup (ekspor)");
	seq_printf(m, "jumlah    : %s%u\n", k.have ? "" : "n/a ", k.have ? *k.num_syms : 0);
	seq_printf(m, "cara      : echo dump|kvm|<nama> > /proc/bfkrt_syms; lalu dmesg\n");
	return 0;
}
static int syms_open(struct inode *inode, struct file *file)
{
	return single_open(file, syms_show, NULL);
}
static ssize_t syms_write(struct file *f, const char __user *buf,
			  size_t count, loff_t *ppos)
{
	char tmp[64];
	size_t n;

	memset(tmp, 0, sizeof(tmp));
	n = count < sizeof(tmp) - 1 ? count : sizeof(tmp) - 1;
	if (copy_from_user(tmp, buf, n))
		return -EFAULT;
	strim(tmp);

	if (k.have)
		bf_dump_all(*tmp ? tmp : NULL);
	else
		bf_lookup_dump(*tmp ? tmp : NULL);
	return count;
}
static const struct file_operations syms_ops = {
	.owner  = THIS_MODULE,
	.open   = syms_open,
	.read   = seq_read,
	.write  = syms_write,
	.llseek = seq_lseek,
	.release = single_release,
};

/* ---------- init ---------- */
static int __init bfkrt_init(void)
{
	int ret;
	u64 *rb;

	ret = register_kprobe(&kln_kp);
	if (ret == 0)
		unregister_kprobe(&kln_kp);
	else
		pr_info("symdump: kallsyms_lookup_name tidak terjangkau (%d)\n", ret);
	if (!bf_kln)
		return -ENOSYS;

	/* coba ambil tabel kallsyms langsung (butuh KALLSYMS_ALL) */
	k.num_syms      = (u32 *)bf_kln("kallsyms_num_syms");
	k.names         = (const char *)bf_kln("kallsyms_names");
	k.offsets       = (const s32 *)bf_kln("kallsyms_offsets");
	k.token_table   = (const char *)bf_kln("kallsyms_token_table");
	k.token_index   = (const u16 *)bf_kln("kallsyms_token_index");
	rb              = (u64 *)bf_kln("kallsyms_relative_base");
	k.rel_base      = rb ? *rb : 0;
	k.have = (k.num_syms && k.names && k.offsets && k.token_table &&
		  k.token_index) ? 1 : 0;
	if (k.have)
		pr_info("symdump: tabel kallsyms ditemukan, %u simbol\n", *k.num_syms);
	else
		pr_info("symdump: tabel tidak lengkap, mode lookup\n");

	if (!proc_create(BFKRT_PROC_SYMD, 0644, NULL, &syms_ops)) {
		pr_err("symdump: gagal buat /proc/%s\n", BFKRT_PROC_SYMD);
		return -ENOMEM;
	}
	pr_info("symdump: siap (resolver %s)\n", bf_kln ? "aktif" : "pasif");
	return 0;
}

static void __exit bfkrt_exit(void)
{
	remove_proc_entry(BFKRT_PROC_SYMD, NULL);
	pr_info("symdump: dilepas\n");
}

module_init(bfkrt_init);
module_exit(bfkrt_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("BFKRT");
MODULE_DESCRIPTION("Berry strong symbol reveal -- decode tabel kallsyms, bypass kptr_restrict");
