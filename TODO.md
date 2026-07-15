# Compiler TODO

## Compiler Infrastructure
    - Scoped symbol table in AST (Stack of tables)
    - Arena allocator
        - TRIGGER: compilation speed is slow
        - TRIGGER: memory usage exceeds a reasonable limit (100MB?)
        - TRIGGER: simply desire to
    - Memory usage tracker (for debug builds)
    - Internal compiler errors in various places
    - Compiler driver (holds the options, tokenizer, parser, codgen generator, etc.)
    - String Interner

### String Interner
    - Find places past parser/sem-analysis where interner could be used.
