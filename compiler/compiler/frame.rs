use crate::op_code::Instructions;
use object::Closure;

#[derive(Debug, Clone)]
pub struct Frame {
    pub closure: Closure,
    pub ip: i32,
    pub base_pointer: usize,
}

impl Frame {
    pub fn new(closure: Closure, base_pointer: usize) -> Self {
        Frame {
            closure, // Field and parameter name alignment
            ip: -1,  // Starts before first instruction
            base_pointer,
        }
    }

    pub fn instructions(&self) -> Instructions {
        Instructions {
            bytes: self.closure.func.instructions.clone(),
        }
    }
}
