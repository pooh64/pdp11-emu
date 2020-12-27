switch (opcode >> 12) {	// top 1+3 bit
	case 001: I_OP(mov);
	//case 006: I_OP(add);
	//case 016: I_OP(sub);
	//case 002: I_OP(cmp);
	//case 005: I_OP(bis);
	//case 003: I_OP(bit);
	//case 004: I_OP(bic);

	case 011: I_OP(movb);
	//case 012: I_OP(cmpb);
	//case 015: I_OP(bisb);
	//case 013: I_OP(bitb);
	//case 014: I_OP(bicb);

	default:
switch (opcode >> 9) { // top 1+3+3 bit
	//case 0072: I_OP(ash);
	//case 0073: I_OP(ashc);
	//case 0070: I_OP(mul);
	//case 0071: I_OP(div);
	//case 0074: I_OP(xor);
	case 0004: I_OP(jsr);
	//case 0077: I_OP(sob);

	default:
switch ((opcode >> 6) & ~((word_t) 3)) { // top 1+3+3+3, 2 lsb zeroed
	case 00004: I_OP(br);
	//case 00014: I_OP(beq);
	//case 00010: I_OP(bne);
	//case 01004: I_OP(bmi);
	//case 01000: I_OP(bpl);
	//case 01034: I_OP(bcs);
	//case 01030: I_OP(bcc);
	//case 01024: I_OP(bvs);
	//case 01020: I_OP(bvc);
	//case 01040: I_OP(emt);
	//case 01044: I_OP(trap);

	default:
switch (opcode >> 6) { // top 1+3+3+3 bit
	//case 00050: I_OP(clr);
	//case 00051: I_OP(com);
	//case 00052: I_OP(inc);
	//case 00053: I_OP(dec);
	//case 00054: I_OP(neg);
	//case 00055: I_OP(adc);
	//case 00056: I_OP(sbc);
	//case 00057: I_OP(tst);
	//case 00060: I_OP(ror);
	//case 00061: I_OP(rol);
	//case 00062: I_OP(asr);
	//case 00063: I_OP(asl);

	//case 01050: I_OP(clrb);
	//case 01051: I_OP(comb);
	//case 01052: I_OP(incb);
	//case 01053: I_OP(decb);
	//case 01054: I_OP(negb);
	//case 01055: I_OP(adcb);
	//case 01056: I_OP(sbcb);
	//case 01057: I_OP(tstb);
	//case 01060: I_OP(rorb);
	//case 01061: I_OP(rolb);
	//case 01062: I_OP(asrb);
	//case 01063: I_OP(aslb);

	//case 00067: I_OP(sxt);
	//case 00003: I_OP(swab);
	//case 00001: I_OP(jmp);
	//case 00066: I_OP(mtpi);
	//case 01066: I_OP(mtpid);
	//case 00064: I_OP(mark);
	//case 00065: I_OP(mfpi);
	//case 01065: I_OP(mfpd);

	default:
	if (((opcode >> 3) & ~(3)) == 000024) { // top 1+3+3+3+3, 2 lsb zeroed
		I_OP(ccode_op);
	}
switch (opcode >> 3) { // top 1+3+3+3+3 bit
	case 000020: I_OP(rts);
	//case 000023: I_OP(spl);

	default:
switch (opcode) {
	//case 0000000: I_OP(halt);
	//case 0000001: I_OP(wait);
	//case 0000002: I_OP(rti);
	//case 0000003: I_OP(bpt);
	//case 0000004: I_OP(iot);
	//case 0000005: I_OP(reset);
	//case 0000006: I_OP(rtt);

	default: I_OP(unknown);
}}}}}}
