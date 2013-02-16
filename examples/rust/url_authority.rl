/*
// -*-go-*-
//
// URL Parser
// Copyright (c) 2010 J.A. Roberts Tunney
// MIT License
//
*/

%% machine url_authority;
%% write data;

/*
// i parse strings like `alice@pokémon.com`.
//
// sounds simple right?  but i also parse stuff like:
//
//   bob%20barker:priceisright@[dead:beef::666]:5060;isup-oli=00
//
// which in actual reality is:
//
// - User: "bob barker"
// - Pass: "priceisright"
// - Host: "dead:beef::666"
// - Port: 5060
// - Params: "isup-oli=00"
//
// which was probably extracted from an absolute url that looked like:
//
//   sip:bob%20barker:priceisright@[dead:beef::666]:5060;isup-oli=00/palfun.html?haha#omg
//
// which was probably extracted from its address form:
//
//   "Bob Barker" <sip:bob%20barker:priceisright@[dead:beef::666]:5060;isup-oli=00/palfun.html?haha#omg>;tag=666
//
// who would have thought this could be so hard ._.
*/

#[deriving_eq]
pub struct Url {
    scheme   : ~str, /* http, sip, file, etc. (never blank, always lowercase) */
    user     : ~str, /* who is you */
    pass     : ~str, /* for like, logging in */
    host     : ~str, /* IP 4/6 address or hostname (mandatory) */
    port     : u16,  /* like 80 or 5060 (default 0) */
    params   : ~str, /* stuff after ';' (NOT UNESCAPED, used in sip) */
    path     : ~str, /* stuff starting with '/' */
    query    : ~str, /* stuff after '?' (NOT UNESCAPED) */
    fragment : ~str, /* stuff after '#' */
}

pub fn parse_authority(url: &mut Url, data: &[u8]) -> Result<(), ~str> {
    let mut cs: int;
    let mut p = 0;
    let mut pe = data.len();
    let mut eof = data.len();
    let mut mark = 0;

    // temporary holding place for user:pass and/or host:port cuz an
    // optional term (user[:pass]) coming before a mandatory term
    // (host[:pass]) would require require backtracking and all that
    // evil nondeterministic stuff which ragel seems to hate.  (for
    // this same reason you're also allowed to use square quotes
    // around the username.)
    let mut b1 = ~"";
    let mut b2 = ~"";

    // this buffer is so we can unescape while we roll
    let mut buf = vec::from_elem(data.len(), 0);

    let mut hex = 0;
    let mut amt = 0;

    fn parse_port(s: &str) -> Option<u16> {
        if s != ~"" {
            do uint::from_str(s).chain_ref |port| {
                if *port > 65535 { None } else { Some(*port as u16) }
            }
        } else {
            Some(0)
        }
    }

    %%{
        action mark      { mark = p;                }
        action str_start { amt = 0;                 }
        action str_char  {
            buf[amt] = fc; amt += 1; }

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
            buf[amt] = hex as u8;
            amt += 1;
        }

        action copy_b1 {
            b1 = str::from_bytes(buf.slice(0, amt));
            amt = 0;
        }

        action copy_b2 {
            b2 = str::from_bytes(buf.slice(0, amt));
            amt = 0;
        }

        action copy_host {
            url.host = copy b1;
            amt = 0;
        }

        action copy_port {
            match parse_port(b2) {
                None => {
                    return Err(fmt!("bad url authority: %s",
                        str::from_bytes(data.slice(0, data.len()))))
                }
                Some(port) => url.port = port,
            }
        }

        action params {
            let params = str::from_bytes(data.slice(mark, p));
            url.params = params;
        }

        action params_eof {
            let params = str::from_bytes(data.slice(mark, p));
            url.params = params;
            return Ok(());
        }

        action atsymbol {
            url.user = copy b1;
            url.pass = copy b2;
            b2 = ~"";
        }

        action alldone {
            url.host = copy b1;

            if url.host == ~"" {
                url.host = str::from_bytes(buf.slice(0, amt));
            } else {
                if amt > 0 {
                    b2 = str::from_bytes(buf.slice(0, amt));
                }
                match parse_port(b2) {
                    None => {
                        return Err(fmt!("bad url authority: %s",
                            str::from_bytes(data.slice(0, data.len()))))
                    }
                    Some(port) => url.port = port,
                }
            }

            return Ok(());
        }

        # define what a single character is allowed to be
        toxic         = ( cntrl | 127 ) ;
        scary         = ( toxic | space | "\"" | "#" | "%" | "<" | ">" ) ;
        authdelims    = ( "/" | "?" | "#" | ":" | "@" | ";" | "[" | "]" ) ;
        userchars     = any -- ( authdelims | scary ) ;
        userchars_esc = userchars | ":" ;
        passchars     = userchars ;
        hostchars     = passchars | "@" ;
        hostchars_esc = hostchars | ":" ;
        portchars     = digit ;
        paramchars    = hostchars | ":" | ";" ;

        # define how characters trigger actions
        escape        = "%" xdigit xdigit ;
        unescape      = "%" ( xdigit @hex_hi ) ( xdigit @hex_lo ) ;
        userchar      = unescape | ( userchars @str_char ) ;
        userchar_esc  = unescape | ( userchars_esc @str_char ) ;
        passchar      = unescape | ( passchars @str_char ) ;
        hostchar      = unescape | ( hostchars @str_char ) ;
        hostchar_esc  = unescape | ( hostchars_esc @str_char ) ;
        portchar      = unescape | ( portchars @str_char ) ;
        paramchar     = escape | paramchars ;

        # define multi-character patterns
        user_plain    = userchar+ >str_start %copy_b1 ;
        user_quoted   = "[" ( userchar_esc+ >str_start %copy_b1 ) "]" ;
        user          = ( user_quoted | user_plain ) %/alldone ;
        pass          = passchar+ >str_start %copy_b2 %/alldone ;
        host_plain    = hostchar+ >str_start %copy_b1 %copy_host ;
        host_quoted   = "[" ( hostchar_esc+ >str_start %copy_b1 %copy_host ) "]" ;
        host          = ( host_quoted | host_plain ) %/alldone ;
        port          = portchar* >str_start %copy_b2 %copy_port %/alldone ;
        params        = ";" ( paramchar* >mark %params %/params_eof ) ;
        userpass      = user ( ":" pass )? ;
        hostport      = host ( ":" port )? ;
        authority     = ( userpass ( "@" @atsymbol ) )? hostport params? ;

        main := authority;
        write init;
        write exec;
    }%%

    Ok(())
}
