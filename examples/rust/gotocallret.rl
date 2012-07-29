/*
 * Demonstrate the use of goto, call and return. This machine expects either a
 * lower case char or a digit as a command then a space followed by the command
 * arg. If the command is a char, then the arg must be an a string of chars.
 * If the command is a digit, then the arg must be a string of digits. This
 * choice is determined by action code, rather than though transition
 * desitinations.
 */

%%{
	machine GotoCallRet;

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
		if comm >= 'a' as u8 {
			fcall alp_comm;
        } else {
			fcall dig_comm;
        }
	}

	# Specifies command string. Note that the arg is left out.
	command = (
		[a-z0-9] @{comm = fc;} ' ' @comm_arg '\n'
	) @{ io::println("correct command"); };

	# Any number of commands. If there is an 
	# error anywhere, garble the line.
	main := command* $!{fhold;fgoto garble_line;};
}%%

%% write data;

class goto_call_ret {
    let mut comm: u8;
    let mut cs: int;
    let mut top: int;
    let mut stack: @~[mut int];

    new() {
        let cs: int;
        let top: int;
        %% write init;
        self.cs = cs;
        self.comm = 0;
        self.top = top;
        self.stack = @vec::to_mut(vec::from_elem(32, 0));
    }

    fn execute(data: &[const u8], is_eof: bool) -> int {
        let mut p = 0;
        let mut pe = data.len();
        let mut eof = if is_eof { data.len() } else { 0 };

        let mut cs = self.cs;
        let mut comm = self.comm;
        let mut top = self.top;
        let mut stack = self.stack;

        %% write exec;

        self.cs = cs;
        self.comm = comm;
        self.top = top;
        self.stack = stack;

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
    let mut buf = vec::to_mut(vec::from_elem(1024, 0));

    let gcr = goto_call_ret();

    loop {
        let count = io::stdin().read(buf, buf.len());
        if count == 0 { break; }

        gcr.execute(vec::mut_view(buf, 0, count), false);
    }

    gcr.execute(~[], true);

    if gcr.cs < GotoCallRet_first_final {
        fail ~"gotocallret: error: parsing input";
    }
}
