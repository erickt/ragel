// -*-rust-*-
//
// Reverse Polish Notation Calculator
// Copyright (c) 2010 J.A. Roberts Tunney
// MIT License
//
// To compile:
//
//   ragel --host-lang=rust -o rpn.rs rpn.rl
//   rust -o rpn rpn.rs
//   ./rpn
//
// To show a diagram of your state machine:
//
//   ragel -V -p -o rpn.dot rpn.rl
//   dot -Tpng -o rpn.png rpn.dot
//   chrome rpn.png
//

use std;
import result::{result, ok, err};

%% machine rpn;
%% write data;

fn rpn(data: ~str) -> result<int, ~str> {
    let mut cs = 0;
    let mut p = 0;
    let mut pe = data.len();
    let mut mark = 0;
    let mut st: ~[int] = ~[];

    %%{
        action mark { mark = p; }
        action push {
            let s = data.slice(mark, p);
            alt int::from_str(s) {
              none { ret err(#fmt("invalid integer %s", s)); }
              some(i) { vec::push(st, i); }
            }
        }
        action add  { let y = vec::pop(st); let x = vec::pop(st); vec::push(st, x + y); }
        action sub  { let y = vec::pop(st); let x = vec::pop(st); vec::push(st, x - y); }
        action mul  { let y = vec::pop(st); let x = vec::pop(st); vec::push(st, x * y); }
        action div  { let y = vec::pop(st); let x = vec::pop(st); vec::push(st, x / y); }
        action abs  { vec::push(st, int::abs(vec::pop(st))); }
        action abba { vec::push(st, 666); }

        stuff  = digit+ >mark %push
               | '+' @add
               | '-' @sub
               | '*' @mul
               | '/' @div
               | 'abs' %abs
               | 'add' %add
               | 'abba' %abba
               ;

        main := ( space | stuff space )* ;

        write init;
        write exec;
    }%%

    if cs < rpn_first_final {
        if p == pe {
            ret err(~"unexpected eof");
        } else {
            //ret err(#fmt("error at position %i", p));
            ret err(~"error at position %i");
        }
    }

    if st.is_empty() {
        ret err(~"rpn stack empty on result");
    } else {
        ret ok(vec::pop(st))
    }
}

//////////////////////////////////////////////////////////////////////

#[cfg(test)]
mod tests {
    #[test]
    fn test_success() {
        let rpnTests = [
            (~"666\n", 666),
            (~"666 111\n", 111),
            (~"4 3 add\n", 7),
            (~"4 3 +\n", 7),
            (~"4 3 -\n", 1),
            (~"4 3 *\n", 12),
            (~"6 2 /\n", 3),
            (~"0 3 -\n", -3),
            (~"0 3 - abs\n", 3),
            (~" 2  2 + 3 - \n", 1),
            (~"10 7 3 2 * - +\n", 11),
            (~"abba abba add\n", 1332),
        ];

        for rpnTests.each |sx| {
            let (s, x) = sx;
            assert rpn(s).get() == x;
        }
    }

    #[test]
    fn test_failure() {
        let rpnFailTests = [
            (~"\n", ~"rpn stack empty on result")
        ];

        for rpnFailTests.each |sx| {
            let (s, x) = sx;
            assert rpn(s).get_err() == x;
        }
    }
}
