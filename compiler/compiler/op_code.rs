use std::collections::HashMap;
use std::fmt::{self, Display, Formatter};
use std::io::{Cursor, Read};
use std::sync::OnceLock;
use strum::{EnumCount, EnumIter};

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Instructions {
    pub bytes: Vec<u8>,
}

#[derive(Debug, Clone)]
pub struct OpcodeDefinition {
    pub name: &'static str,
    pub operand_widths: &'static [usize],
}

#[repr(u8)]
#[derive(Debug, Hash, Eq, Clone, Copy, PartialEq, EnumCount, EnumIter)]
pub enum Opcode {
    OpConst,
    OpAdd,
    OpPop,
    OpSub,
    OpMul,
    OpDiv,
    OpTrue,
    OpFalse,
    OpEqual,
    OpNotEqual,
    OpGreaterThan,
    OpMinus,
    OpBang,
    OpJumpNotTruthy,
    OpJump,
    OpNull,
    OpGetGlobal,
    OpSetGlobal,
    OpArray,
    OpHash,
    OpIndex,
    OpCall,
    OpReturnValue,
    OpReturn,
    OpGetLocal,
    OpSetLocal,
    OpGetBuiltin,
    OpClosure,
    OpGetFree,
    OpCurrentClosure,
}

pub static DEFINITIONS: OnceLock<HashMap<Opcode, OpcodeDefinition>> = OnceLock::new();

pub fn definitions() -> &'static HashMap<Opcode, OpcodeDefinition> {
    DEFINITIONS.get_or_init(|| {
        let mut m = HashMap::new();
        insert_def(&mut m, Opcode::OpConst, "OpConst", &[2]);
        insert_def(&mut m, Opcode::OpAdd, "OpAdd", &[]);
        insert_def(&mut m, Opcode::OpPop, "OpPop", &[]);
        insert_def(&mut m, Opcode::OpSub, "OpSub", &[]);
        insert_def(&mut m, Opcode::OpMul, "OpMul", &[]);
        insert_def(&mut m, Opcode::OpDiv, "OpDiv", &[]);
        insert_def(&mut m, Opcode::OpTrue, "OpTrue", &[]);
        insert_def(&mut m, Opcode::OpFalse, "OpFalse", &[]);
        insert_def(&mut m, Opcode::OpEqual, "OpEqual", &[]);
        insert_def(&mut m, Opcode::OpNotEqual, "OpNotEqual", &[]);
        insert_def(&mut m, Opcode::OpGreaterThan, "OpGreaterThan", &[]);
        insert_def(&mut m, Opcode::OpMinus, "OpMinus", &[]);
        insert_def(&mut m, Opcode::OpBang, "OpBang", &[]);
        insert_def(&mut m, Opcode::OpJumpNotTruthy, "OpJumpNotTruthy", &[2]);
        insert_def(&mut m, Opcode::OpJump, "OpJump", &[2]);
        insert_def(&mut m, Opcode::OpNull, "OpNull", &[]);
        insert_def(&mut m, Opcode::OpGetGlobal, "OpGetGlobal", &[2]);
        insert_def(&mut m, Opcode::OpSetGlobal, "OpSetGlobal", &[2]);
        insert_def(&mut m, Opcode::OpArray, "OpArray", &[2]);
        insert_def(&mut m, Opcode::OpHash, "OpHash", &[2]);
        insert_def(&mut m, Opcode::OpIndex, "OpIndex", &[]);
        insert_def(&mut m, Opcode::OpCall, "OpCall", &[1]);
        insert_def(&mut m, Opcode::OpReturn, "OpReturn", &[]);
        insert_def(&mut m, Opcode::OpReturnValue, "OpReturnValue", &[]);
        insert_def(&mut m, Opcode::OpGetLocal, "OpGetLocal", &[1]);
        insert_def(&mut m, Opcode::OpSetLocal, "OpSetLocal", &[1]);
        insert_def(&mut m, Opcode::OpGetBuiltin, "OpGetBuiltin", &[1]);
        insert_def(&mut m, Opcode::OpClosure, "OpClosure", &[2, 1]);
        insert_def(&mut m, Opcode::OpGetFree, "OpGetFree", &[1]);
        insert_def(&mut m, Opcode::OpCurrentClosure, "OpCurrentClosure", &[]);
        m
    })
}

fn insert_def(
    map: &mut HashMap<Opcode, OpcodeDefinition>,
    op: Opcode,
    name: &'static str,
    widths: &'static [usize],
) {
    map.insert(
        op,
        OpcodeDefinition {
            name,
            operand_widths: widths,
        },
    );
}

