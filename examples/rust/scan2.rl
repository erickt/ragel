%%{
    machine scanner;

    # Warning: changing the patterns or the input string will affect the
    # coverage of the scanner action types.
    main := |*
        'a' => {
            assert expect[i] == Pat1;
            i += 1;
        };

        [ab]+ . 'c' => {
            assert expect[i] == Pat2;
            i += 1;
        };

        any => {
            assert expect[i] == Any;
            i += 1;
        };
    *|;

  write data;
}%%

#[deriving_eq]
enum expect { Pat1, Pat2, Any }

fn main() {
    let mut i = 0;
    let mut expect = ~[Pat1, Any, Pat2, Any, Any, Any];

    let mut ts = 0;
    let mut te = 0;
    let mut cs = 0;
    let mut act = 0;
    let data = ~"araabccde";
    let mut p = 0;
    let mut pe = data.len();
    let mut eof = pe;
    %% write init;
    %% write exec;
}
