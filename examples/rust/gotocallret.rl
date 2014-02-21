/*
 * Demonstrate the use of goto, call and return. This machine expects either a
 * lower case char or a digit as a command then a space followed by the command
 * arg. If the command is a char, then the arg must be an a string of chars.
 * If the command is a digit, then the arg must be a string of digits. This
 * choice is determined by action code, rather than though transition
 * desitinations.
 */

use std::io;
use std::vec;

%%{
	machine GotoCallRet;

	access self_.;
	getkey data[p];

	# Error machine, consumes to end of 
	# line, then starts the main line over.
	garble_line := (
		(any-'\n')*'\n'
	) >{ io::println("error: garbling line"); } @{fgoto main;};

	# Look for a string of alphas or of digits, 
	# on anything else, hold the character and return.
	alp_comm := alpha+ $!{fhold;fret;};
	dig_comm := digit+ $!{fhold;fret;};

	# Choose which to machine to call into based on the command.
	action comm_arg {
		if self_.comm >= 'a' as u8 {
			fcall alp_comm;
		} else {
			fcall dig_comm;
		}
	}

	# Specifies command string. Note that the arg is left out.
	command = (
		[a-z0-9] @{ self_.comm = fc; } ' ' @comm_arg '\n'
	) @{ io::println("correct command"); };

	# Any number of commands. If there is an 
	# error anywhere, garble the line.
	main := command* $!{fhold;fgoto garble_line;};
}%%

%% write data;

struct GotoCallRet {
    comm: u8,
    cs: int,
    top: int,
    stack: ~[int],
}

impl GotoCallRet {
    fn new() -> GotoCallRet {
        let mut self_ = GotoCallRet {
            cs: 0,
            comm: 0,
            top: 0,
            stack: vec::from_elem(32, 0),
        };
        %% write init;
        self_
    }

    fn execute(&mut self, data: &mut [u8], is_eof: bool) -> int {
        let self_ = &mut *self;
        let mut p = 0;
        let pe = data.len();
        let eof = if is_eof { data.len() } else { 0 };

        %% write exec;

        if self.cs == GotoCallRet_error {
            -1
        } else if self.cs >= GotoCallRet_first_final {
            1
        } else {
            0
        }
    }
}

fn main() {
    let mut buf = [0u8, .. 1024];
    let mut gcr = GotoCallRet::new();

    loop {
        let count = io::stdin().read(buf).unwrap();
        if count == 0 { break; }

        gcr.execute(buf.mut_slice(0, count), false);
    }

    gcr.execute([], true);

    if gcr.cs < GotoCallRet_first_final {
        fail!(~"gotocallret: error: parsing input");
    }
}
