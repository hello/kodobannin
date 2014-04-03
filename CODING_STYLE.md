* `static` variables and functions should be prefixed with an underscore (`_`). This is for variables and functions only, not type declarations.

* 4-space tabs.

* 4-space indentation.

* Put the return type for a function declaration on its own line.

* Return parameters in functions should be prefixed with `out_`.

* One-line expressions inside the arm of an `if()` does _not_ need braces. Put the expression on its one line.

* Cast the return value of a function to (void) if you are purposefully ignoring the return value. For example, if a function has a return type of int, but is documented to never fail and always return 0, which makes the return value useless.

* structs and unions should retain struct/union

* prefer static const to enum to #define.
