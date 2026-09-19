
SkyrimUpscaler.dll:     file format pei-x86-64


Disassembly of section .text:

00000001801f43e0 <.text+0x1f33e0>:
   1801f43e0:	4c 8b dc             	mov    r11,rsp
   1801f43e3:	49 89 5b 20          	mov    QWORD PTR [r11+0x20],rbx
   1801f43e7:	55                   	push   rbp
   1801f43e8:	56                   	push   rsi
   1801f43e9:	41 56                	push   r14
   1801f43eb:	49 8d 6b 88          	lea    rbp,[r11-0x78]
   1801f43ef:	48 81 ec 60 01 00 00 	sub    rsp,0x160
   1801f43f6:	80 b9 95 01 00 00 00 	cmp    BYTE PTR [rcx+0x195],0x0
   1801f43fd:	45 0f b6 f0          	movzx  r14d,r8b
   1801f4401:	48 8b f2             	mov    rsi,rdx
   1801f4404:	48 8b d9             	mov    rbx,rcx
   1801f4407:	0f 85 e2 01 00 00    	jne    0x1801f45ef
   1801f440d:	80 79 5c 00          	cmp    BYTE PTR [rcx+0x5c],0x0
   1801f4411:	0f 84 d8 01 00 00    	je     0x1801f45ef
   1801f4417:	80 79 5d 00          	cmp    BYTE PTR [rcx+0x5d],0x0
   1801f441b:	0f 84 ce 01 00 00    	je     0x1801f45ef
   1801f4421:	48 85 d2             	test   rdx,rdx
   1801f4424:	0f 84 c5 01 00 00    	je     0x1801f45ef
   1801f442a:	48 8b 02             	mov    rax,QWORD PTR [rdx]
   1801f442d:	48 85 c0             	test   rax,rax
   1801f4430:	0f 84 b9 01 00 00    	je     0x1801f45ef
   1801f4436:	48 8b 89 40 05 00 00 	mov    rcx,QWORD PTR [rcx+0x540]
   1801f443d:	48 85 c9             	test   rcx,rcx
   1801f4440:	0f 84 a9 01 00 00    	je     0x1801f45ef
   1801f4446:	48 8b 93 98 05 00 00 	mov    rdx,QWORD PTR [rbx+0x598]
   1801f444d:	48 85 d2             	test   rdx,rdx
   1801f4450:	0f 84 99 01 00 00    	je     0x1801f45ef
   1801f4456:	c5 f9 ef c0          	vpxor  xmm0,xmm0,xmm0
   1801f445a:	c5 f1 ef c9          	vpxor  xmm1,xmm1,xmm1
   1801f445e:	49 89 7b 10          	mov    QWORD PTR [r11+0x10],rdi
   1801f4462:	4d 89 7b 18          	mov    QWORD PTR [r11+0x18],r15
   1801f4466:	45 33 ff             	xor    r15d,r15d
   1801f4469:	4c 89 7c 24 48       	mov    QWORD PTR [rsp+0x48],r15
   1801f446e:	41 8b ff             	mov    edi,r15d
   1801f4471:	44 89 7c 24 20       	mov    DWORD PTR [rsp+0x20],r15d
   1801f4476:	48 89 44 24 28       	mov    QWORD PTR [rsp+0x28],rax
   1801f447b:	48 89 54 24 30       	mov    QWORD PTR [rsp+0x30],rdx
   1801f4480:	48 89 4c 24 38       	mov    QWORD PTR [rsp+0x38],rcx
   1801f4485:	48 89 44 24 40       	mov    QWORD PTR [rsp+0x40],rax
   1801f448a:	c5 fa 7f 44 24 60    	vmovdqu XMMWORD PTR [rsp+0x60],xmm0
   1801f4490:	c5 f8 11 4c 24 70    	vmovups XMMWORD PTR [rsp+0x70],xmm1
   1801f4496:	c5 f8 11 45 80       	vmovups XMMWORD PTR [rbp-0x80],xmm0
   1801f449b:	c5 f8 11 4d 90       	vmovups XMMWORD PTR [rbp-0x70],xmm1
   1801f44a0:	c5 f8 11 45 a0       	vmovups XMMWORD PTR [rbp-0x60],xmm0
   1801f44a5:	c5 f8 11 4d b0       	vmovups XMMWORD PTR [rbp-0x50],xmm1
   1801f44aa:	c5 f8 11 45 c0       	vmovups XMMWORD PTR [rbp-0x40],xmm0
   1801f44af:	c5 f8 11 4d d0       	vmovups XMMWORD PTR [rbp-0x30],xmm1
   1801f44b4:	c5 f8 11 45 e0       	vmovups XMMWORD PTR [rbp-0x20],xmm0
   1801f44b9:	c5 f8 11 4d f0       	vmovups XMMWORD PTR [rbp-0x10],xmm1
   1801f44be:	45 84 c0             	test   r8b,r8b
   1801f44c1:	75 29                	jne    0x1801f44ec
   1801f44c3:	48 8b cb             	mov    rcx,rbx
   1801f44c6:	e8 05 fe ff ff       	call   0x1801f42d0
   1801f44cb:	84 c0                	test   al,al
   1801f44cd:	74 1d                	je     0x1801f44ec
   1801f44cf:	48 8b 8b 38 04 00 00 	mov    rcx,QWORD PTR [rbx+0x438]
   1801f44d6:	48 85 c9             	test   rcx,rcx
   1801f44d9:	75 0c                	jne    0x1801f44e7
   1801f44db:	48 8b 8b e0 03 00 00 	mov    rcx,QWORD PTR [rbx+0x3e0]
   1801f44e2:	48 85 c9             	test   rcx,rcx
   1801f44e5:	74 38                	je     0x1801f451f
   1801f44e7:	48 8b 3e             	mov    rdi,QWORD PTR [rsi]
   1801f44ea:	eb 33                	jmp    0x1801f451f
   1801f44ec:	48 8b cb             	mov    rcx,rbx
   1801f44ef:	e8 3c fe ff ff       	call   0x1801f4330
   1801f44f4:	84 c0                	test   al,al
   1801f44f6:	74 21                	je     0x1801f4519
   1801f44f8:	48 8b bb 90 04 00 00 	mov    rdi,QWORD PTR [rbx+0x490]
   1801f44ff:	48 85 ff             	test   rdi,rdi
   1801f4502:	74 15                	je     0x1801f4519
   1801f4504:	48 8b 8b e0 03 00 00 	mov    rcx,QWORD PTR [rbx+0x3e0]
   1801f450b:	48 85 c9             	test   rcx,rcx
   1801f450e:	74 09                	je     0x1801f4519
   1801f4510:	44 38 bb ca 00 00 00 	cmp    BYTE PTR [rbx+0xca],r15b
   1801f4517:	75 06                	jne    0x1801f451f
   1801f4519:	49 8b ff             	mov    rdi,r15
   1801f451c:	49 8b cf             	mov    rcx,r15
   1801f451f:	0f b6 43 78          	movzx  eax,BYTE PTR [rbx+0x78]
   1801f4523:	48 85 ff             	test   rdi,rdi
   1801f4526:	c5 fa 10 43 10       	vmovss xmm0,DWORD PTR [rbx+0x10]
   1801f452b:	c5 fa 10 4b 14       	vmovss xmm1,DWORD PTR [rbx+0x14]
   1801f4530:	88 45 18             	mov    BYTE PTR [rbp+0x18],al
   1801f4533:	8b 43 64             	mov    eax,DWORD PTR [rbx+0x64]
   1801f4536:	89 45 1c             	mov    DWORD PTR [rbp+0x1c],eax
   1801f4539:	0f b6 43 5c          	movzx  eax,BYTE PTR [rbx+0x5c]
   1801f453d:	c5 fa 11 45 00       	vmovss DWORD PTR [rbp+0x0],xmm0
   1801f4542:	c5 f8 10 43 68       	vmovups xmm0,XMMWORD PTR [rbx+0x68]
   1801f4547:	48 89 7c 24 50       	mov    QWORD PTR [rsp+0x50],rdi
   1801f454c:	48 8b bc 24 88 01 00 	mov    rdi,QWORD PTR [rsp+0x188]
   1801f4553:	00 
   1801f4554:	88 45 22             	mov    BYTE PTR [rbp+0x22],al
   1801f4557:	c5 fa 11 4d 04       	vmovss DWORD PTR [rbp+0x4],xmm1
   1801f455c:	48 89 4c 24 58       	mov    QWORD PTR [rsp+0x58],rcx
   1801f4561:	c5 f8 11 45 08       	vmovups XMMWORD PTR [rbp+0x8],xmm0
   1801f4566:	66 44 89 7d 20       	mov    WORD PTR [rbp+0x20],r15w
   1801f456b:	74 09                	je     0x1801f4576
   1801f456d:	c6 45 23 01          	mov    BYTE PTR [rbp+0x23],0x1
   1801f4571:	48 85 c9             	test   rcx,rcx
   1801f4574:	75 04                	jne    0x1801f457a
   1801f4576:	44 88 7d 23          	mov    BYTE PTR [rbp+0x23],r15b
   1801f457a:	4c 89 7d 28          	mov    QWORD PTR [rbp+0x28],r15
   1801f457e:	45 84 f6             	test   r14b,r14b
   1801f4581:	75 13                	jne    0x1801f4596
   1801f4583:	44 38 7b 5e          	cmp    BYTE PTR [rbx+0x5e],r15b
   1801f4587:	75 0d                	jne    0x1801f4596
   1801f4589:	c6 45 30 01          	mov    BYTE PTR [rbp+0x30],0x1
   1801f458d:	44 39 bb d4 00 00 00 	cmp    DWORD PTR [rbx+0xd4],r15d
   1801f4594:	7f 04                	jg     0x1801f459a
   1801f4596:	44 88 7d 30          	mov    BYTE PTR [rbp+0x30],r15b
   1801f459a:	c5 f8 10 83 80 00 00 	vmovups xmm0,XMMWORD PTR [rbx+0x80]
   1801f45a1:	00 
   1801f45a2:	8b 43 7c             	mov    eax,DWORD PTR [rbx+0x7c]
   1801f45a5:	c5 f8 11 45 38       	vmovups XMMWORD PTR [rbp+0x38],xmm0
   1801f45aa:	c5 fa 10 83 90 00 00 	vmovss xmm0,DWORD PTR [rbx+0x90]
   1801f45b1:	00 
   1801f45b2:	89 45 34             	mov    DWORD PTR [rbp+0x34],eax
   1801f45b5:	0f b6 83 94 00 00 00 	movzx  eax,BYTE PTR [rbx+0x94]
   1801f45bc:	c5 fa 11 45 48       	vmovss DWORD PTR [rbp+0x48],xmm0
   1801f45c1:	88 45 4c             	mov    BYTE PTR [rbp+0x4c],al
   1801f45c4:	44 38 bb 95 00 00 00 	cmp    BYTE PTR [rbx+0x95],r15b
   1801f45cb:	74 06                	je     0x1801f45d3
   1801f45cd:	44 89 7d 50          	mov    DWORD PTR [rbp+0x50],r15d
   1801f45d1:	eb 09                	jmp    0x1801f45dc
   1801f45d3:	8b 83 98 00 00 00    	mov    eax,DWORD PTR [rbx+0x98]
   1801f45d9:	89 45 50             	mov    DWORD PTR [rbp+0x50],eax
   1801f45dc:	48 8d 4c 24 20       	lea    rcx,[rsp+0x20]
   1801f45e1:	ff 15 99 77 18 00    	call   QWORD PTR [rip+0x187799]        # 0x18037bd80
   1801f45e7:	4c 8b bc 24 90 01 00 	mov    r15,QWORD PTR [rsp+0x190]
   1801f45ee:	00 
   1801f45ef:	48 8b 9c 24 98 01 00 	mov    rbx,QWORD PTR [rsp+0x198]
   1801f45f6:	00 
   1801f45f7:	48 81 c4 60 01 00 00 	add    rsp,0x160
   1801f45fe:	41 5e                	pop    r14
   1801f4600:	5e                   	pop    rsi
   1801f4601:	5d                   	pop    rbp
   1801f4602:	c3                   	ret
