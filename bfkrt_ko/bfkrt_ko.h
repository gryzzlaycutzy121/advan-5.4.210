/* SPDX-License-Identifier: GPL-2.0 */
/*
 * bfkrt_ko.h -- definisi bersama ketiga modul BFKRT
 *   berryfetcher_kernel.ko   : ambil info kernel dari dalam ruang kernel
 *   berry_trig_all.ko        : rekam register saat fungsi core dipanggil
 *   berry_m_virtkrn_obs.ko   : amati urutan panggilan + status kunci (deadlock)
 *
 * Aturan yang dipatuhi ketiga modul (lihat ko/README.md):
 *   * handler kprobe TIDAK PERNAH tidur, tidak kmalloc, tidak printk berulang;
 *     hanya menulis ke buffer statis di .bss dengan spinlock irqsave.
 *   * seluruh keputusan berat (diff, verdict, dekompresi) ada di userspace
 *     (bfkrt). Modul hanya MEREKAM.
 *   * semuanya default MATI: ditulis "on" ke node /proc bila mau dipakai,
 *     supaya tidak menambah beban pada boot yang sedang diukur.
 */
#ifndef BFKRT_KO_H
#define BFKRT_KO_H

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/atomic.h>

#define BFKRT_PROC_INFO  "bfkrt_info"
#define BFKRT_PROC_SYM   "bfkrt_sym"
#define BFKRT_PROC_TRIG  "bfkrt_trig"
#define BFKRT_PROC_OBS   "bfkrt_obs"
#define BFKRT_PROC_SYMD  "bfkrt_syms"
#define BFKRT_PROC_LKVM  "bfkrt_lkvm"

/* ukuran ring: 4096 entri x 96 byte = 384 KiB, muat di .bss modul */
#define BFKRT_RING 4096

static inline const char *bfkrt_task_name(struct task_struct *t)
{
	return t ? t->comm : "?";
}

/*
 * CFI stub (kernel stock = Android CFI_CLANG_SHADOW / KCFI, clang 12.0.5).
 *
 * Kernel memanggil mod->cfi_check pada SETIAP indirect-call ke fungsi modul
 * (termasuk cleanup_module saat rmmod). Tanpa simbol __cfi_check global,
 * mod->cfi_check = NULL -> kernel/cfi.c cfi_slowpath_handler ->
 * find_module_cfi_check -> NULL -> handle_cfi_failure -> panic()
 * (terbukti: "CFI failure (target: cleanup_module+0x0/0xcec [mod])").
 *
 * Stub di bawah membuat mod->cfi_check terisi (lookup by name "__cfi_check"),
 * sehingga unload/rmmod tidak lagi panic. Konsekuensi: call-site kernel TIDAK
 * memvalidasi type-hash utk call INTO modul ini (dianggap lulus). Memang itu
 * tujuannya: modul analisis di-load di kernel stock yang hash-nya tidak kita
 * kendalikan. Tidak dipakai -fsanitize=cfi-icall (butuh -flto + clang KCFI
 * yang tidak ada di Ubuntu clang-12).
 */
bool __cfi_check(uint64_t id, void *ptr, void *diag)
{
	(void)id; (void)ptr; (void)diag;
	return true;
}

#endif /* BFKRT_KO_H */
