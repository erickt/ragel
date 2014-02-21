//
// URL Parser
// Copyright (c) 2010 J.A. Roberts Tunney
// MIT License
//
// Converted to Rust by Erick Tryzelaar
//
// To compile:
//
//    ragel --host-lang=rust url.rl -o url.rs
//    ragel --host-lang=rust url_authority.rl -o url_authority.rs
//    rustc url.rs
//    ./url
//
// To show a diagram of your state machine:
//
//   ragel -V -p -o url.dot url.rl
//   dot -Tpng -o url.png url.dot
//   chrome url.png
//
//   ragel -V -p -o url_authority.dot url_authority.rl
//   dot -Tpng -o url_authority.png url_authority.dot
//   chrome url_authority.png
//
// Reference:
//
// - http://tools.ietf.org/html/rfc3986
//

extern crate extra;

use std::char;
use std::str;
use std::vec;

pub use url_authority::{Url, parse_authority};

mod url_authority;

pub fn dummy() -> Url {
    Url {
        scheme: ~"", user: ~"", pass: ~"", host: ~"", port: 0, 
        params: ~"", path: ~"", query: ~"", fragment: ~"",
    }
}

%% machine url;
%% write data;

// i parse absolute urls and don't suck at it.  i'll parse just about
// any type of url you can think of and give you a human-friendly data
// structure.
//
// this routine takes no more than a few microseconds, is reentrant,
// performs in a predictable manner (for security/soft-realtime,)
// doesn't modify your `data` buffer, and under no circumstances will
// it panic (i hope!)
pub fn url_parse(data: &[u8]) -> Result<Url, ~str> {
    let mut cs: int;
    let mut p = 0;
    let pe = data.len();
    let eof = data.len();
    let mut mark = 0;
    let mut url = dummy();
   
    // this buffer is so we can unescape while we roll
    let mut buf = vec::with_capacity(500);
    let mut hex = 0;

    %%{
        action mark      { mark = p;                  }
        action str_start { buf.clear();               }
        action str_char  { buf.push(fc);              }
        action str_lower { buf.push(fc + 0x20)        }

        action hex_hi {
            hex = match char::to_digit(fc as char, 16) {
              None => return Err(~"invalid hex"),
              Some(hex) => hex * 16,
            }
        }

        action hex_lo {
            hex += match char::to_digit(fc as char, 16) {
              None => return Err(~"invalid hex"),
              Some(hex) => hex,
            };
            buf.push(hex as u8);
        }

        action scheme {
            url.scheme = str::from_utf8(buf).unwrap().to_owned();
        }

        action authority {
            let v = data.slice(mark, p);
            match parse_authority(&mut url, v) {
                Ok(()) => { },
                Err(e) => return Err(e),
            }
        }

        action path     {
            url.path = str::from_utf8(buf).unwrap().to_owned();
        }

        action query {
            url.query = str::from_utf8(data.slice(mark, p)).unwrap().to_owned();
        }

        action fragment {
            url.fragment = str::from_utf8(buf).unwrap().to_owned();
        }

        # define what a single character is allowed to be
        toxic     = ( cntrl | 127 ) ;
        scary     = ( toxic | " " | "\"" | "#" | "%" | "<" | ">" ) ;
        schmchars = ( lower | digit | "+" | "-" | "." ) ;
        authchars = any -- ( scary | "/" | "?" | "#" ) ;
        pathchars = any -- ( scary | "?" | "#" ) ;
        querchars = any -- ( scary | "#" ) ;
        fragchars = any -- ( scary ) ;

        # define how characters trigger actions
        escape    = "%" xdigit xdigit ;
        unescape  = "%" ( xdigit @hex_hi ) ( xdigit @hex_lo ) ;
        schmfirst = ( upper @str_lower ) | ( lower @str_char ) ;
        schmchar  = ( upper @str_lower ) | ( schmchars @str_char ) ;
        authchar  = escape | authchars ;
        pathchar  = unescape | ( pathchars @str_char ) ;
        querchar  = escape | querchars ;
        fragchar  = unescape | ( fragchars @str_char ) ;

        # define multi-character patterns
        scheme    = ( schmfirst schmchar* ) >str_start %scheme ;
        authority = authchar+ >mark %authority ;
        path      = ( ( "/" @str_char ) pathchar* ) >str_start %path ;
        query     = "?" ( querchar* >mark %query ) ;
        fragment  = "#" ( fragchar* >str_start %fragment ) ;
        url       = scheme ":" "//"? authority path? query? fragment?
                  | scheme ":" "//" authority? path? query? fragment?
                  ;

        main := url;
        write init;
        write exec;
    }%%

    if cs < url_first_final {
        if p == pe {
            Err(~"unexpected eof")
        } else {
            Err(format!("error in url at pos {}", p))
        }
    } else {
        Ok(url)
    }
}