pub fn make(op: Opcode, operands: &[usize]) -> Result<Instructions, String> {
    let def = definitions()
        .get(&op)
        .ok_or_else(|| format!("Undefined opcode: {:?}", op))?;

    if def.operand_widths.len() != operands.len() {
        return Err(format!(
            "Operand count mismatch: expected {}, got {}",
            def.operand_widths.len(),
            operands.len()
        ));
    }

    let mut bytes = Vec::with_capacity(1 + def.operand_widths.iter().sum::<usize>());
    bytes.push(op as u8);

    for (&operand, &width) in operands.iter().zip(def.operand_widths) {
        match width {
            2 => bytes.extend_from_slice(&(operand as u16).to_be_bytes()),
            1 => bytes.push(operand as u8),
            w => return Err(format!("Unsupported operand width: {}", w)),
        }
    }

    Ok(Instructions { bytes })
}

pub fn read_operands(
    def: &OpcodeDefinition,
    mut bytes: &[u8],
) -> Result<(Vec<usize>, usize), String> {
    let mut operands = Vec::with_capacity(def.operand_widths.len());
    let mut bytes_read = 0;

    for &width in def.operand_widths {
        match width {
            2 => {
                let mut buf = [0u8; 2];
                bytes.read_exact(&mut buf).map_err(|e| e.to_string())?;
                operands.push(u16::from_be_bytes(buf) as usize);
                bytes_read += 2;
            }
            1 => {
                let mut buf = [0u8; 1];
                bytes.read_exact(&mut buf).map_err(|e| e.to_string())?;
                operands.push(buf[0] as usize);
                bytes_read += 1;
            }
            0 => operands.push(0), // For 0-width operands
            w => return Err(format!("Unsupported operand width: {}", w)),
        }
    }

    Ok((operands, bytes_read))
}

impl Instructions {
    pub fn merge<I: IntoIterator<Item = Self>>(instructions: I) -> Self {
        Self {
            bytes: instructions.into_iter().flat_map(|i| i.bytes).collect(),
        }
    }

    pub fn disassemble(&self) -> String {
        let mut output = String::new();
        let mut cursor = Cursor::new(&self.bytes);

        while let Ok(op) = cursor.read_u8() {
            let pos = cursor.position() as usize - 1;
            let opcode = Opcode::try_from(op).unwrap_or_else(|_| {
                panic!("Invalid opcode byte: 0x{:02x} at position {}", op, pos)
            });

            let def = definitions()
                .get(&opcode)
                .unwrap_or_else(|| panic!("Missing definition for {:?}", opcode));

            let (operands, read) = read_operands(def, &self.bytes[pos + 1..])
                .unwrap_or_else(|e| panic!("Error reading operands at {}: {}", pos, e));

            output.push_str(&format!("{:04} {}\n", pos, def.display(&operands)));

            cursor.set_position((pos + 1 + read) as u64);
        }

        output
    }

    pub fn string(&self) -> String {
        self.disassemble()
    }

    pub fn data(&self) -> &[u8] {
        &self.bytes
    }

    pub fn len(&self) -> usize {
        self.bytes.len()
    }

    pub fn is_empty(&self) -> bool {
        self.bytes.is_empty()
    }
}

impl Display for OpcodeDefinition {
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.name)?;
        for width in self.operand_widths {
            write!(f, " {}", width)?;
        }
        Ok(())
    }
}

impl OpcodeDefinition {
    fn display(&self, operands: &[usize]) -> String {
        match self.operand_widths.len() {
            0 => self.name.to_string(),
            _ => format!(
                "{} {}",
                self.name,
                operands
                    .iter()
                    .map(ToString::to_string)
                    .collect::<Vec<_>>()
                    .join(" ")
            ),
        }
    }
}

trait ReadExt: Read {
    fn read_u8(&mut self) -> Result<u8, std::io::Error> {
        let mut buf = [0u8; 1];
        self.read_exact(&mut buf)?;
        Ok(buf[0])
    }
}

impl<R: Read> ReadExt for R {}

impl TryFrom<u8> for Opcode {
    type Error = ();

    fn try_from(value: u8) -> Result<Self, Self::Error> {
        if value < Opcode::COUNT as u8 {
            // SAFETY: We've checked the bounds above
            Ok(unsafe { std::mem::transmute(value) })
        } else {
            Err(())
        }
    }
}

// Add missing functions that are imported by other modules
pub fn cast_u8_to_opcode(byte: u8) -> Opcode {
    Opcode::try_from(byte).unwrap_or_else(|_| panic!("Invalid opcode byte: 0x{:02x}", byte))
}

pub fn make_instructions(op: Opcode, operands: &[usize]) -> Instructions {
    make(op, operands).expect("Failed to make instructions")
}

pub fn concat_instructions(instructions: Vec<Instructions>) -> Instructions {
    Instructions::merge(instructions)
}
