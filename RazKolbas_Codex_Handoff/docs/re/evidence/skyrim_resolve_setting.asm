
SkyrimUpscaler.dll:     file format pei-x86-64


Disassembly of section .text:

00000001801fb158 <.text+0x1fa158>:
   1801fb158:	49 8d 5e 7c          	lea    rbx,[r14+0x7c]
   1801fb15c:	45 33 c9             	xor    r9d,r9d
   1801fb15f:	4c 8d 05 72 b4 13 00 	lea    r8,[rip+0x13b472]        # 0x1803365d8
   1801fb166:	48 8d 15 6b b3 13 00 	lea    rdx,[rip+0x13b36b]        # 0x1803364d8
   1801fb16d:	48 8d 4c 24 30       	lea    rcx,[rsp+0x30]
   1801fb172:	e8 69 37 f5 ff       	call   0x18014e8e0
   1801fb177:	89 03                	mov    DWORD PTR [rbx],eax
   1801fb179:	c5 e1 57 db          	vxorpd xmm3,xmm3,xmm3
   1801fb17d:	4c 8d 05 6c b4 13 00 	lea    r8,[rip+0x13b46c]        # 0x1803365f0
   1801fb184:	48 8d 15 4d b3 13 00 	lea    rdx,[rip+0x13b34d]        # 0x1803364d8
   1801fb18b:	48 8d 4c 24 30       	lea    rcx,[rsp+0x30]
   1801fb190:	e8 1b 38 f5 ff       	call   0x18014e9b0
   1801fb195:	c5 fb 5a c8          	vcvtsd2ss xmm1,xmm0,xmm0
   1801fb199:	c4 c1 7a 11 8e 80 00 	vmovss DWORD PTR [r14+0x80],xmm1
   1801fb1a0:	00 00 
