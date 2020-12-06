switch (opcode >> 12) {
	case 0b0001:
		I_OP(mov);
	case 0b1001:
		I_OP(movb);
	default:
		I_OP(unknown);
}