//////////////////////////////////////////////////////////////////////

#[cfg(test)]
mod tests {
    use super::{Url, url_parse, dummy};

    #[test]
    fn test() {
        let data = [(
            ~"http://user:pass@example.com:80;hello/lol.php?fun#omg",
            Url {
                scheme: ~"http",
                user: ~"user",
                pass: ~"pass",
                host: ~"example.com",
                port: 80,
                params: ~"hello",
                path: ~"/lol.php",
                query: ~"fun",
                fragment: ~"omg",
            }
        ), (
            ~"a:b",
            Url {
                scheme: ~"a",
                host: ~"b",
                .. dummy()
            }
        ), (
            ~"GoPHeR://@example.com@:;/?#",
            Url {
                scheme: ~"gopher",
                host: ~"@example.com@",
                path: ~"/",
                .. dummy()
            }
        ), (
            ~"ldap://[2001:db8::7]/c=GB?objectClass/?one",
            Url {
                scheme: ~"ldap",
                host: ~"2001:db8::7",
                path: ~"/c=GB",
                query: ~"objectClass/?one",
                .. dummy()
            }
        ), (

            ~"http://user@example.com",
            Url {
                scheme: ~"http",
                user: ~"user",
                host: ~"example.com",
                .. dummy()
            }
        ), (

            ~"http://品研发和研发管@☃.com:65000;%20",
            Url {
                scheme: ~"http",
                user: ~"品研发和研发管",
                host: ~"☃.com",
                port: 65000,
                params: ~"%20",
                .. dummy()
            }
        ), (

            ~"https://example.com:80",
            Url {
                scheme: ~"https",
                host: ~"example.com",
                port: 80,
                .. dummy()
            }
        ), (

            ~"file:///etc/passwd",
            Url {
                scheme: ~"file",
                path: ~"/etc/passwd",
                .. dummy()
            }
        ), (

            ~"file:///c:/WINDOWS/clock.avi",
            Url {
                scheme: ~"file",
                path: ~"/c:/WINDOWS/clock.avi", // <-- is this kosher?
                .. dummy()
            }
        ), (

            ~"file://hostname/path/to/the%20file.txt",
            Url {
                scheme: ~"file",
                host: ~"hostname",
                path: ~"/path/to/the file.txt",
                .. dummy()
            }
        ), (

            ~"sip:example.com",
            Url {
                scheme: ~"sip",
                host: ~"example.com",
                .. dummy()
            }
        ), (

            ~"sip:example.com:5060",
            Url {
                scheme: ~"sip",
                host: ~"example.com",
                port: 5060,
                .. dummy()
            }
        ), (

            ~"mailto:ditto@pokémon.com",
            Url {
                scheme: ~"mailto",
                user: ~"ditto",
                host: ~"pokémon.com",
                .. dummy()
            }
        ), (

            ~"sip:[dead:beef::666]:5060",
            Url {
                scheme: ~"sip",
                host: ~"dead:beef::666",
                port: 5060,
                .. dummy()
            }
        ), (

            ~"tel:+12126660420",
            Url {
                scheme: ~"tel",
                host: ~"+12126660420",
                .. dummy()
            }
        ), (

            ~"sip:bob%20barker:priceisright@[dead:beef::666]:5060;isup-oli=00/palfun.html?haha#omg",
            Url {
                scheme: ~"sip",
                user: ~"bob barker",
                pass: ~"priceisright",
                host: ~"dead:beef::666",
                port: 5060,
                params: ~"isup-oli=00",
                path: ~"/palfun.html",
                query: ~"haha",
                fragment: ~"omg",
                .. dummy()
            }
        ), (

            ~"http://www.google.com/search?%68l=en&safe=off&q=omfg&aq=f&aqi=g2g-s1g1g-s1g5&aql=&oq=&gs_rfai=",
            Url {
                scheme: ~"http",
                host: ~"www.google.com",
                path: ~"/search",
                query: ~"%68l=en&safe=off&q=omfg&aq=f&aqi=g2g-s1g1g-s1g5&aql=&oq=&gs_rfai=",
                .. dummy()
            }
        )];

        for data in data.iter() {
            match *data {
                (ref s, ref expected) => {
                    match url_parse(s.as_bytes()) {
                        Err(e) => fail!(e),
                        Ok(url) => assert_eq!(expected, &url),
                    }
                }
            }
        }
    }
}

