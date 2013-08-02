%%{
machine atoi;
write data;
}%%


fn atoi(data: &str) -> Option<int> {
  let mut cs: int;
  let mut p = 0;
  let pe = data.len();
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
        None
    } else {
        Some(res)
    }
}

#[test]
fn test_atoi() {
    assert_eq!(atoi("7"), Some(7));
    assert_eq!(atoi("666"), Some(666));
    assert_eq!(atoi("-666"), Some(-666));
    assert_eq!(atoi("+666"), Some(666));
    assert_eq!(atoi("123456789"), Some(123456789));
    assert_eq!(atoi("+123456789\n"), Some(123456789));
    assert_eq!(atoi("+ 1234567890"), None);
}
