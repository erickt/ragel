
.SUFFIXES:

RAGEL = ../../ragel/ragel
MODE =

all: url concurrent rpn atoi clang gotocallret scan1 scan2

url: url.rl
	$(RAGEL) $(MODE) --host-lang=rust url.rl -o url.rs
	$(RAGEL) $(MODE) --host-lang=rust url_authority.rl -o url_authority.rs
	rustc --test url.rc

concurrent: concurrent.rl
	$(RAGEL) $(MODE) --host-lang=rust -o concurrent.rs concurrent.rl
	rustc concurrent.rs

rpn: rpn.rl
	$(RAGEL) $(MODE) --host-lang=rust -o rpn.rs rpn.rl
	rustc --test rpn.rs

atoi: atoi.rl
	$(RAGEL) $(MODE) --host-lang=rust -o atoi.rs atoi.rl
	rustc atoi.rs

clang: clang.rl
	$(RAGEL) $(MODE) --host-lang=rust -o clang.rs clang.rl
	rustc clang.rs

gotocallret: gotocallret.rl
	$(RAGEL) $(MODE) --host-lang=rust -o gotocallret.rs gotocallret.rl
	rustc gotocallret.rs

scan1: scan1.rl
	$(RAGEL) $(MODE) --host-lang=rust -o scan1.rs scan1.rl
	rustc scan1.rs

scan2: scan2.rl
	$(RAGEL) $(MODE) --host-lang=rust -o scan2.rs scan2.rl
	rustc scan2.rs

.PHONY: clean
clean:
	rm -f *.o
	rm -f $(subst .rl,.rs,$(wildcard *.rl))
	rm -f $(subst .rl,,$(wildcard *.rl))
