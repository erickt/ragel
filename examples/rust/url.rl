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
//    rustc url.rc
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

use std;

import result::{result, ok, err};
import url_authority::{url, parse_authority};

fn dummy() -> url {
    {
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
fn url_parse(data: ~[u8]) -> result<url, @~str> {
    let mut cs: int;
    let mut p = 0;
    let mut pe = data.len();
    let mut eof = data.len();
    let mut mark = 0;
    let mut url = dummy();
   
    // this buffer is so we can unescape while we roll
    let mut buf = vec::to_mut(vec::from_elem(data.len(), 0));

    let mut hex = 0;
    let mut amt = 0;

    %%{
        action mark      { mark = p;                              }
        action str_start { amt = 0;                               }
        action str_char  { buf[amt] = fc; amt += 1;               }
        action str_lower { buf[amt] = fc + 0x20; amt += 1;        }

        action hex_hi {
            hex = alt char::to_digit(fc as char, 16) {
              none { ret err(@~"invalid hex"); }
              some(hex) { hex * 16 }
            }
        }

        action hex_lo {
            hex += alt char::to_digit(fc as char, 16) {
              none { ret err(@~"invalid hex"); }
              some(hex) { hex }
            };
            buf[amt] = hex as u8;
            amt += 1;
        }

        action scheme {
            url.scheme = str::from_bytes(buf.slice(0, amt));
        }

        action authority {
            let v = vec::view(data, mark, p);
            let authority = parse_authority(url, v);
            if authority.is_err() {
                ret err(authority.get_err());
            }
            url = result::unwrap(authority);
        }

        action path     {
            url.path = str::from_bytes(buf.slice(0, amt));
        }

        action query {
            url.query = str::from_bytes(data.slice(mark, p));
        }

        action fragment {
            url.fragment = str::from_bytes(buf.slice(0, amt));
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
            err(@~"unexpected eof")
        } else {
            err(@#fmt("error in url at pos %u", p))
        }
    } else {
        ok(url)
    }
}

//////////////////////////////////////////////////////////////////////

#[cfg(test)]
mod tests {
    import std::time;

    #[test]
    fn test() {
        let data = [(
            ~"http://user:pass@example.com:80;hello/lol.php?fun#omg",
            {
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
            {
                scheme: ~"a",
                host: ~"b",
                with dummy()
            }
        ), (
            ~"GoPHeR://@example.com@:;/?#",
            {
                scheme: ~"gopher",
                host: ~"@example.com@",
                path: ~"/",
                with dummy()
            }
        ), (
            ~"ldap://[2001:db8::7]/c=GB?objectClass/?one",
            {
                scheme: ~"ldap",
                host: ~"2001:db8::7",
                path: ~"/c=GB",
                query: ~"objectClass/?one",
                with dummy()
            }
        ), (

            ~"http://user@example.com",
            {
                scheme: ~"http",
                user: ~"user",
                host: ~"example.com",
                with dummy()
            }
        ), (

            ~"http://品研发和研发管@☃.com:65000;%20",
            {
                scheme: ~"http",
                user: ~"品研发和研发管",
                host: ~"☃.com",
                port: 65000,
                params: ~"%20",
                with dummy()
            }
        ), (

            ~"https://example.com:80",
            {
                scheme: ~"https",
                host: ~"example.com",
                port: 80,
                with dummy()
            }
        ), (

            ~"file:///etc/passwd",
            {
                scheme: ~"file",
                path: ~"/etc/passwd",
                with dummy()
            }
        ), (

            ~"file:///c:/WINDOWS/clock.avi",
            {
                scheme: ~"file",
                path: ~"/c:/WINDOWS/clock.avi", /* <-- is this kosher? */
                with dummy()
            }
        ), (

            ~"file://hostname/path/to/the%20file.txt",
            {
                scheme: ~"file",
                host: ~"hostname",
                path: ~"/path/to/the file.txt",
                with dummy()
            }
        ), (

            ~"sip:example.com",
            {
                scheme: ~"sip",
                host: ~"example.com",
                with dummy()
            }
        ), (

            ~"sip:example.com:5060",
            {
                scheme: ~"sip",
                host: ~"example.com",
                port: 5060,
                with dummy()
            }
        ), (

            ~"mailto:ditto@pokémon.com",
            {
                scheme: ~"mailto",
                user: ~"ditto",
                host: ~"pokémon.com",
                with dummy()
            }
        ), (

            ~"sip:[dead:beef::666]:5060",
            {
                scheme: ~"sip",
                host: ~"dead:beef::666",
                port: 5060,
                with dummy()
            }
        ), (

            ~"tel:+12126660420",
            {
                scheme: ~"tel",
                host: ~"+12126660420",
                with dummy()
            }
        ), (

            ~"sip:bob%20barker:priceisright@[dead:beef::666]:5060;isup-oli=00/palfun.html?haha#omg",
            {
                scheme: ~"sip",
                user: ~"bob barker",
                pass: ~"priceisright",
                host: ~"dead:beef::666",
                port: 5060,
                params: ~"isup-oli=00",
                path: ~"/palfun.html",
                query: ~"haha",
                fragment: ~"omg",
                with dummy()
            }
        ), (

            ~"http://www.google.com/search?%68l=en&safe=off&q=omfg&aq=f&aqi=g2g-s1g1g-s1g5&aql=&oq=&gs_rfai=",
            {
                scheme: ~"http",
                host: ~"www.google.com",
                path: ~"/search",
                query: ~"%68l=en&safe=off&q=omfg&aq=f&aqi=g2g-s1g1g-s1g5&aql=&oq=&gs_rfai=",
                with dummy()
            }
        )];

        for data.each |data| {
            alt data {
              (s, expected) {
                alt url_parse(str::bytes(s)) {
                  err(e) { fail *e; }
                  ok(url) { assert expected == url; }
                }
              }
            }
        }
    }

    #[test]
    fn benchmark() {
        let rounds = 100000;
        let urls = [
            ~"a:a",
            ~"http://google.com/",
            ~"sip:jtunney@lobstertech.com",
            ~"http://user:pass@example.com:80;hello/lol.php?fun#omg",
            ~"file:///etc/passwd",
        ];

        for urls.each |url| {
            let t1 = time::precise_time_ns();
            for rounds.times {
                url_parse(str::bytes(url));
            }
            let t2 = time::precise_time_ns();

            io::println(#fmt("BENCH parse %s -> %f ns",
                             url,
                             ((t2 - t1) as float) / (rounds as float)));
        }
    }
}
