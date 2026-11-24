/** @file mulib-conditional_probablity.h
 * @brief Conditional probability and Bayes' rule for scalar events.
 * Define MULIB_IMPL_CONDITIONAL_PROBABLITY. The historical filename is retained.
 */
#ifndef MULIB_CONDITIONAL_PROBABLITY_H
#define MULIB_CONDITIONAL_PROBABLITY_H
#ifdef __cplusplus
extern "C" {
#endif
/** Return joint/evidence, or NAN unless 0 <= joint <= evidence <= 1 and evidence > 0. */
double mulib_conditional_probability(double joint, double evidence);
/** Return likelihood*prior/evidence; NAN for inconsistent probabilities. */
double mulib_conditional_bayes(double likelihood, double prior, double evidence);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CONDITIONAL_PROBABLITY) && !defined(MULIB_CONDITIONAL_PROBABLITY_IMPLEMENTED)
#define MULIB_CONDITIONAL_PROBABLITY_IMPLEMENTED
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
double mulib_conditional_probability(double joint, double evidence)
{
  if (!isfinite(joint) || !isfinite(evidence) || joint < 0 || evidence <= 0
      || evidence > 1 || joint > evidence) return NAN;
  return joint / evidence;
}
double mulib_conditional_bayes(double likelihood, double prior, double evidence)
{
  if (!isfinite(likelihood) || !isfinite(prior) || likelihood < 0
      || likelihood > 1 || prior < 0 || prior > 1) return NAN;
  return mulib_conditional_probability(likelihood * prior, evidence);
}
#ifdef __cplusplus
}
#endif
#endif
