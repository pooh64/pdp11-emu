switch (masked) {
	case 00011: I_OP(setd);
	case 00002: I_OP(seti);
	default:
		    I_OP(fpu_unknown);
};
