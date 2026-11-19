# SPDX-License-Identifier: GPL-2.0
hostprogs-y	:= mdp
HOST_EXTRACFLAGS += \
	-I$(srctree)/arch/$(SRCARCH)/include/uapi \
	-I$(objtree)/arch/$(SRCARCH)/include/generated/uapi \
	-I$(srctree)/include/uapi -I$(srctree)/include \
	-I$(srctree)/security/selinux/include -I$(objtree)/include

always		:= $(hostprogs-y)
clean-files	:= policy.* file_contexts
