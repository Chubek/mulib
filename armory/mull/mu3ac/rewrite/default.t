; Pure, local rewrites on already evaluated scalar registers. No FP reassociation.
(rewrite positive-number (positive (number ?r)) (use ?r))
(rewrite double-negative (negative (negative ?r)) (use ?r))
(rewrite not-true (not (literal "true")) (literal "false"))
(rewrite not-false (not (literal "false")) (literal "true"))
(rewrite not-nil (not (literal "nil")) (literal "true"))
(rewrite not-number (not (number ?r)) (literal "false"))
