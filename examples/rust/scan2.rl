%%{
    machine scanner;

    # Warning: changing the patterns or the input string will affect the
    # coverage of the scanner action types.
    main := |*
        'a' => {
            assert_eq!(expect[i], Pat1);
            i += 1;
        };

        [ab]+ . 'c' => {
            assert_eq!(expect[i], Pat2);
            i += 1;
        };

        any => {
            assert_eq!(expect[i], Any);
            i += 1;
        };
    *|;

  write data;
}%%

#[deriving(Eq)]
enum Expect { Pat1, Pat2, Any }

#[allow(dead_assignment, unused_variable)]
fn main() {
    let mut i = 0;
    let expect = ~[Pat1, Any, Pat2, Any, Any, Any];

    let mut ts: uint;
    let mut te: uint;
    let mut cs: int;
    let mut act: uint;
    let data = ~"araabccde";
    let mut p = 0;
    let pe = data.len();
    let eof = pe;
    %% write init;
    %% write exec;
}
