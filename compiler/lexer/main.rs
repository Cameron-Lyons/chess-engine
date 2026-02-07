use lexer::token::TokenKind;
use lexer::Lexer;
use std::io::stdin;

pub fn main() {
    loop {
        let mut input = String::new();
        stdin().read_line(&mut input).unwrap();

        if input.trim_end().is_empty() {
            std::process::exit(0)
        }

        let mut l = Lexer::new(&input);
        loop {
            let t = l.next_token();
            if t.kind == TokenKind::EOF {
                break;
            } else {
                println!("{}", t)
            }
        }
    }
}
