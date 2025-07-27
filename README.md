# bfjit
JIT compiler for brainf**k, because I was bored
It actually works shockingly well, and blazingly fast

### How it works
1. Reads input file
2. Compresses redundant instructions
3. Uses mmap to reserve executable memory
4. Directly writes x86-64 instructions to mmapped memory
5. Jumps to instructions

### Issues

- Tape is 65536 bytes, not 30000
- No tape wrapping, going to far left or right will probably segfault, untested
- Currently x86-64 linux exclusive

### Future plans
- [ ] Fix issues
- [ ] Add support for other cpu architectures
- [ ] Add support for windows syscalls
