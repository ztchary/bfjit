# bfjit
JIT compiler for brainf**k, because I was bored
It actually works shockingly well, and blazingly fast

### How it works
1. Reads input file
2. Compresses redundant instructions
3. Uses mmap to reserve executable memory
4. Directly writes x86-64 instructions to mmapped memory
5. Jumps to instructions

### zcu.h
ZCU is my stb style header only library for my projects. I'm planning on expanding it to actually be useful, but it's enough for this project. I'll put it in a separate repo later

### Issues
- Tape is 65536 bytes, not 30000
- No tape wrapping, going to far left or right will probably segfault, untested
- Currently x86-64 linux exclusive

### Future plans
- [ ] Fix issues
- [ ] Add support for other cpu architectures
- [ ] Add support for windows syscalls
