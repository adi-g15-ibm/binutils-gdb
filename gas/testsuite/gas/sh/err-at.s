! { dg-do assemble }

! Make sure we reject the invalid uses below:
start:
	mov.l	r1,@r0		! ok
	mov.l	r1,@(r0)	! { dg-error "syntax error" }
! { dg-bogus "invalid operands for opcode" "" { xfail *-*-* } 6 }
	mov.l	r1,@(r0,)	! { dg-error "syntax error" }
! { dg-bogus "invalid operands for opcode" "" { xfail *-*-* } 8 }
	mov.l	r1,@(r0,r0)	! ok
	mov.l	r1,@(r0,r1)	! ok
	mov.l	r1,@(r1,r0)	! { dg-error "must be" }
