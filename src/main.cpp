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
	static constexpr word_t BASE_ADDR = 0177560;
	static constexpr word_t ADDR_LEN = 0177560;
	enum RegId : word_t {
		RCSR = 0,
		RBUF = 1,
		XCSR = 2,
		XBUF = 3,
		MAX_REG,
	};

	void getInfo(Emu::DevInfo &info)
	{
		info.ptr = BASE_ADDR;
		info.len = ADDR_LEN;
		info.dev = this;
	}

	int fd = -1;
	struct termios attrOld;
	int Open(char const *path)
	{
		int rc;
		char const clear_str[] = "\033[H\033[J";
		struct termios attrNew;
		if ((rc = open(path, O_RDWR | O_NONBLOCK)) < 0)
			goto out;
		fd = rc;
		if ((rc = tcgetattr(fd, &attrOld)) < 0)
			goto out;
		attrNew = attrOld;
		attrNew.c_lflag &= ~(ICANON | ECHO);
		attrNew.c_cc[VTIME] = 0;
		attrNew.c_cc[VMIN] = 1;
		if ((rc = tcsetattr(fd, TCSANOW, &attrNew)) < 0)
			goto out;
		write(fd, clear_str, sizeof(clear_str));
		return 0;
	out:
		if (fd >= 0) close(fd);
		return rc;
	}
	int Close()
	{
		int rc;
		if ((rc = tcsetattr(fd, TCSANOW, &attrOld)) < 0) {
			close(fd);
			return rc;
		}
		return close(fd);
	}
	~DummyVT() { Close(); }
	void putChar(char c)
	{
		ssize_t rc = write(fd, &c, sizeof(c));
		assert(sizeof(c) == rc);
	}
	bool charReady()
	{
		struct pollfd pf;
		pf.fd = fd;
		pf.events = POLLIN;
		int rc = poll(&pf, 1, 0);
		assert(rc >= 0);
		return rc == 1;
	}
	void getChar(char *c)
	{
		ssize_t rc = read(fd, c, sizeof(*c));
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
				reg = 0x80 * charReady();
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

	if (argc != 3) {
		std::cerr << argv[0] << " <bin> <tty>\n";
		return 1;
	}
	DummyVT vt;
	if (vt.Open(argv[2]) < 0) {
		std::cerr << "vt.Open failed\n";
		return 1;
	}
	Emu::DevInfo vt_info;
	vt.getInfo(vt_info);

	Emu emu;
	emu.IOspaceRegister(vt_info);
	std::ifstream test(argv[1], std::ios::binary);
	test.read(reinterpret_cast<char*>(emu.coreMem.mem + load_addr), 64);
	test.close();
	emu.genReg[Emu::REG_PC] = load_addr;

	while (!emu.trapPending)
		emu.DbgStep(std::cout);
	return 0;
}
