# Discrete hidden Markov models

Define `MULIB_IMPL_HIDDEN_MARKOV_CHAIN`, `MULIB_IMPL_MARKOV_CHAIN` and
`MULIB_IMPL_PROBABILITY`; link `-lm`.
[Example](../examples/modules/hidden_markov_chain.c).

Models borrow initial[states], transition[states*states] and
emission[states*symbols]. All rows are stochastic with tolerance 1e-9.
Emission at time zero uses initial state before any transition. Observations
are zero-based symbol IDs. Arrays remain caller-owned and immutable during calls.

Forward inference rescales every observation and accumulates log scale factors,
using long-double working values, O(length*states²) time and O(states) space.
Returns 1 with a log likelihood: zero for empty observations, -INFINITY for an
impossible sequence. Invalid input, dimension overflow or OOM returns 0 without
changing output. Extremely small products remain limited by platform floating
point. This module does not train HMMs or implement continuous emissions.
