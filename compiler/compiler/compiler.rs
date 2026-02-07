use object::builtins::BuiltIns;
use std::rc::Rc;

use object::Object;
use parser::ast::{BlockStatement, Expression, Literal, Node, Statement};
use parser::lexer::token::TokenKind;

use crate::op_code::Opcode::*;
use crate::op_code::{cast_u8_to_opcode, make_instructions, Instructions, Opcode};
use crate::symbol_table::{Symbol, SymbolScope, SymbolTable};

struct CompilationScope {
    instructions: Instructions,
    last_instruction: EmittedInstruction,
    previous_instruction: EmittedInstruction,
}

impl Default for CompilationScope {
    fn default() -> Self {
        Self {
            instructions: Instructions { bytes: vec![] },
            last_instruction: EmittedInstruction {
                opcode: OpNull,
                position: 0,
            },
            previous_instruction: EmittedInstruction {
                opcode: OpNull,
                position: 0,
            },
        }
    }
}

pub struct Compiler {
    pub constants: Vec<Rc<Object>>,
    pub symbol_table: SymbolTable,
    scopes: Vec<CompilationScope>,
    scope_index: usize,
}

pub struct Bytecode {
    pub instructions: Instructions,
    pub constants: Vec<Rc<Object>>,
}

#[derive(Clone)]
pub struct EmittedInstruction {
    pub opcode: Opcode,
    pub position: usize,
}

type CompileError = String;

impl Compiler {
    const PLACEHOLDER_ADDRESS: usize = 0;

    pub fn new() -> Self {
        let main_scope = CompilationScope::default();

        let symbol_table = SymbolTable::new();
        for (key, value) in BuiltIns.iter().enumerate() {
            symbol_table.define_builtin(key, value.0);
        }

        Compiler {
            constants: vec![],
            symbol_table,
            scopes: vec![main_scope],
            scope_index: 0,
        }
    }

    pub fn new_with_state(symbol_table: SymbolTable, constants: Vec<Rc<Object>>) -> Self {
        let mut compiler = Self::new();
        compiler.constants = constants;
        compiler.symbol_table = symbol_table;
        compiler
    }

    pub fn compile(&mut self, node: &Node) -> Result<Bytecode, CompileError> {
        match node {
            Node::Program(p) => {
                for stmt in &p.body {
                    self.compile_stmt(stmt)?;
                }
            }
            Node::Statement(s) => {
                self.compile_stmt(s)?;
            }
            Node::Expression(e) => {
                self.compile_expr(e)?;
            }
        }

        Ok(self.bytecode())
    }

    fn compile_stmt(&mut self, s: &Statement) -> Result<(), CompileError> {
        match s {
            Statement::Let(let_statement) => {
                let name = match &let_statement.identifier.kind {
                    TokenKind::IDENTIFIER { name } => name,
                    _ => return Err("Expected identifier".to_string()),
                };
                let symbol = self.symbol_table.define(name);
                self.compile_expr(&let_statement.expr)?;
                if symbol.scope == SymbolScope::Global {
                    self.emit(Opcode::OpSetGlobal, &[symbol.index]);
                } else {
                    self.emit(Opcode::OpSetLocal, &[symbol.index]);
                }
                Ok(())
            }
            Statement::Return(r) => {
                self.compile_expr(&r.argument)?;
                self.emit(Opcode::OpReturnValue, &[]);
                Ok(())
            }
            Statement::Expr(e) => {
                self.compile_expr(e)?;
                self.emit(OpPop, &[]);
                Ok(())
            }
        }
    }

