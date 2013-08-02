
.SUFFIXES:

RAGEL = ../../ragel/ragel
MODE =
RUSTC = rustc -O

all: url concurrent rpn atoi clang gotocallret scan1 scan2

url: url.rl
	$(RAGEL) $(MODE) --host-lang=rust url_authority.rl -o url_authority.rs
	$(RAGEL) $(MODE) --host-lang=rust url.rl -o url.rs
	$(RUSTC) --test url.rs

concurrent: concurrent.rl
	$(RAGEL) $(MODE) --host-lang=rust concurrent.rl -o concurrent.rs
	$(RUSTC) concurrent.rs

rpn: rpn.rl
	$(RAGEL) $(MODE) --host-lang=rust rpn.rl -o rpn.rs
	$(RUSTC) --test rpn.rs

atoi: atoi.rl
	$(RAGEL) $(MODE) --host-lang=rust atoi.rl -o atoi.rs
	$(RUSTC) --test atoi.rs

clang: clang.rl
	$(RAGEL) $(MODE) --host-lang=rust clang.rl -o clang.rs
	$(RUSTC) clang.rs

gotocallret: gotocallret.rl
	$(RAGEL) $(MODE) --host-lang=rust gotocallret.rl -o gotocallret.rs
	$(RUSTC) gotocallret.rs

scan1: scan1.rl
	$(RAGEL) $(MODE) --host-lang=rust scan1.rl -o scan1.rs
	$(RUSTC) scan1.rs

scan2: scan2.rl
	$(RAGEL) $(MODE) --host-lang=rust scan2.rl -o scan2.rs
	$(RUSTC) scan2.rs

.PHONY: clean
clean:
	rm -f *.o
	rm -f $(subst .rl,.rs,$(wildcard *.rl))
	rm -f $(subst .rl,,$(wildcard *.rl))
