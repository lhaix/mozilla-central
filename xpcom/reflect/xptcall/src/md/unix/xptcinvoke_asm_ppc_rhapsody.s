#
# -*- Mode: Asm -*-
#
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1999
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Patrick C. Beard <beard@netscape.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

# 
# ** Assumed vtable layout (obtained by disassembling with gdb):
# ** 4 bytes per vtable entry, skip 0th and 1st entries, so the mapping
# ** from index to entry is (4 * index) + 8.
#

.text
	.align 2
#
#   NS_InvokeByIndex_P(nsISupports* that, PRUint32 methodIndex,
#                    PRUint32 paramCount, nsXPTCVariant* params)
#

.globl __NS_InvokeByIndex_P	
__NS_InvokeByIndex_P:
	mflr	r0
	stw	r31,-4(r1)
#
# save off the incoming values in the callers parameter area
#		
	stw	r3,24(r1)               ; that
	stw	r4,28(r1)               ; methodIndex
	stw	r5,32(r1)               ; paramCount
	stw	r6,36(r1)               ; params
	stw	r0,8(r1)
	stwu	r1,-144(r1)             ; 24 for linkage area, 
                                        ; 8*13 for fprData area,
                                        ; 8 for saved registers,
                                        ; 8 to keep stack 16-byte aligned

# set up for and call 'invoke_count_words' to get new stack size
#	
	mr	r3,r5
	mr	r4,r6

	stwu	r1,-24(r1)	
	bl	L_invoke_count_words$stub
	lwz	r1,0(r1)

# prepare args for 'invoke_copy_to_stack' call
#		
	lwz	r4,176(r1)              ; paramCount
	lwz	r5,180(r1)              ; params
	mr	r6,r1                   ; fprData
	slwi	r3,r3,2                 ; number of stack bytes required
	addi	r3,r3,28                ; linkage area
	mr	r31,r1                  ; save original stack top
	sub	r1,r1,r3                ; bump the stack
	clrrwi	r1,r1,4                 ; keep the stack 16-byte aligned
	addi	r3,r31,144              ; act like real alloca, so 0(sp) always
	stw	r3,0(r1)                ;  points back to previous stack frame
	addi	r3,r1,28                ; parameter pointer excludes linkage area size + 'this'
	
# create "temporary" stack frame for _invoke_copy_to_stack to operate in.
	stwu	r1,-40(r1)
	bl	L_invoke_copy_to_stack$stub
# remove temporary stack frame.
	lwz	r1,0(r1)

	lfd	f1,0(r31)				
	lfd	f2,8(r31)				
	lfd	f3,16(r31)				
	lfd	f4,24(r31)				
	lfd	f5,32(r31)				
	lfd	f6,40(r31)				
	lfd	f7,48(r31)				
	lfd	f8,56(r31)				
	lfd	f9,64(r31)				
	lfd	f10,72(r31)				
	lfd	f11,80(r31)				
	lfd	f12,88(r31)				
	lfd	f13,96(r31)				
	
	lwz	r3,168(r31)             ; that
	lwz	r4,0(r3)                ; get vTable from 'that'
	lwz	r5,172(r31)             ; methodIndex
	slwi	r5,r5,2                 ; methodIndex * 4
	lwzx	r12,r5,r4               ; get function pointer

	lwz	r4,28(r1)
	lwz	r5,32(r1)
	lwz	r6,36(r1)
	lwz	r7,40(r1)
	lwz	r8,44(r1)
	lwz	r9,48(r1)
	lwz	r10,52(r1)
	
	mtlr	r12
	blrl
	
	mr      r1,r31
	lwz	r0,152(r1)
	addi    r1,r1,144
	mtlr    r0
	lwz     r31,-4(r1)

	blr

.picsymbol_stub
L_invoke_count_words$stub:
        .indirect_symbol _invoke_count_words
        mflr r0
        bcl 20,31,L1$pb
L1$pb:
        mflr r11
        addis r11,r11,ha16(L1$lz-L1$pb)
        mtlr r0
        lwz r12,lo16(L1$lz-L1$pb)(r11)
        mtctr r12
        addi r11,r11,lo16(L1$lz-L1$pb)
        bctr
.lazy_symbol_pointer
L1$lz:
        .indirect_symbol _invoke_count_words
        .long dyld_stub_binding_helper


.picsymbol_stub
L_invoke_copy_to_stack$stub:
        .indirect_symbol _invoke_copy_to_stack
        mflr r0
        bcl 20,31,L2$pb
L2$pb:
        mflr r11
        addis r11,r11,ha16(L2$lz-L2$pb)
        mtlr r0
        lwz r12,lo16(L2$lz-L2$pb)(r11)
        mtctr r12
        addi r11,r11,lo16(L2$lz-L2$pb)
        bctr
.lazy_symbol_pointer
L2$lz:
        .indirect_symbol _invoke_copy_to_stack
        .long dyld_stub_binding_helper

