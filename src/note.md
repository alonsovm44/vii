# NOTE
io.c works fine, i am working to migrate io source code into io so we can bootstrap it

## Bootstrapping Prerequisites — Done

5 new built-in keywords added to io.c for self-hosting support:

- `len x` — returns length of string or list (needed to iterate source code)
- `ord s` — returns char code of first character (needed to classify chars in lexer)
- `chr n` — returns single-char string from code (needed to build strings char-by-char)
- `tonum s` — parses string to number (needed to convert token text to values)
- `tostr n` — converts number to string (needed to build output strings)

These are implemented in: lexer, parser, interpreter, and codegen (compile-to-C).

Next step: write the self-hosted interpreter/compiler in io.io.