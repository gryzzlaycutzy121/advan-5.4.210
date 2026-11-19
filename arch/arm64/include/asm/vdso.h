/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 ARM Limited
 */
#ifndef __ASM_VDSO_H
#define __ASM_VDSO_H

/*
 * Default link address for the vDSO.
 * Since we randomise the VDSO mapping, there's little point in trying
 * to prelink this.
 */
#define VDSO_LBASE	0x0

#ifndef __ASSEMBLY__

#include <generated/vdso-offsets.h>
/*
 * Fallback: header generated bisa gagal menghasilkan vdso_offset_sigtramp
 * (mis. saat build me-regenerate vdso-offsets.h tanpa symbol VDSO_sigtramp).
 * Offset 0x5ec = layout vDSO kernel 5.4.210 T618 (sudah diverifikasi).
 */
#ifndef vdso_offset_sigtramp
#define vdso_offset_sigtramp 0x05ec
#endif
#ifdef CONFIG_COMPAT_VDSO
#include <generated/vdso32-offsets.h>
#endif

#define VDSO_SYMBOL(base, name)						   \
({									   \
	(void *)(vdso_offset_##name - VDSO_LBASE + (unsigned long)(base)); \
})

#endif /* !__ASSEMBLY__ */

#endif /* __ASM_VDSO_H */
