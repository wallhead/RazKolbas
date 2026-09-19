
PDPerfPlugin.dll:     file format pei-x86-64


Disassembly of section .text:

0000000180029e10 <.text+0x28e10>:
   180029e10:	83 b9 14 01 00 00 02 	cmp    DWORD PTR [rcx+0x114],0x2
   180029e17:	75 06                	jne    0x180029e1f
   180029e19:	b8 02 00 00 00       	mov    eax,0x2
   180029e1e:	c3                   	ret
   180029e1f:	f3 0f 10 05 b5 bb 0a 	movss  xmm0,DWORD PTR [rip+0xabbb5]        # 0x1800d59dc
   180029e26:	00 
   180029e27:	33 c0                	xor    eax,eax
   180029e29:	0f 2f c1             	comiss xmm0,xmm1
   180029e2c:	0f 97 c0             	seta   al
   180029e2f:	c3                   	ret
