# Conditional probability

Define `MULIB_IMPL_CONDITIONAL_PROBABLITY`; link `-lm` where required by
your compiler. [Example](../examples/modules/conditional_probablity.c).
The supplied filename spelling is preserved; function names spell probability
correctly.

`mulib_conditional_probability(joint,evidence)` returns P(A and B)/P(B),
requiring 0 <= joint <= evidence <= 1 and evidence > 0.
`mulib_conditional_bayes(likelihood,prior,evidence)` computes the joint as
P(B|A)*P(A). Individual inputs must be finite probabilities and the computed
joint must not exceed evidence. Invalid/undefined results return NAN.
No allocations, ownership, event storage, or independence assumptions are involved.
