/*
 * A mini C-like language scanner.
 */

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
        io::println(fmt!("symbol(%i): %c", curlin, data[ts] as char));
    };

    # Identifier. Upon entering clear the buffer. On all transitions
    # buffer a character. Upon leaving, dump the identifier.
    alpha_u alnum_u* {
        io::println(fmt!("ident(%i): %s", curlin, str::from_bytes(data.slice(ts, te))));
    };

    # Single Quote.
    sliteralChar = [^'\\] | newline | ( '\\' . any_count_line );
    '\'' . sliteralChar* . '\'' {
        io::println(fmt!("single_lit(%i): %s", curlin, str::from_bytes(data.slice(ts, te))));
    };

    # Double Quote.
    dliteralChar = [^"\\] | newline | ( '\\' any_count_line );
    '"' . dliteralChar* . '"' {
        io::println(fmt!("double_lit(%i): %s", curlin, str::from_bytes(data.slice(ts, te))));
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
        io::println(fmt!("int(%i): %s", curlin, str::from_bytes(data.slice(ts, te))));
    };

    # Match a float. Upon entering the machine clear the buf, buffer
    # characters on every trans and dump the float upon leaving.
    digit+ '.' digit+ {
        io::println(fmt!("float(%i): %s", curlin, str::from_bytes(data.slice(ts, te))));
    };

    # Match a hex. Upon entering the hex part, clear the buf, buffer characters
    # on every trans and dump the hex on leaving transitions.
    '0x' xdigit+ {
        io::println(fmt!("hex(%i): %s", curlin, str::from_bytes(data.slice(ts, te))));
    };

    *|;
}%%

%% write data nofinal;

const BUFSIZE: uint = 2048;

fn main() {
    let mut data = vec::from_elem(BUFSIZE, 0);

    let mut cs = 0;
    let mut act = 0;
    let mut have = 0;
    let mut curlin = 1;
    let mut ts = 0;
    let mut te = 0;
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

        let pe = io::stdin().read(vec::mut_slice(data, have, data.len()), space);

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
            have = pe - ts;
            vec::bytes::copy_memory(data, vec::const_slice(data, ts, pe), have);
            te = te - ts;
            ts = 0;
        }
    }
}