    fn compile_expr(&mut self, e: &Expression) -> Result<(), CompileError> {
        match e {
            Expression::IDENTIFIER(identifier) => {
                let symbol = self.symbol_table.resolve(&identifier.name);
                match symbol {
                    Some(symbol) => {
                        self.load_symbol(&symbol);
                    }
                    None => {
                        return Err(format!("Undefined variable '{}'", identifier.name));
                    }
                }
            }
            Expression::LITERAL(l) => match l {
                Literal::Integer(i) => {
                    let int = Object::Integer(i.raw);
                    let operands = vec![self.add_constant(int)];
                    self.emit(OpConst, &operands);
                }
                Literal::Boolean(i) => {
                    if i.raw {
                        self.emit(OpTrue, &[]);
                    } else {
                        self.emit(OpFalse, &[]);
                    }
                }
                Literal::String(s) => {
                    let string_object = Object::String(s.raw.clone());
                    let operands = vec![self.add_constant(string_object)];
                    self.emit(OpConst, &operands);
                }
                Literal::Array(array) => {
                    for element in array.elements.iter() {
                        self.compile_expr(element)?;
                    }
                    self.emit(OpArray, &[array.elements.len()]);
                }
                Literal::Hash(hash) => {
                    for (key, value) in hash.elements.iter() {
                        self.compile_expr(key)?;
                        self.compile_expr(value)?;
                    }
                    self.emit(OpHash, &[hash.elements.len() * 2]);
                }
            },
            Expression::PREFIX(prefix) => {
                self.compile_expr(&prefix.operand)?;
                match prefix.op.kind {
                    TokenKind::MINUS => {
                        self.emit(OpMinus, &[]);
                    }
                    TokenKind::BANG => {
                        self.emit(OpBang, &[]);
                    }
                    _ => {
                        return Err(format!("unexpected prefix op: {}", prefix.op));
                    }
                }
            }
            Expression::INFIX(infix) => {
                if infix.op.kind == TokenKind::LT {
                    self.compile_expr(&infix.right)?;
                    self.compile_expr(&infix.left)?;
                    self.emit(Opcode::OpGreaterThan, &[]);
                    return Ok(());
                }
                self.compile_expr(&infix.left)?;
                self.compile_expr(&infix.right)?;
                match infix.op.kind {
                    TokenKind::PLUS => self.emit(OpAdd, &[]),
                    TokenKind::MINUS => self.emit(OpSub, &[]),
                    TokenKind::ASTERISK => self.emit(OpMul, &[]),
                    TokenKind::SLASH => self.emit(OpDiv, &[]),
                    TokenKind::GT => self.emit(Opcode::OpGreaterThan, &[]),
                    TokenKind::EQ => self.emit(Opcode::OpEqual, &[]),
                    TokenKind::NotEq => self.emit(Opcode::OpNotEqual, &[]),
                    _ => return Err(format!("unexpected infix op: {}", infix.op)),
                };
            }
            Expression::IF(if_node) => {
                self.compile_expr(&if_node.condition)?;
                let jump_not_truthy = self.emit(OpJumpNotTruthy, &[Self::PLACEHOLDER_ADDRESS]);
                self.compile_block_statement(&if_node.consequent)?;
                if self.last_instruction_is(OpPop) {
                    self.remove_last_pop();
                }

                let jump_pos = self.emit(OpJump, &[Self::PLACEHOLDER_ADDRESS]);
                let after_consequence_location = self.current_instruction().bytes.len();
                self.change_operand(jump_not_truthy, after_consequence_location);

                if let Some(alternate) = &if_node.alternate {
                    self.compile_block_statement(alternate)?;
                    if self.last_instruction_is(OpPop) {
                        self.remove_last_pop();
                    }
                } else {
                    self.emit(OpNull, &[]);
                }

                let after_alternative_location = self.current_instruction().bytes.len();
                self.change_operand(jump_pos, after_alternative_location);
            }
            Expression::Index(index) => {
                self.compile_expr(&index.object)?;
                self.compile_expr(&index.index)?;
                self.emit(OpIndex, &[]);
            }
            Expression::FUNCTION(f) => {
                self.enter_scope();
                for param in &f.params {
                    self.symbol_table.define(&param.name);
                }
                self.compile_block_statement(&f.body)?;
                if self.last_instruction_is(OpPop) {
                    self.replace_last_pop_with_return();
                }
                if !self.last_instruction_is(OpReturnValue) {
                    self.emit(OpReturn, &[]);
                }
                let num_locals = self.symbol_table.num_definitions();
                let free_symbols = self.symbol_table.free_symbols().to_vec();
                let instructions = self.leave_scope();
                for symbol in &free_symbols {
                    self.load_symbol(symbol);
                }

                let compiled_function = Rc::new(object::CompiledFunction {
                    instructions: instructions.bytes,
                    num_locals,
                    num_parameters: f.params.len(),
                });

                let operands = vec![
                    self.add_constant(Object::CompiledFunction(compiled_function)),
                    free_symbols.len(),
                ];
                self.emit(OpClosure, &operands);
            }
            Expression::FunctionCall(fc) => {
                self.compile_expr(&fc.callee)?;
                for arg in &fc.arguments {
                    self.compile_expr(arg)?;
                }
                self.emit(OpCall, &[fc.arguments.len()]);
            }
        }

        Ok(())
    }

