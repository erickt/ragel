/*
 * A mini C-like language scanner.
 */

use std::io;
use std::str;
use std::vec;

%%{
    machine clang;

    newline = '\n' @{ curlin += 1; };
    any_count_line = any | newline;

    # Consume a C comment.
    c_comment := any_count_line* :>> '*/' @{ fgoto main; };

    main := |*

    # Alpha numberic characters or underscore.
    alnum_u = alnum | '_';

    # Alpha charactres or underscore.
    alpha_u = alpha | '_';

    # Symbols. Upon entering clear the buffer. On all transitions
    # buffer a character. Upon leaving dump the symbol.
    ( punct - [_'"] ) {
        println!("symbol({}): {}", curlin, data[ts] as char);
    };

    # Identifier. Upon entering clear the buffer. On all transitions
    # buffer a character. Upon leaving, dump the identifier.
    alpha_u alnum_u* {
        println!("ident({}): {}", curlin, str::from_utf8(data.slice(ts, te)));
    };

    # Single Quote.
    sliteralChar = [^'\\] | newline | ( '\\' . any_count_line );
    '\'' . sliteralChar* . '\'' {
        println!("single_lit({}): {}", curlin, str::from_utf8(data.slice(ts, te)));
    };

    # Double Quote.
    dliteralChar = [^"\\] | newline | ( '\\' any_count_line );
    '"' . dliteralChar* . '"' {
        println!("double_lit({}): {}", curlin, str::from_utf8(data.slice(ts, te)));
    };

    # Whitespace is standard ws, newlines and control codes.
    any_count_line - 0x21..0x7e;

    # Describe both c style comments and c++ style comments. The
    # priority bump on tne terminator of the comments brings us
    # out of the extend* which matches everything.
    '//' [^\n]* newline;

    '/*' { fgoto c_comment; };

    # Match an integer. We don't bother clearing the buf or filling it.
    # The float machine overlaps with int and it will do it.
    digit+ {
        println!("int({}): {}", curlin, str::from_utf8(data.slice(ts, te)));
    };

    # Match a float. Upon entering the machine clear the buf, buffer
    # characters on every trans and dump the float upon leaving.
    digit+ '.' digit+ {
        println!("float({}): {}", curlin, str::from_utf8(data.slice(ts, te)));
    };

    # Match a hex. Upon entering the hex part, clear the buf, buffer characters
    # on every trans and dump the hex on leaving transitions.
    '0x' xdigit+ {
        println!("hex({}): {}", curlin, str::from_utf8(data.slice(ts, te)));
    };

    *|;
}%%

%% write data nofinal;

static BUFSIZE: uint = 2048;

#[allow(dead_assignment, unused_variable)]
fn main() {
    let mut data = vec::from_elem(BUFSIZE, 0u8);

    let mut cs: int;
    let mut act: uint;
    let mut have = 0;
    let mut curlin = 1;
    let mut ts: uint;
    let mut te: uint;
    let mut done = false;

    %% write init;

    while !done {
        let mut p = have;
        let space = BUFSIZE - have;
        let mut eof = -1;

        if space == 0 {
          /* We've used up the entire buffer storing an already-parsed token
           * prefix that must be preserved. */
          fail!(~"OUT OF BUFFER SPACE");
        }

        let pe = io::stdin().read(data.mut_slice_from(have)).unwrap();

        /* Check if this is the end of file. */
        if pe < space {
            eof = pe;
            done = true;
        }

        %% write exec;

        if cs == clang_error {
            fail!(~"PARSE ERROR");
        }

        if ts == -1 {
            have = 0;
        } else {
            /* There is a prefix to preserve, shift it over. */
            let mut i = 0;
            while i < ts - pe {
                data[i] = data[ts + i];
                i += 1;
            }
            te = te - ts;
            ts = 0;
        }
    }
}
