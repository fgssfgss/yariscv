//
// Created by andrew on 9/8/21.
//

#include "console.h"
#include "uart.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

#define BUFSIZE 256
#define INDEX(i) ((start + i) % BUFSIZE)
#define GET(i) (buffer[INDEX(i)])

static struct termios old_tty;
static int old_flags;
static bool esc_state = false;
static uint8_t buffer[BUFSIZE];
static long start = 0;
static long len = 0;

static void write_to_buffer(uint8_t ch);
static void exit_callback();

void console_init() {
	struct termios tty;

	memset(&tty, 0, sizeof(tty));
	tcgetattr(STDIN_FILENO, &tty);
	old_tty = tty;
	old_flags = fcntl(STDIN_FILENO, F_GETFL);
	fcntl(STDIN_FILENO, F_SETFL, old_flags | O_NONBLOCK);

	tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP
	                 | INLCR | IGNCR | ICRNL | IXON);
	tty.c_oflag |= OPOST;
	tty.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN);
	tty.c_lflag &= ~ISIG;
	tty.c_cflag &= ~(CSIZE | PARENB);
	tty.c_cflag |= CS8;
	tty.c_cc[VMIN] = 1;
	tty.c_cc[VTIME] = 0;

	tcsetattr(STDIN_FILENO, TCSANOW, &tty);
	atexit(exit_callback);

	printf("Console inited!\n");
}

void console_read(void) {
	uint8_t ch = -1;

again:
	if (read(STDIN_FILENO, &ch, 1) <= 0) {
		return;
	}

	if (esc_state) {
		esc_state = false;
		switch (ch) {
			case 'x':
				printf("Exiting...\n");
				exit(0);
			case 'h':
				printf("Ctrl+A and X - exit\nCtrl+A and H - help\n");
				break;
			case 1:
			default:
				write_to_buffer(ch);
				return;
		}
	} else {
		if (ch == 1) {
			esc_state = true;
			goto again;
		} else {
			write_to_buffer(ch);
			return;
		}
	}
}

uint8_t console_get_char(void) {
	if (len != 0) {
		uint8_t ch = GET(len - 1);
		start = (start + 1) % BUFSIZE;
		--len;
		return ch;
	} else {
		return -1;
	}
}

long console_is_empty(void) {
	return len == 0;
}

static void write_to_buffer(uint8_t ch) {
	buffer[INDEX(len)] = ch;
	++len;
}

static void exit_callback() {
	tcsetattr(STDIN_FILENO, TCSANOW, &old_tty);
	fcntl(STDIN_FILENO, F_SETFL, old_flags);
}