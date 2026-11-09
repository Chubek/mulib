; MuLL short circuit operators return operands, not coerced Booleans.
; No arithmetic reassociation: numbers are finite IEEE binary64 values.
(rewrite true-and
  (binary "and" (names) ?site (literal "true" (names) ?left) ?x) ?x)
(rewrite false-or
  (binary "or" (names) ?site (literal "false" (names) ?left) ?x) ?x)
(rewrite nil-or
  (binary "or" (names) ?site (literal "nil" (names) ?left) ?x) ?x)
(rewrite false-and
  (binary "and" (names) ?site (literal "false" (names) ?left) ?x)
  (literal "false" (names) ?site))
(rewrite nil-and
  (binary "and" (names) ?site (literal "nil" (names) ?left) ?x)
  (literal "nil" (names) ?site))
(rewrite true-or
  (binary "or" (names) ?site (literal "true" (names) ?left) ?x)
  (literal "true" (names) ?site))
(rewrite positive-number
  (unary "+" (names) ?site (number ?value (names) ?origin))
  (number ?value (names) ?origin))
(rewrite not-true
  (unary "not" (names) ?site (literal "true" (names) ?origin))
  (literal "false" (names) ?site))
(rewrite not-false
  (unary "not" (names) ?site (literal "false" (names) ?origin))
  (literal "true" (names) ?site))
(rewrite not-nil
  (unary "not" (names) ?site (literal "nil" (names) ?origin))
  (literal "true" (names) ?site))
