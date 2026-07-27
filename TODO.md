# Compiler TODO

## Compiler Infrastructure
    - Scoped symbol table in AST (Stack of tables)
    - Arena allocator
        - TRIGGER: compilation speed is slow
        - TRIGGER: memory usage exceeds a reasonable limit (100MB?)
        - TRIGGER: simply desire to
    - Memory usage tracker (for debug builds)
    - Internal compiler errors in various places

### Compiler driver

Currently holds the options, interner, tokenizer, parser, and codegen driver.

## Features

## Improvements
    - Error handling:
        - tests/ch06/parser_invalid/extra/goto_without_label.c
        - tests/ch06/parser_invalid/extra/kw_label.c
        - tests/ch06/semantics_invalid/extra/use_label_as_variable.c
