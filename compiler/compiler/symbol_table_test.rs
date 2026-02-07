#[cfg(test)]
mod tests {
    use crate::symbol_table::{SymbolScope, SymbolTable};
    #[test]
    fn test_define() {
        let symbol_table = SymbolTable::new();
        let symbol = symbol_table.define("x");
        assert_eq!(symbol.name, "x");
        assert_eq!(symbol.scope, SymbolScope::Global);
        assert_eq!(symbol.index, 0);
    }

    #[test]
    fn test_resolve() {
        let symbol_table = SymbolTable::new();
        let symbol = symbol_table.define("x");
        assert_eq!(symbol_table.resolve("x"), Some(symbol));
    }
}
