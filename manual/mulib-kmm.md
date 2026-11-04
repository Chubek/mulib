# Kernel mean matching (KMM)

Define `MULIB_IMPL_KMM`; link `-lm`. [Example](../examples/modules/kmm.c).
KMM here means **kernel mean matching**, not k-means clustering.

Solves min 0.5 wᵀKw - κᵀw subject to 0<=w<=bound and sum(w)=n, the
epsilon=0 variant of KMM. Caller supplies n*n symmetric positive-semidefinite
kernel entries and κ[i]=(n/m) sum_j kernel(source_i,target_j). Kernel
construction is external. Exact symmetry and finite entries are checked;
positive semidefiniteness/conditioning are caller preconditions, not verified.
Require n>0, bound>=1, positive finite tolerance and positive iteration budget.

Projected gradient uses a row-sum Lipschitz bound and 128 bisection steps for
box/simplex projection. Return 1 for successive max coordinate change <=
tolerance, 2 on iteration exhaustion (approximate weights), 0 invalid/OOM/
numerical failure. Stopping is not an optimality certificate; equality is met
only to floating-point accuracy. Inputs are borrowed, output has n slots and
must not overlap inputs. Failure leaves output unchanged. O(n) temporary
storage, O(iterations*(n²+128n)) time. No relaxed-sum constraint or kernel loader.
