use crate::compiler::Compiler;
use crate::op_code::{concat_instructions, Instructions};
use object::Object;
use parser::parse;
use std::borrow::Borrow;
use std::rc::Rc;

#[allow(dead_code)]
pub fn test_constants(expected: &Vec<Object>, actual: &Vec<Rc<Object>>) {
    assert_eq!(expected.len(), actual.len());
    for (exp, b_got) in expected.iter().zip(actual) {
        let got = b_got.borrow();
        assert_eq!(exp, got);
    }
}

#[allow(dead_code)]
#[derive(Debug, Clone)]
pub struct CompilerTestCase<'a> {
    pub(crate) input: &'a str,
    pub(crate) expected_constants: Vec<Object>,
    pub(crate) expected_instructions: Vec<Instructions>,
}

#[allow(dead_code)]
pub fn run_compiler_test(tests: Vec<CompilerTestCase>) {
    for t in tests {
        let program = parse(t.input).unwrap();
        let mut compiler = Compiler::new();
        let bytecodes = compiler.compile(&program).unwrap();
        test_instructions(&t.expected_instructions, &bytecodes.instructions);
        test_constants(&t.expected_constants, &bytecodes.constants);
    }
}

#[allow(dead_code)]
fn test_instructions(expected: &Vec<Instructions>, actual: &Instructions) {
    let expected_ins = concat_instructions(expected.clone());

    for (&exp, got) in expected_ins.bytes.iter().zip(actual.bytes.clone()) {
        assert_eq!(
            exp,
            got,
            "instruction not equal\n actual  : \n{}\n expected: \n{}",
            actual.string(),
            expected_ins.string()
        );
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::op_code::make_instructions;
    use crate::op_code::Opcode::*;

    #[test]
    fn integer_arithmetic() {
        let tests = vec![
            CompilerTestCase {
                input: "1 + 2",
                expected_constants: vec![Object::Integer(1), Object::Integer(2)],
                expected_instructions: vec![
                    make_instructions(OpConst, &vec![0]),
                    make_instructions(OpConst, &vec![1]),
                    make_instructions(OpAdd, &vec![]),
                    make_instructions(OpPop, &vec![]),
                ],
            },
            CompilerTestCase {
                input: "1; 2",
                expected_constants: vec![Object::Integer(1), Object::Integer(2)],
                expected_instructions: vec![
                    make_instructions(OpConst, &vec![0]),
                    make_instructions(OpPop, &vec![]),
                    make_instructions(OpConst, &vec![1]),
                    make_instructions(OpPop, &vec![]),
                ],
            },
            CompilerTestCase {
                input: "1 - 2",
                expected_constants: vec![Object::Integer(1), Object::Integer(2)],
                expected_instructions: vec![
                    make_instructions(OpConst, &vec![0]),
                    make_instructions(OpConst, &vec![1]),
                    make_instructions(OpSub, &vec![]),
                    make_instructions(OpPop, &vec![]),
                ],
            },
            CompilerTestCase {
                input: "1 * 2",
                expected_constants: vec![Object::Integer(1), Object::Integer(2)],
                expected_instructions: vec![
                    make_instructions(OpConst, &vec![0]),
                    make_instructions(OpConst, &vec![1]),
                    make_instructions(OpMul, &vec![]),
                    make_instructions(OpPop, &vec![]),
                ],
            },
            CompilerTestCase {
                input: "2 / 1",
                expected_constants: vec![Object::Integer(2), Object::Integer(1)],
                expected_instructions: vec![
                    make_instructions(OpConst, &vec![0]),
                    make_instructions(OpConst, &vec![1]),
                    make_instructions(OpDiv, &vec![]),
                    make_instructions(OpPop, &vec![]),
                ],
            },
            CompilerTestCase {
                input: "-1",
                expected_constants: vec![Object::Integer(1)],
                expected_instructions: vec![
                    make_instructions(OpConst, &vec![0]),
                    make_instructions(OpMinus, &vec![]),
                    make_instructions(OpPop, &vec![]),
                ],
            },
            CompilerTestCase {
                input: "!true",
                expected_constants: vec![],
                expected_instructions: vec![
                    make_instructions(OpTrue, &vec![]),
                    make_instructions(OpBang, &vec![]),
                    make_instructions(OpPop, &vec![]),
                ],
            },
        ];

        run_compiler_test(tests);
    }
    #[test]
    fn boolean_expression() {
        let tests = vec![
            CompilerTestCase {
                input: "true",
                expected_constants: vec![],
                expected_instructions: vec![
                    make_instructions(OpTrue, &vec![]),
                    make_instructions(OpPop, &vec![]),
                ],
            },
            CompilerTestCase {
                input: "false",
                expected_constants: vec![],
                expected_instructions: vec![
                    make_instructions(OpFalse, &vec![]),
                    make_instructions(OpPop, &vec![]),
                ],
            },
            CompilerTestCase {
                input: "1 > 2",
                expected_constants: vec![Object::Integer(1), Object::Integer(2)],
                expected_instructions: vec![
                    make_instructions(OpConst, &vec![0]),
                    make_instructions(OpConst, &vec![1]),
                    make_instructions(OpGreaterThan, &vec![]),
                    make_instructions(OpPop, &vec![]),
                ],
            },
            CompilerTestCase {
                input: "1 < 2",
                expected_constants: vec![Object::Integer(2), Object::Integer(1)],
                expected_instructions: vec![
                    make_instructions(OpConst, &vec![0]),
                    make_instructions(OpConst, &vec![1]),
                    make_instructions(OpGreaterThan, &vec![]),
                    make_instructions(OpPop, &vec![]),
                ],
            },
            CompilerTestCase {
                input: "1 == 2",
                expected_constants: vec![Object::Integer(1), Object::Integer(2)],
                expected_instructions: vec![
                    make_instructions(OpConst, &vec![0]),
                    make_instructions(OpConst, &vec![1]),
                    make_instructions(OpEqual, &vec![]),
                    make_instructions(OpPop, &vec![]),
                ],
            },
            CompilerTestCase {
                input: "1 != 2",
                expected_constants: vec![Object::Integer(1), Object::Integer(2)],
                expected_instructions: vec![
                    make_instructions(OpConst, &vec![0]),
                    make_instructions(OpConst, &vec![1]),
                    make_instructions(OpNotEqual, &vec![]),
                    make_instructions(OpPop, &vec![]),
                ],
            },
            CompilerTestCase {
                input: "true == false",
                expected_constants: vec![],
                expected_instructions: vec![
                    make_instructions(OpTrue, &vec![]),
                    make_instructions(OpFalse, &vec![]),
                    make_instructions(OpEqual, &vec![]),
                    make_instructions(OpPop, &vec![]),
                ],
            },
            CompilerTestCase {
                input: "true != false",
                expected_constants: vec![],
                expected_instructions: vec![
                    make_instructions(OpTrue, &vec![]),
                    make_instructions(OpFalse, &vec![]),
                    make_instructions(OpNotEqual, &vec![]),
                    make_instructions(OpPop, &vec![]),
                ],
            },
        ];

        run_compiler_test(tests);
    }

    #[test]
    fn conditions_only_if() {
        let tests = vec![CompilerTestCase {
            input: "if (true) { 10 }; 3333;",
            expected_constants: vec![Object::Integer(10), Object::Integer(3333)],
            expected_instructions: vec![
                make_instructions(OpTrue, &vec![]),
                make_instructions(OpJumpNotTruthy, &vec![10]),
                make_instructions(OpConst, &vec![0]),
                make_instructions(OpJump, &vec![11]),
                make_instructions(OpNull, &vec![]),
                make_instructions(OpPop, &vec![]),
                make_instructions(OpConst, &vec![1]),
                make_instructions(OpPop, &vec![]),
            ],
        }];

        run_compiler_test(tests);
    }

    #[test]
    fn conditions_with_else() {
        let tests = vec![CompilerTestCase {
            input: "if (true) { 10 } else { 20 }; 3333;",
            expected_constants: vec![
                Object::Integer(10),
                Object::Integer(20),
                Object::Integer(3333),
            ],
            expected_instructions: vec![
                make_instructions(OpTrue, &vec![]),
                make_instructions(OpJumpNotTruthy, &vec![10]),
                make_instructions(OpConst, &vec![0]),
                make_instructions(OpJump, &vec![13]),
                make_instructions(OpConst, &vec![1]),
                make_instructions(OpPop, &vec![]),
                make_instructions(OpConst, &vec![2]),
                make_instructions(OpPop, &vec![]),
            ],
        }];

        run_compiler_test(tests);
    }

    #[test]
    fn test_global_constants() {
        let tests = vec![
            CompilerTestCase {
                input: "let one = 1; let two = 2;",
                expected_constants: vec![Object::Integer(1), Object::Integer(2)],
                expected_instructions: vec![
                    make_instructions(OpConst, &vec![0]),
                    make_instructions(OpSetGlobal, &vec![0]),
                    make_instructions(OpConst, &vec![1]),
                    make_instructions(OpSetGlobal, &vec![1]),
                ],
            },
            CompilerTestCase {
                input: "let one = 1; one",
                expected_constants: vec![Object::Integer(1)],
                expected_instructions: vec![
                    make_instructions(OpConst, &vec![0]),
                    make_instructions(OpSetGlobal, &vec![0]),
                    make_instructions(OpGetGlobal, &vec![0]),
                    make_instructions(OpPop, &vec![]),
                ],
            },
            CompilerTestCase {
                input: "let one = 1; let two = one; two",
                expected_constants: vec![Object::Integer(1)],
                expected_instructions: vec![
                    make_instructions(OpConst, &vec![0]),
                    make_instructions(OpSetGlobal, &vec![0]),
                    make_instructions(OpGetGlobal, &vec![0]),
                    make_instructions(OpSetGlobal, &vec![1]),
                    make_instructions(OpGetGlobal, &vec![1]),
                    make_instructions(OpPop, &vec![]),
                ],
            },
        ];

        run_compiler_test(tests);
    }

    #[test]
    fn test_string() {
        let tests = vec![
            CompilerTestCase {
                input: "\"monkey\"",
                expected_constants: vec![Object::String("monkey".to_string())],
                expected_instructions: vec![
                    make_instructions(OpConst, &vec![0]),
                    make_instructions(OpPop, &vec![]),
                ],
            },
            CompilerTestCase {
                input: r#""mon" + "key""#,
                expected_constants: vec![
                    Object::String("mon".to_string()),
                    Object::String("key".to_string()),
                ],
                expected_instructions: vec![
                    make_instructions(OpConst, &vec![0]),
                    make_instructions(OpConst, &vec![1]),
                    make_instructions(OpAdd, &vec![]),
                    make_instructions(OpPop, &vec![]),
                ],
            },
        ];

        run_compiler_test(tests);
    }

    #[test]
    fn test_array() {
        let tests = vec![
            CompilerTestCase {
                input: "[]",
                expected_constants: vec![],
                expected_instructions: vec![
                    make_instructions(OpArray, &vec![0]),
                    make_instructions(OpPop, &vec![]),
                ],
            },
            CompilerTestCase {
                input: "[1, 2, 3]",
                expected_constants: vec![
                    Object::Integer(1),
                    Object::Integer(2),
                    Object::Integer(3),
                ],
                expected_instructions: vec![
                    make_instructions(OpConst, &vec![0]),
                    make_instructions(OpConst, &vec![1]),
                    make_instructions(OpConst, &vec![2]),
                    make_instructions(OpArray, &vec![3]),
                    make_instructions(OpPop, &vec![]),
                ],
            },
            CompilerTestCase {
                input: "[1 + 2, 3 - 4, 5 * 6]",
                expected_constants: vec![
                    Object::Integer(1),
                    Object::Integer(2),
                    Object::Integer(3),
                    Object::Integer(4),
                    Object::Integer(5),
                    Object::Integer(6),
                ],
                expected_instructions: vec![
                    make_instructions(OpConst, &vec![0]),
                    make_instructions(OpConst, &vec![1]),
                    make_instructions(OpAdd, &vec![]),
                    make_instructions(OpConst, &vec![2]),
                    make_instructions(OpConst, &vec![3]),
                    make_instructions(OpSub, &vec![]),
                    make_instructions(OpConst, &vec![4]),
                    make_instructions(OpConst, &vec![5]),
                    make_instructions(OpMul, &vec![]),
                    make_instructions(OpArray, &vec![3]),
                    make_instructions(OpPop, &vec![]),
                ],
            },
        ];

        run_compiler_test(tests);
    }

    #[test]
    fn test_hashmap() {
        let tests = vec![
            CompilerTestCase {
                input: "{}",
                expected_constants: vec![],
                expected_instructions: vec![
                    make_instructions(OpHash, &vec![0]),
                    make_instructions(OpPop, &vec![]),
                ],
            },
            CompilerTestCase {
                input: "{1: 2, 3: 4, 5: 6}",
                expected_constants: vec![
                    Object::Integer(1),
                    Object::Integer(2),
                    Object::Integer(3),
                    Object::Integer(4),
                    Object::Integer(5),
                    Object::Integer(6),
                ],
                expected_instructions: vec![
                    make_instructions(OpConst, &vec![0]),
                    make_instructions(OpConst, &vec![1]),
                    make_instructions(OpConst, &vec![2]),
                    make_instructions(OpConst, &vec![3]),
                    make_instructions(OpConst, &vec![4]),
                    make_instructions(OpConst, &vec![5]),
                    make_instructions(OpHash, &vec![6]),
                    make_instructions(OpPop, &vec![]),
                ],
            },
            CompilerTestCase {
                input: "{1: 2 + 3, 4: 5 * 6}",
                expected_constants: vec![
                    Object::Integer(1),
                    Object::Integer(2),
                    Object::Integer(3),
                    Object::Integer(4),
                    Object::Integer(5),
                    Object::Integer(6),
                ],
                expected_instructions: vec![
                    make_instructions(OpConst, &vec![0]),
                    make_instructions(OpConst, &vec![1]),
                    make_instructions(OpConst, &vec![2]),
                    make_instructions(OpAdd, &vec![]),
                    make_instructions(OpConst, &vec![3]),
                    make_instructions(OpConst, &vec![4]),
                    make_instructions(OpConst, &vec![5]),
                    make_instructions(OpMul, &vec![]),
                    make_instructions(OpHash, &vec![4]),
                    make_instructions(OpPop, &vec![]),
                ],
            },
        ];

        run_compiler_test(tests);
    }

    #[test]
    fn test_index() {
        let tests = vec![
            CompilerTestCase {
                input: "[1, 2, 3][1 + 1]",
                expected_constants: vec![
                    Object::Integer(1),
                    Object::Integer(2),
                    Object::Integer(3),
                    Object::Integer(1),
                    Object::Integer(1),
                ],
                expected_instructions: vec![
                    make_instructions(OpConst, &vec![0]),
                    make_instructions(OpConst, &vec![1]),
                    make_instructions(OpConst, &vec![2]),
                    make_instructions(OpArray, &vec![3]),
                    make_instructions(OpConst, &vec![3]),
                    make_instructions(OpConst, &vec![4]),
                    make_instructions(OpAdd, &vec![]),
                    make_instructions(OpIndex, &vec![]),
                    make_instructions(OpPop, &vec![]),
                ],
            },
            CompilerTestCase {
                input: "{1: 2 }[2 -1]",
                expected_constants: vec![
                    Object::Integer(1),
                    Object::Integer(2),
                    Object::Integer(2),
                    Object::Integer(1),
                ],
                expected_instructions: vec![
                    make_instructions(OpConst, &vec![0]),
                    make_instructions(OpConst, &vec![1]),
                    make_instructions(OpHash, &vec![2]),
                    make_instructions(OpConst, &vec![2]),
                    make_instructions(OpConst, &vec![3]),
                    make_instructions(OpSub, &vec![]),
                    make_instructions(OpIndex, &vec![]),
                    make_instructions(OpPop, &vec![]),
                ],
            },
        ];

        run_compiler_test(tests);
    }
}
