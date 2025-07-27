#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "zcu.h"

#define PROGRAM_MEM 30000

#define SYSCALL_OP_SIZE 2
#define INC_DEC_OP_SIZE 3
#define LEFT_RIGHT_OP_SIZE 7
#define LOOP_OP_SIZE 10
#define READ_WRITE_OP_SIZE 16

typedef void(*runner)(char *, size_t, size_t);

typedef struct {
	char *map;
	size_t length;
} mmapped_file;

typedef struct {
	char op;
	int operand;
	size_t instr;
} bfop;

typedef struct {
	size_t instr;
	ssize_t cap;
	ssize_t count;
	bfop *items;
} bfops;

enum bfop_type {
	OP_INC   = '+',
	OP_DEC   = '-',
	OP_LEFT  = '<',
	OP_RIGHT = '>',
	OP_OPEN  = '[',
	OP_CLOSE = ']',
	OP_READ  = ',',
	OP_WRITE = '.',
};

mmapped_file mmap_read_file(char *path) {
	int fd;
	struct stat statbuf;
	char *map;
	mmapped_file result = {0};

	fd = open(path, O_RDONLY);
	if (fd < 0) return result;
	if (fstat(fd, &statbuf) < 0) return result;

	result.length = statbuf.st_size;

	map = mmap(NULL, result.length, PROT_READ, MAP_PRIVATE, fd, 0);
	if (map == MAP_FAILED) return result;

	result.map = map;
	return result;
}

bfops lex_bf(mmapped_file file) {
	bfops ops = {0};
	size_t cop = 0;
	for (char *opc = file.map; opc < file.map + file.length; opc++) {
		bfop current_op = {0};
		current_op.instr = ops.instr;
		switch (*opc) {
			case OP_LEFT:
			case OP_RIGHT: {
				if (ops.count > 0 && ops.items[ops.count-1].op == *opc) {
					ops.items[ops.count-1].operand++;
					continue;
				}
				current_op.op = *opc;
				current_op.operand = 1;
				zcuda_append(&ops, current_op);
				cop++;
				ops.instr += LEFT_RIGHT_OP_SIZE;
			} break;
			case OP_INC:
			case OP_DEC: {
				if (ops.count > 0 && ops.items[ops.count-1].op == *opc) {
					ops.items[ops.count-1].operand++;
					continue;
				}
				current_op.op = *opc;
				current_op.operand = 1;
				zcuda_append(&ops, current_op);
				cop++;
				ops.instr += INC_DEC_OP_SIZE;
			} break;
			case OP_OPEN: {
				current_op.op = OP_OPEN;
				current_op.operand = -1;
				zcuda_append(&ops, current_op);
				cop++;
				ops.instr += LOOP_OP_SIZE;
			} break;
			case OP_CLOSE: {
				current_op.op = OP_CLOSE;
				int search = ops.count;
				while (--search >= 0) {
					if (ops.items[search].operand == -1) break;
				}
				if (search < 0) {
					zcuda_free(&ops);
					ops.count = -1;
					return ops;
				}
				ops.items[search].operand = cop + 1;
				current_op.operand = search + 1;
				zcuda_append(&ops, current_op);
				cop++;
				ops.instr += LOOP_OP_SIZE;
			} break;
			case OP_READ:
			case OP_WRITE: {
				if (ops.count > 0 && ops.items[ops.count-1].op == *opc) {
					ops.items[ops.count-1].operand++;
					ops.instr += SYSCALL_OP_SIZE;
					continue;
				}
				current_op.op = *opc;
				current_op.operand = 1;
				zcuda_append(&ops, current_op);
				cop++;
				ops.instr += READ_WRITE_OP_SIZE;
			} break;
		}
	}
	for (size_t i = 0; i < ops.count; i++) {
		if (ops.items[i].operand == -1) {
			zcuda_free(&ops);
			ops.count = -1;
			return ops;
		}
	}
	return ops;
}



void compile_bf(bfops ops, char *mem) {
	memcpy(mem, "\x48\x89\xfe", 3);
	mem += 3;
	for (int i = 0; i < ops.count; i++) {
		bfop cop = ops.items[i];
		switch (cop.op) {
			case OP_INC: {
				memcpy(mem + cop.instr, "\x80\x06", 2);
				memcpy(mem + cop.instr + 2, (char *)&cop.operand, 1);
			} break;
			case OP_DEC: {
				memcpy(mem + cop.instr, "\x80\x2e", 2);
				memcpy(mem + cop.instr + 2, (char *)&cop.operand, 1);
			} break;
			case OP_LEFT: {
				memcpy(mem + cop.instr, "\x48\x81\xee", 3);
				memcpy(mem + cop.instr + 3, (char *)&cop.operand, 4);
			} break;
			case OP_RIGHT: {
				memcpy(mem + cop.instr, "\x48\x81\xc6", 3);
				memcpy(mem + cop.instr + 3, (char *)&cop.operand, 4);
			} break;
			case OP_OPEN: {
				int offset = ops.items[cop.operand].instr - (cop.instr + LOOP_OP_SIZE);
				memcpy(mem + cop.instr, "\x8a\x06\x84\xc0\x0f\x84", 6);
				memcpy(mem + cop.instr + 6, (char *)&offset, 4);
			} break;
			case OP_CLOSE: {
				int offset = ops.items[cop.operand].instr - (cop.instr + LOOP_OP_SIZE);
				memcpy(mem + cop.instr, "\x8a\x06\x84\xc0\x0f\x85", 6);
				memcpy(mem + cop.instr + 6, (char *)&offset, 4);
			} break;
			case OP_READ: {
				memcpy(mem + cop.instr, "\x48\xc7\xc0\x00\x00\x00\x00\x48\xc7\xc7\x00\x00\x00\x00", 14);
				for (size_t i = 0; i < cop.operand; i++) {
					memcpy(mem + cop.instr + 14 + i*2, "\x0f\x05", 2);
				}
			} break;
			case OP_WRITE: {
				memcpy(mem + cop.instr, "\x48\xc7\xc0\x01\x00\x00\x00\x48\xc7\xc7\x01\x00\x00\x00", 14);
				for (size_t i = 0; i < cop.operand; i++) {
					memcpy(mem + cop.instr + 14 + i*2, "\x0f\x05", 2);
				}
			} break;
		}
	}
	mem[ops.instr] = 0xc3;
}

int main(int argc, char **argv) {
	int result = 1;
	mmapped_file file;
	bfops ops;
	char *runner_mem, *prog_mem;
	zcu_assert(argc >= 2, "file not provided");

	file = mmap_read_file(argv[1]);
	zcu_assert(file.map != NULL, "failed to read file");

	ops = lex_bf(file);

	munmap(file.map, file.length);

	zcu_assert(ops.count > 0, "mismatched brackets");

	runner_mem = mmap(NULL, ops.instr + 4, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
	zcu_assert(runner_mem != MAP_FAILED, "couldn't allocate runner memory");

	prog_mem = mmap(NULL, PROGRAM_MEM, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
	zcu_assert(runner_mem != MAP_FAILED, "couldn't allocate program memory");

	compile_bf(ops, runner_mem);
	((runner)(runner_mem))(prog_mem, 0, 1);


	result = 0;
defer:
	if (file.map != NULL) munmap(file.map, file.length);
	if (runner_mem != NULL) munmap(runner_mem, ops.instr);
	if (prog_mem != NULL) munmap(prog_mem, PROGRAM_MEM);
	if (ops.items != NULL) zcuda_free(&ops);

	return result;
}


