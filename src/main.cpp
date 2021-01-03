#include <iostream>
#include <fstream>
#include <cstring>
#include <cassert>
#include <emu.h>
#include <termios.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <poll.h>

struct DummyVT : public Emu::DevBase {
	enum RegId : word_t {
		RCSR = 0,
		RBUF = 1,
		XCSR = 2,
		XBUF = 3,
		MAX_REG,
	};

	static constexpr word_t BASE_ADDR = 0177560;
	static constexpr word_t ADDR_LEN = MAX_REG * sizeof(word_t);

	void getInfo(Emu::DevInfo &info)
	{
		info.ptr = BASE_ADDR;
		info.len = ADDR_LEN;
		info.dev = this;
	}

	int master_fd = -1;
	int slave_fd = -1;

	int SetupAttr()
	{
		int rc;
		struct termios attr;
		if ((rc = tcgetattr(slave_fd, &attr)) < 0)
			return rc;
		attr.c_lflag &= ~(ICANON | ECHO);
		attr.c_cc[VTIME] = 0;
		attr.c_cc[VMIN] = 1;
		if ((rc = tcsetattr(slave_fd, TCSANOW, &attr)) < 0)
			return rc;

		char wnd[64];
		if ((rc = read(slave_fd, wnd, sizeof(wnd))) < 0)
			return rc;
		return 0;
	}

	int Create()
	{
		int rc;

		if ((rc = posix_openpt(O_RDWR)) < 0)
			goto out;
		master_fd = rc;

		grantpt(master_fd);
		unlockpt(master_fd);

		char buf[256];
		snprintf(buf, sizeof(buf), "-S%s/%d",
				strrchr(ptsname(master_fd), '/') + 1,
				master_fd);

		if (!fork()) {
			execlp("xterm", "xterm", buf, NULL);
			exit(1);
		}

		if ((rc = open(ptsname(master_fd), O_RDWR)) < 0)
			goto out;
		slave_fd = rc;
		if ((rc = SetupAttr()) < 0)
			goto out;

		return 0;
	out:
		Close();
		return rc;
	}

	int Close()
	{
		int tmp, rc = 0;

		if (slave_fd != -1) {
			if ((tmp = close(slave_fd)) < 0)
				rc = tmp;
			slave_fd = -1;
		}
		if (master_fd != -1) {
			if ((tmp = close(master_fd)) < 0)
				rc = tmp;
			master_fd = -1;
		}

		return rc;
	}
	~DummyVT() { Close(); }
	void putChar(char c)
	{
		ssize_t rc = write(slave_fd, &c, sizeof(c));
		assert(sizeof(c) == rc);
	}
	bool charReady()
	{
		struct pollfd pf;
		pf.fd = slave_fd;
		pf.events = POLLIN;
		int rc = poll(&pf, 1, 0);
		assert(rc >= 0);
		return rc == 1;
	}
	void getChar(char *c)
	{
		ssize_t rc = read(slave_fd, c, sizeof(*c));
		assert(sizeof(*c) == rc);
	}
	void Load(Emu &emu, word_t ptr, byte_t *buf, uint8_t sz)
	{
		word_t reg;
		char c;
		switch (ptr / 2) {
			case XCSR:
				reg = 0x80;
				break;
			case RCSR:
				reg = charReady() ? 0x80 : 0;
				break;
			case RBUF:
				if (!charReady())
					break;
				getChar(&c);
				reg = c;
				break;
			default:
				emu.RaiseTrap(Emu::TRAP_MME);
				return;
		}
		memcpy(buf, (byte_t*) &reg + ptr % 2, sz);
	}
	void Store(Emu &emu, word_t ptr, byte_t *buf, uint8_t sz)
	{
		switch (ptr / 2) {
			case XBUF:
				putChar(*buf);
				break;
			default:
				emu.RaiseTrap(Emu::TRAP_MME);
				return;
		}
	}
};


int main(int argc, char **argv)
{
	word_t const load_addr = 01000;

	if (argc != 2) {
		std::cerr << argv[0] << " <bin>\n";
		return 1;
	}
	DummyVT vt;
	if (vt.Create() < 0) {
		std::cerr << "vt.Create failed\n";
		return 1;
	}
	Emu::DevInfo vt_info;
	vt.getInfo(vt_info);

	Emu emu;
	emu.IOspaceRegister(vt_info);
	std::ifstream test(argv[1], std::ios::binary);
	test.read(reinterpret_cast<char*>(emu.coreMem.mem + load_addr),
			16 * 1024);
	test.close();
	emu.genReg[Emu::REG_PC] = load_addr;

#ifdef CONF_SHOW_CYCLES
	size_t nCycles = 0;
#endif
	while (!emu.trapPending) {
		emu.DbgStep(std::cout);
#ifdef CONF_SHOW_CYCLES
		if ((++nCycles) % (2ull << 20) == 0)
			std::cout << nCycles / (2ull << 20) << "M cycles\n";
#endif
	}
	return 0;
}
