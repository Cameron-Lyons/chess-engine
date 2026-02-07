use std::cell::{Cell, RefCell};
use std::collections::HashMap;
use std::rc::Rc;

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum SymbolScope {
    Local,
    Global,
    Builtin,
    Free,
    Function,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Symbol {
    pub name: String,
    pub scope: SymbolScope,
    pub index: usize,
}

#[derive(Debug, Clone)]
pub struct SymbolTable {
    outer: Option<Rc<SymbolTable>>,
    symbols: RefCell<HashMap<String, Rc<Symbol>>>,
    free_symbols: RefCell<Vec<Rc<Symbol>>>,
    num_definitions: Cell<usize>,
}

impl SymbolTable {
    pub fn new() -> Self {
        Self {
            symbols: RefCell::new(HashMap::new()),
            free_symbols: RefCell::new(Vec::new()),
            num_definitions: Cell::new(0),
            outer: None,
        }
    }

    pub fn new_enclosed(outer: Rc<Self>) -> Self {
        Self {
            symbols: RefCell::new(HashMap::new()),
            free_symbols: RefCell::new(Vec::new()),
            num_definitions: Cell::new(0),
            outer: Some(outer),
        }
    }

    pub fn define(&self, name: &str) -> Rc<Symbol> {
        let scope = if self.outer.is_some() {
            SymbolScope::Local
        } else {
            SymbolScope::Global
        };

        let name = name.to_string();
        let symbol = Rc::new(Symbol {
            name: name.clone(),
            index: self.num_definitions.get(),
            scope,
        });

        self.symbols.borrow_mut().insert(name, Rc::clone(&symbol));
        self.num_definitions.set(self.num_definitions.get() + 1);
        symbol
    }

    pub fn resolve(&self, name: &str) -> Option<Rc<Symbol>> {
        if let Some(symbol) = self.symbols.borrow().get(name) {
            return Some(Rc::clone(symbol));
        }

        if let Some(outer) = &self.outer {
            let outer_symbol = outer.resolve(name);

            if let Some(symbol) = outer_symbol {
                match symbol.scope {
                    SymbolScope::Local | SymbolScope::Free => {
                        return Some(self.define_free_checked(symbol));
                    }
                    _ => return Some(symbol),
                }
            }
        }

        None
    }

    pub fn define_builtin(&self, index: usize, name: &str) -> Rc<Symbol> {
        let name = name.to_string();
        let symbol = Rc::new(Symbol {
            name: name.clone(),
            index,
            scope: SymbolScope::Builtin,
        });
        self.symbols.borrow_mut().insert(name, Rc::clone(&symbol));
        symbol
    }

    pub fn define_function_name(&self, name: &str) -> Rc<Symbol> {
        let name = name.to_string();
        let symbol = Rc::new(Symbol {
            name: name.clone(),
            index: 0,
            scope: SymbolScope::Function,
        });
        self.symbols.borrow_mut().insert(name, Rc::clone(&symbol));
        symbol
    }

    fn define_free_checked(&self, original: Rc<Symbol>) -> Rc<Symbol> {
        if let Some(existing) = self.symbols.borrow().get(&original.name) {
            return Rc::clone(existing);
        }
        self.define_free(original)
    }

    fn define_free(&self, original: Rc<Symbol>) -> Rc<Symbol> {
        self.free_symbols.borrow_mut().push(Rc::clone(&original));
        let symbol = Rc::new(Symbol {
            name: original.name.clone(),
            index: self.free_symbols.borrow().len() - 1,
            scope: SymbolScope::Free,
        });
        self.symbols
            .borrow_mut()
            .insert(original.name.clone(), Rc::clone(&symbol));
        symbol
    }

    // Accessor methods
    pub fn num_definitions(&self) -> usize {
        self.num_definitions.get()
    }

    pub fn free_symbols(&self) -> Vec<Rc<Symbol>> {
        self.free_symbols.borrow().clone()
    }

    pub fn outer(&self) -> Option<&Rc<Self>> {
        self.outer.as_ref()
    }
}
