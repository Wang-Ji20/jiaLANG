# Design: Parsing

The *parser* turns the flat list of tokens into a tree that shows how the
pieces nest: which operator applies to which operands, which statements
belong to which block.

```cpp
Result<TranslationUnit> parse(std::span<const Token> tokens);
```

## Decisions

**Recursive descent, by hand.** Each grammar rule is one method:
`parseStatement`, `parseUnary`, and so on. The 2022 compiler used Bison,
a parser generator. We switched for three reasons:

1. C's grammar is not context-free. `T * x;` is a declaration if `T` is
   a `typedef` name and a multiplication otherwise. A hand-written
   parser can ask "is `T` a type?" in the middle of a rule; a Bison
   grammar cannot do that cleanly. We will need this for real C.
2. Error messages and recovery are easier to control.
3. It removes a build-time tool and its generated code.

**Precedence climbing for binary operators.** Instead of one method per
precedence level, `parseBinary(min)` reads an operand, then keeps taking
operators whose precedence is at least `min`. A small table
(`binaryInfo`) holds the levels:

| Level | Operators |
|---|---|
| 1 | `\|\|` |
| 2 | `&&` |
| 3 | `==` `!=` |
| 4 | `<` `>` `<=` `>=` |
| 5 | `+` `-` |
| 6 | `*` `/` `%` |

Assignment sits below all of these and associates to the right, so
`a = b = 1` means `a = (b = 1)`. Unary operators, casts, and `sizeof`
bind tighter than any binary operator, and postfix calls and indexing
bind tightest of all.

**Declarators are parsed in two steps.** C writes types "inside out":
`int *a[3]` is an array of three pointers, while `int (*b)[3]` is a
pointer to an array. The parser first records the *derivations* it sees
(pointer, array of N, function taking these parameters) in the order
they apply to the base type, then `resolve` applies them one by one.
For `int *a[3]` the list is `[pointer, array 3]`: start from `int`, make
a pointer, make an array of those. This keeps the tricky part, the
order, in one small function.

**Folding array sizes early.** Array sizes must be constant, so the
parser folds them with `evaluateConstant` and builds the array type at
once. `int a[2 * 8]` has type `int[16]` straight out of the parser.

**Errors do not stop the parser.** On a syntax error the parser records
a diagnostic and *synchronizes*: it skips tokens to the next `;` (inside
a block) or to the end of the current declaration (at file scope), then
carries on. A broken statement does not take the rest of its function
with it.

## Grammar

The full grammar, in the notation of the C standard, is in the
[user manual](../user-manual.md#language-reference).