    fn load_symbol(&mut self, symbol: &Rc<Symbol>) {
        match symbol.scope {
            SymbolScope::Global => self.emit(OpGetGlobal, &[symbol.index]),
            SymbolScope::Local => self.emit(OpGetLocal, &[symbol.index]),
            SymbolScope::Builtin => self.emit(OpGetBuiltin, &[symbol.index]),
            SymbolScope::Free => self.emit(OpGetFree, &[symbol.index]),
            SymbolScope::Function => self.emit(OpCurrentClosure, &[]),
        };
    }

    pub fn bytecode(&self) -> Bytecode {
        Bytecode {
            instructions: self.current_instruction().clone(),
            constants: self.constants.clone(),
        }
    }

    pub fn add_constant(&mut self, obj: Object) -> usize {
        self.constants.push(Rc::new(obj));
        self.constants.len() - 1
    }

    pub fn emit(&mut self, op: Opcode, operands: &[usize]) -> usize {
        let ins = make_instructions(op, operands);
        let pos = self.add_instructions(&ins);
        self.set_last_instruction(op, pos);
        pos
    }

    fn compile_block_statement(
        &mut self,
        block_statement: &BlockStatement,
    ) -> Result<(), CompileError> {
        for stmt in &block_statement.body {
            self.compile_stmt(stmt)?;
        }
        Ok(())
    }

    pub fn add_instructions(&mut self, ins: &Instructions) -> usize {
        let pos = self.current_instruction().bytes.len();
        self.scopes[self.scope_index]
            .instructions
            .bytes
            .extend_from_slice(&ins.bytes);
        pos
    }

    fn set_last_instruction(&mut self, op: Opcode, pos: usize) {
        let previous_instruction = self.scopes[self.scope_index].last_instruction.clone();
        let last_instruction = EmittedInstruction {
            opcode: op,
            position: pos,
        };
        self.scopes[self.scope_index].last_instruction = last_instruction;
        self.scopes[self.scope_index].previous_instruction = previous_instruction;
    }

    fn last_instruction_is(&self, op: Opcode) -> bool {
        if self.current_instruction().bytes.is_empty() {
            return false;
        }
        self.scopes[self.scope_index].last_instruction.opcode == op
    }

    fn remove_last_pop(&mut self) {
        let last = self.scopes[self.scope_index].last_instruction.clone();
        let previous = self.scopes[self.scope_index].previous_instruction.clone();

        self.scopes[self.scope_index]
            .instructions
            .bytes
            .truncate(last.position);
        self.scopes[self.scope_index].last_instruction = previous;
    }

    fn replace_instruction(&mut self, pos: usize, new_instruction: &Instructions) {
        let ins = &mut self.scopes[self.scope_index].instructions.bytes;
        ins.splice(
            pos..pos + new_instruction.bytes.len(),
            new_instruction.bytes.iter().cloned(),
        );
    }

    fn replace_last_pop_with_return(&mut self) {
        let last_pos = self.scopes[self.scope_index].last_instruction.position;
        let ins = make_instructions(OpReturnValue, &[]);
        self.replace_instruction(last_pos, &ins);
        self.scopes[self.scope_index].last_instruction.opcode = OpReturnValue;
    }

    fn change_operand(&mut self, pos: usize, operand: usize) {
        let op = cast_u8_to_opcode(self.current_instruction().bytes[pos]);
        let ins = make_instructions(op, &[operand]);
        self.replace_instruction(pos, &ins);
    }

    fn current_instruction(&self) -> &Instructions {
        &self.scopes[self.scope_index].instructions
    }

    fn enter_scope(&mut self) {
        self.scopes.push(CompilationScope::default());
        self.scope_index += 1;
        self.symbol_table = SymbolTable::new_enclosed(Rc::new(self.symbol_table.clone()));
    }

    fn leave_scope(&mut self) -> Instructions {
        let instructions = self.current_instruction().clone();
        self.scopes.pop();
        self.scope_index -= 1;
        self.symbol_table = self.symbol_table.outer().as_ref().unwrap().as_ref().clone();
        instructions
    }
}
