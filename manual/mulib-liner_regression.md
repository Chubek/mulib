# Simple linear regression

Define `MULIB_IMPL_LINER_REGRESSION`; link `-lm`.
[Example](../examples/modules/liner_regression.c). Filename spelling retained.

Fits y=intercept+slope*x by ordinary least squares using scaled centered sums.
The result is a copied plain struct with slope, intercept and r_squared.
Input arrays are borrowed. Need at least two finite paired observations and
nonconstant x; invalid pointers, undefined fit or coefficients outside double
range return 0 without output mutation. Constant y yields r_squared=1.
Predict evaluates using fma; NULL model/nonfinite x gives NAN, overflow may
give INFINITY. No allocation, O(n) fit time. Near-constant data remains limited
by floating-point precision. No multiple predictors, regularization, confidence
intervals or statistical significance tests are implied.