#[cfg(test)]
mod bench {
    extern crate test;

    use super::url_parse;
    use self::test::BenchHarness;

    #[bench]
    fn benchmark_ragel_1(bh: &mut BenchHarness) {
        bh.iter(|| {
            let _x = url_parse("a:a".as_bytes());
        })
    }

    #[bench]
    fn benchmark_ragel_2(bh: &mut BenchHarness) {
        bh.iter(|| {
            let _x = url_parse("http://google.com/".as_bytes());
        })
    }

    #[bench]
    fn benchmark_ragel_3(bh: &mut BenchHarness) {
        bh.iter(|| {
            let _x = url_parse("sip:jtunney@lobstertech.com".as_bytes());
        })
    }

    #[bench]
    fn benchmark_ragel_4(bh: &mut BenchHarness) {
        bh.iter(|| {
            let _x = url_parse("http://user:pass@example.com".as_bytes());
        })
    }

    #[bench]
    fn benchmark_ragel_5(bh: &mut BenchHarness) {
        bh.iter(|| {
            let _x = url_parse("http://user:pass@example.com:80".as_bytes());
        })
    }

    #[bench]
    fn benchmark_ragel_6(bh: &mut BenchHarness) {
        bh.iter(|| {
            let _x = url_parse("http://user:pass@example.com:80;hello".as_bytes());
        })
    }

    #[bench]
    fn benchmark_ragel_7(bh: &mut BenchHarness) {
        bh.iter(|| {
            let _x = url_parse("http://user:pass@example.com:80;hello/lol.php".as_bytes());
        })
    }

    #[bench]
    fn benchmark_ragel_8(bh: &mut BenchHarness) {
        bh.iter(|| {
            let _x = url_parse("http://user:pass@example.com:80;hello/lol.php?fun".as_bytes());
        })
    }

    #[bench]
    fn benchmark_ragel_9(bh: &mut BenchHarness) {
        bh.iter(|| {
            let _x = url_parse("http://user:pass@example.com:80;hello/lol.php?fun#omg".as_bytes());
        })
    }

    /*
    #[bench]
    fn benchmark_extra_url_1(bh: &mut BenchHarness) {
        bh.iter(|| {
            let _x = extra::url::from_str("a:a");
        })
    }

    #[bench]
    fn benchmark_extra_url_2(bh: &mut BenchHarness) {
        bh.iter(|| {
            let _x = extra::url::from_str("http://google.com/");
        })
    }

    #[bench]
    fn benchmark_extra_url_3(bh: &mut BenchHarness) {
        bh.iter(|| {
            let _x = extra::url::from_str("sip:jtunney@lobstertech.com");
        })
    }

    #[bench]
    fn benchmark_extra_url_4(bh: &mut BenchHarness) {
        bh.iter(|| {
            let _x = extra::url::from_str("http://user:pass@example.com");
        })
    }

    #[bench]
    fn benchmark_extra_url_5(bh: &mut BenchHarness) {
        bh.iter(|| {
            let _x = extra::url::from_str("http://user:pass@example.com:80");
        })
    }

    #[bench]
    fn benchmark_extra_url_6(bh: &mut BenchHarness) {
        bh.iter(|| {
            let _x = extra::url::from_str("http://user:pass@example.com:80;hello");
        })
    }

    #[bench]
    fn benchmark_extra_url_7(bh: &mut BenchHarness) {
        bh.iter(|| {
            let _x = extra::url::from_str("http://user:pass@example.com:80;hello/lol.php");
        })
    }

    #[bench]
    fn benchmark_extra_url_8(bh: &mut BenchHarness) {
        bh.iter(|| {
            let _x = extra::url::from_str("http://user:pass@example.com:80;hello/lol.php?fun");
        })
    }

    #[bench]
    fn benchmark_extra_url_9(bh: &mut BenchHarness) {
        bh.iter(|| {
            let _x = extra::url::from_str("http://user:pass@example.com:80;hello/lol.php?fun#omg");
        })
    }
    */
}
