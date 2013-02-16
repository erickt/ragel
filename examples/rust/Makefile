
.SUFFIXES:

RAGEL = ../../ragel/ragel
MODE =
RUSTC = rustc -O

all: url concurrent rpn atoi clang gotocallret scan1 scan2

url: url.rl
	$(RAGEL) $(MODE) --host-lang=rust url_authority.rl -o url_authority.rs
	$(RAGEL) $(MODE) --host-lang=rust url.rl -o url.rc
	$(RUSTC) --test url.rc

concurrent: concurrent.rl
	$(RAGEL) $(MODE) --host-lang=rust concurrent.rl -o concurrent.rc
	$(RUSTC) concurrent.rc

rpn: rpn.rl
	$(RAGEL) $(MODE) --host-lang=rust rpn.rl -o rpn.rc
	$(RUSTC) --test rpn.rc

atoi: atoi.rl
	$(RAGEL) $(MODE) --host-lang=rust atoi.rl -o atoi.rc
	$(RUSTC) --test atoi.rc

clang: clang.rl
	$(RAGEL) $(MODE) --host-lang=rust clang.rl -o clang.rc
	$(RUSTC) clang.rc

gotocallret: gotocallret.rl
	$(RAGEL) $(MODE) --host-lang=rust gotocallret.rl -o gotocallret.rc
	$(RUSTC) gotocallret.rc

scan1: scan1.rl
	$(RAGEL) $(MODE) --host-lang=rust scan1.rl -o scan1.rc
	$(RUSTC) scan1.rc

scan2: scan2.rl
	$(RAGEL) $(MODE) --host-lang=rust scan2.rl -o scan2.rc
	$(RUSTC) scan2.rc

.PHONY: clean
clean:
	rm -f *.o
	rm -f $(subst .rl,.rc,$(wildcard *.rl))
	rm -f $(subst .rl,,$(wildcard *.rl))
