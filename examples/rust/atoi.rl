
%%{
machine atoi;
write data;
}%%


fn atoi(data: ~str) -> option<int> {
  let mut cs: int;
  let mut p = 0;
  let mut pe = data.len();
  let mut neg = false;
  let mut res = 0;

    %%{
        action see_neg   { neg = true; }
        action add_digit { res = res * 10 + (fc as int - '0' as int); }

        main :=
            ( '-' @see_neg | '+' )? ( digit @add_digit )+
            '\n'?
        ;

        write init;
        write exec;
    }%%

    if neg { res = -1 * res; }

    if cs < atoi_first_final {
        none
    } else {
        some(res)
    }
}

fn main() {
    assert atoi(~"7") == some(7);
    assert atoi(~"666") == some(666);
    assert atoi(~"-666") == some(-666);
    assert atoi(~"+666") == some(666);
    assert atoi(~"123456789") == some(123456789);
    assert atoi(~"+123456789\n") == some(123456789);
    assert atoi(~"+ 1234567890") == none;
}
