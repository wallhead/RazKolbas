
PDPerfPlugin.dll:     file format pei-x86-64


Disassembly of section .text:

000000018004fdb0 <.text+0x4edb0>:
   18004fdb0:	48 8b d1             	mov    rdx,rcx
   18004fdb3:	48 8b 0d 2e 80 0a 00 	mov    rcx,QWORD PTR [rip+0xa802e]        # 0x1800f7de8
   18004fdba:	48 85 c9             	test   rcx,rcx
   18004fdbd:	74 0f                	je     0x18004fdce
   18004fdbf:	48 85 d2             	test   rdx,rdx
   18004fdc2:	74 0a                	je     0x18004fdce
   18004fdc4:	48 8b 01             	mov    rax,QWORD PTR [rcx]
   18004fdc7:	48 ff a0 e8 00 00 00 	rex.W jmp QWORD PTR [rax+0xe8]
   18004fdce:	32 c0                	xor    al,al
   18004fdd0:	c3                   	ret
   18004fdd1:	cc                   	int3
   18004fdd2:	cc                   	int3
   18004fdd3:	cc                   	int3
   18004fdd4:	cc                   	int3
   18004fdd5:	cc                   	int3
   18004fdd6:	cc                   	int3
   18004fdd7:	cc                   	int3
   18004fdd8:	cc                   	int3
   18004fdd9:	cc                   	int3
   18004fdda:	cc                   	int3
   18004fddb:	cc                   	int3
   18004fddc:	cc                   	int3
   18004fddd:	cc                   	int3
   18004fdde:	cc                   	int3
   18004fddf:	cc                   	int3
   18004fde0:	48 81 ec 68 01 00 00 	sub    rsp,0x168
   18004fde7:	4c 8b 05 fa 7f 0a 00 	mov    r8,QWORD PTR [rip+0xa7ffa]        # 0x1800f7de8
   18004fdee:	4d 85 c0             	test   r8,r8
   18004fdf1:	0f 84 9b 00 00 00    	je     0x18004fe92
   18004fdf7:	48 85 c9             	test   rcx,rcx
   18004fdfa:	0f 84 92 00 00 00    	je     0x18004fe92
   18004fe00:	48 8d 54 24 20       	lea    rdx,[rsp+0x20]
   18004fe05:	b8 02 00 00 00       	mov    eax,0x2
   18004fe0a:	66 0f 1f 44 00 00    	nop    WORD PTR [rax+rax*1+0x0]
   18004fe10:	48 8d 92 80 00 00 00 	lea    rdx,[rdx+0x80]
   18004fe17:	0f 10 01             	movups xmm0,XMMWORD PTR [rcx]
   18004fe1a:	0f 10 49 10          	movups xmm1,XMMWORD PTR [rcx+0x10]
   18004fe1e:	48 8d 89 80 00 00 00 	lea    rcx,[rcx+0x80]
   18004fe25:	0f 11 42 80          	movups XMMWORD PTR [rdx-0x80],xmm0
   18004fe29:	0f 10 41 a0          	movups xmm0,XMMWORD PTR [rcx-0x60]
   18004fe2d:	0f 11 4a 90          	movups XMMWORD PTR [rdx-0x70],xmm1
   18004fe31:	0f 10 49 b0          	movups xmm1,XMMWORD PTR [rcx-0x50]
   18004fe35:	0f 11 42 a0          	movups XMMWORD PTR [rdx-0x60],xmm0
   18004fe39:	0f 10 41 c0          	movups xmm0,XMMWORD PTR [rcx-0x40]
   18004fe3d:	0f 11 4a b0          	movups XMMWORD PTR [rdx-0x50],xmm1
   18004fe41:	0f 10 49 d0          	movups xmm1,XMMWORD PTR [rcx-0x30]
   18004fe45:	0f 11 42 c0          	movups XMMWORD PTR [rdx-0x40],xmm0
   18004fe49:	0f 10 41 e0          	movups xmm0,XMMWORD PTR [rcx-0x20]
   18004fe4d:	0f 11 4a d0          	movups XMMWORD PTR [rdx-0x30],xmm1
   18004fe51:	0f 10 49 f0          	movups xmm1,XMMWORD PTR [rcx-0x10]
   18004fe55:	0f 11 42 e0          	movups XMMWORD PTR [rdx-0x20],xmm0
   18004fe59:	0f 11 4a f0          	movups XMMWORD PTR [rdx-0x10],xmm1
   18004fe5d:	48 83 e8 01          	sub    rax,0x1
   18004fe61:	75 ad                	jne    0x18004fe10
   18004fe63:	0f 10 01             	movups xmm0,XMMWORD PTR [rcx]
   18004fe66:	48 8b 41 30          	mov    rax,QWORD PTR [rcx+0x30]
   18004fe6a:	0f 10 49 10          	movups xmm1,XMMWORD PTR [rcx+0x10]
   18004fe6e:	0f 11 02             	movups XMMWORD PTR [rdx],xmm0
   18004fe71:	0f 10 41 20          	movups xmm0,XMMWORD PTR [rcx+0x20]
   18004fe75:	49 8b c8             	mov    rcx,r8
   18004fe78:	0f 11 4a 10          	movups XMMWORD PTR [rdx+0x10],xmm1
   18004fe7c:	0f 11 42 20          	movups XMMWORD PTR [rdx+0x20],xmm0
   18004fe80:	48 89 42 30          	mov    QWORD PTR [rdx+0x30],rax
   18004fe84:	48 8d 54 24 20       	lea    rdx,[rsp+0x20]
   18004fe89:	49 8b 00             	mov    rax,QWORD PTR [r8]
   18004fe8c:	ff 90 f0 00 00 00    	call   QWORD PTR [rax+0xf0]
   18004fe92:	48 81 c4 68 01 00 00 	add    rsp,0x168
   18004fe99:	c3                   	ret
   18004fe9a:	cc                   	int3
   18004fe9b:	cc                   	int3
   18004fe9c:	cc                   	int3
   18004fe9d:	cc                   	int3
   18004fe9e:	cc                   	int3
   18004fe9f:	cc                   	int3
   18004fea0:	8b d1                	mov    edx,ecx
   18004fea2:	48 8b 0d 3f 7f 0a 00 	mov    rcx,QWORD PTR [rip+0xa7f3f]        # 0x1800f7de8
   18004fea9:	48 85 c9             	test   rcx,rcx
   18004feac:	74 0a                	je     0x18004feb8
   18004feae:	48 8b 01             	mov    rax,QWORD PTR [rcx]
   18004feb1:	48 ff a0 f8 00 00 00 	rex.W jmp QWORD PTR [rax+0xf8]
   18004feb8:	c3                   	ret
