// SPDX-License-Identifier: GPL-2.0
/*
 * berry_compat.c - shim simbol CFI untuk ADVAN Tab VX Lite (UMS512 / T618)
 *
 * Kernel sig-off ini DIBANGUN TANPA CONFIG_CFI_CLANG (symbol Kconfig CFI
 * tidak ada di tree), sedangkan modul vendor (sdhci-sprd, pinctrl-sprd,
 * trusty, unisoc-iommu, dll) di-build dengan -fsanitize=kcfi dan merujuk
 * __cfi_slowpath / __ubsan_handle_cfi_check_fail_abort. Shim no-op di bawah
 * membuat modul tersebut tetap bisa resolve dan ter-load.
 */
#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/types.h>

#ifndef CONFIG_CFI_CLANG

void __cfi_slowpath(uintptr_t call_site, uintptr_t type_id, void *target)
{
}
EXPORT_SYMBOL(__cfi_slowpath);

void __ubsan_handle_cfi_check_fail_abort(void *data, void *ptr, unsigned long v)
{
}
EXPORT_SYMBOL(__ubsan_handle_cfi_check_fail_abort);

#endif /* !CONFIG_CFI_CLANG */
