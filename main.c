#include "src/yariscvemu.h"

int main(int argc, char *argv[]) {
	int ret = yariscvemu_init(argc, argv);

	if (ret != 0) {
		return ret;
	}

	yariscvemu_running(true);

	yariscvemu_run();
}
