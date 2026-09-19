
SkyrimUpscaler.dll:     file format pei-x86-64


Disassembly of section .text:

00000001801f4eaf <.text+0x1f3eaf>:
   1801f4eaf:	e8 9c a1 00 00       	call   0x1801ff050
   1801f4eb4:	41 80 7e 5e 00       	cmp    BYTE PTR [r14+0x5e],0x0
   1801f4eb9:	74 1c                	je     0x1801f4ed7
   1801f4ebb:	41 80 be 95 01 00 00 	cmp    BYTE PTR [r14+0x195],0x0
   1801f4ec2:	00 
   1801f4ec3:	75 1c                	jne    0x1801f4ee1
   1801f4ec5:	49 8d 96 e8 04 00 00 	lea    rdx,[r14+0x4e8]
   1801f4ecc:	41 b0 01             	mov    r8b,0x1
   1801f4ecf:	49 8b ce             	mov    rcx,r14
   1801f4ed2:	e8 09 f5 ff ff       	call   0x1801f43e0
   1801f4ed7:	41 80 be 95 01 00 00 	cmp    BYTE PTR [r14+0x195],0x0
   1801f4ede:	00 
   1801f4edf:	74 07                	je     0x1801f4ee8
   1801f4ee1:	b9 01 00 00 00       	mov    ecx,0x1
   1801f4ee6:	eb 09                	jmp    0x1801f4ef1
   1801f4ee8:	41 8b 8e 20 07 00 00 	mov    ecx,DWORD PTR [r14+0x720]
   1801f4eef:	ff c9                	dec    ecx
   1801f4ef1:	33 c0                	xor    eax,eax
   1801f4ef3:	89 4d 20             	mov    DWORD PTR [rbp+0x20],ecx
   1801f4ef6:	48 89 45 a8          	mov    QWORD PTR [rbp-0x58],rax
   1801f4efa:	48 8d 4d 70          	lea    rcx,[rbp+0x70]
   1801f4efe:	48 89 45 f0          	mov    QWORD PTR [rbp-0x10],rax
   1801f4f02:	48 89 45 f8          	mov    QWORD PTR [rbp-0x8],rax
   1801f4f06:	48 89 45 00          	mov    QWORD PTR [rbp+0x0],rax
   1801f4f0a:	89 45 80             	mov    DWORD PTR [rbp-0x80],eax
   1801f4f0d:	48 89 45 e8          	mov    QWORD PTR [rbp-0x18],rax
   1801f4f11:	48 8b 85 80 01 00 00 	mov    rax,QWORD PTR [rbp+0x180]
   1801f4f18:	c5 f8 57 c0          	vxorps xmm0,xmm0,xmm0
   1801f4f1c:	c4 c1 7a 2a 46 2c    	vcvtsi2ss xmm0,xmm0,DWORD PTR [r14+0x2c]
   1801f4f22:	c5 fa 11 45 b8       	vmovss DWORD PTR [rbp-0x48],xmm0
   1801f4f27:	c4 c1 7a 10 46 08    	vmovss xmm0,DWORD PTR [r14+0x8]
   1801f4f2d:	c5 fa 11 45 c4       	vmovss DWORD PTR [rbp-0x3c],xmm0
   1801f4f32:	48 89 45 88          	mov    QWORD PTR [rbp-0x78],rax
   1801f4f36:	c4 c1 7a 2c 46 10    	vcvttss2si eax,DWORD PTR [r14+0x10]
   1801f4f3c:	c5 f0 57 c9          	vxorps xmm1,xmm1,xmm1
   1801f4f40:	c4 c1 72 2a 4e 30    	vcvtsi2ss xmm1,xmm1,DWORD PTR [r14+0x30]
   1801f4f46:	c5 fa 11 4d bc       	vmovss DWORD PTR [rbp-0x44],xmm1
   1801f4f4b:	c4 c1 7a 10 4e 0c    	vmovss xmm1,DWORD PTR [r14+0xc]
   1801f4f51:	c5 fa 11 4d c8       	vmovss DWORD PTR [rbp-0x38],xmm1
   1801f4f56:	c5 f8 57 c0          	vxorps xmm0,xmm0,xmm0
   1801f4f5a:	c5 fa 2a c0          	vcvtsi2ss xmm0,xmm0,eax
   1801f4f5e:	c4 c1 7a 2c 46 14    	vcvttss2si eax,DWORD PTR [r14+0x14]
   1801f4f64:	c5 fa 11 45 cc       	vmovss DWORD PTR [rbp-0x34],xmm0
   1801f4f69:	c4 c1 7a 10 86 ac 00 	vmovss xmm0,DWORD PTR [r14+0xac]
   1801f4f70:	00 00 
   1801f4f72:	4c 89 7d 98          	mov    QWORD PTR [rbp-0x68],r15
   1801f4f76:	45 33 ff             	xor    r15d,r15d
   1801f4f79:	c5 fa 11 75 c0       	vmovss DWORD PTR [rbp-0x40],xmm6
   1801f4f7e:	c5 fa 11 45 d8       	vmovss DWORD PTR [rbp-0x28],xmm0
   1801f4f83:	c5 fa 11 7d e0       	vmovss DWORD PTR [rbp-0x20],xmm7
   1801f4f88:	4c 89 6d 90          	mov    QWORD PTR [rbp-0x70],r13
   1801f4f8c:	4c 89 7d a0          	mov    QWORD PTR [rbp-0x60],r15
   1801f4f90:	4c 89 7d b0          	mov    QWORD PTR [rbp-0x50],r15
   1801f4f94:	48 89 75 08          	mov    QWORD PTR [rbp+0x8],rsi
   1801f4f98:	48 89 7d 10          	mov    QWORD PTR [rbp+0x10],rdi
   1801f4f9c:	44 88 7d d4          	mov    BYTE PTR [rbp-0x2c],r15b
   1801f4fa0:	c6 45 e4 01          	mov    BYTE PTR [rbp-0x1c],0x1
   1801f4fa4:	c6 45 1c 01          	mov    BYTE PTR [rbp+0x1c],0x1
   1801f4fa8:	4c 89 7d 28          	mov    QWORD PTR [rbp+0x28],r15
   1801f4fac:	c5 f0 57 c9          	vxorps xmm1,xmm1,xmm1
   1801f4fb0:	c5 f2 2a c8          	vcvtsi2ss xmm1,xmm1,eax
   1801f4fb4:	41 8b 86 58 01 00 00 	mov    eax,DWORD PTR [r14+0x158]
   1801f4fbb:	c5 fa 11 4d d0       	vmovss DWORD PTR [rbp-0x30],xmm1
   1801f4fc0:	c4 c1 7a 10 8e a8 00 	vmovss xmm1,DWORD PTR [r14+0xa8]
   1801f4fc7:	00 00 
   1801f4fc9:	89 45 18             	mov    DWORD PTR [rbp+0x18],eax
   1801f4fcc:	48 8d 45 80          	lea    rax,[rbp-0x80]
   1801f4fd0:	c5 fa 11 4d dc       	vmovss DWORD PTR [rbp-0x24],xmm1
   1801f4fd5:	c5 fc 10 00          	vmovups ymm0,YMMWORD PTR [rax]
   1801f4fd9:	c5 fc 10 88 80 00 00 	vmovups ymm1,YMMWORD PTR [rax+0x80]
   1801f4fe0:	00 
   1801f4fe1:	c5 fc 11 01          	vmovups YMMWORD PTR [rcx],ymm0
   1801f4fe5:	c5 fc 10 40 20       	vmovups ymm0,YMMWORD PTR [rax+0x20]
   1801f4fea:	c5 fc 11 41 20       	vmovups YMMWORD PTR [rcx+0x20],ymm0
   1801f4fef:	c5 fc 10 40 40       	vmovups ymm0,YMMWORD PTR [rax+0x40]
   1801f4ff4:	c5 fc 11 41 40       	vmovups YMMWORD PTR [rcx+0x40],ymm0
   1801f4ff9:	c5 fc 10 40 60       	vmovups ymm0,YMMWORD PTR [rax+0x60]
   1801f4ffe:	c5 fc 11 41 60       	vmovups YMMWORD PTR [rcx+0x60],ymm0
   1801f5003:	c5 fc 11 89 80 00 00 	vmovups YMMWORD PTR [rcx+0x80],ymm1
   1801f500a:	00 
   1801f500b:	c5 f8 10 88 a0 00 00 	vmovups xmm1,XMMWORD PTR [rax+0xa0]
   1801f5012:	00 
   1801f5013:	c5 f8 11 89 a0 00 00 	vmovups XMMWORD PTR [rcx+0xa0],xmm1
   1801f501a:	00 
   1801f501b:	48 8d 4d 70          	lea    rcx,[rbp+0x70]
   1801f501f:	c5 f8 77             	vzeroupper
   1801f5022:	ff 15 30 6d 18 00    	call   QWORD PTR [rip+0x186d30]        # 0x18037bd58
