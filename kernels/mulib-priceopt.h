/** @file mulib-priceopt.h
 * @brief Self-contained continuous/discrete demand-based price optimization,
 * scenario-risk objectives, and deterministic UCB bandit selection.
 * Define MULIB_IMPL_PRICEOPT in exactly one translation unit; link -lm.
 * This is model-based optimization; data alone cannot identify demand causally.
 */
#ifndef MULIB_PRICEOPT_H
#define MULIB_PRICEOPT_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_priceopt_demand_kind {
  MULIB_PRICEOPT_LINEAR,          /* max(0,scale-shape*price) */
  MULIB_PRICEOPT_ISOELASTIC,     /* scale * (price/reference)^(-shape) */
  MULIB_PRICEOPT_LOGISTIC        /* scale/(1+exp((price-reference)/shape)) */
} mulib_priceopt_demand_kind_t;
typedef struct mulib_priceopt_demand_model {
  mulib_priceopt_demand_kind_t kind;
  double scale, shape, reference;
} mulib_priceopt_demand_model_t;
typedef enum mulib_priceopt_objective {
  MULIB_PRICEOPT_PROFIT, MULIB_PRICEOPT_REVENUE
} mulib_priceopt_objective_t;
typedef struct mulib_priceopt_constraints {
  double min_price, max_price, unit_cost, min_unit_margin;
  double previous_price, max_relative_change; /* previous=0 or max_change=0 disables */
  double tick; /* 0=continuous; >0 means exact tick-grid search, <=1M candidates */
} mulib_priceopt_constraints_t;
typedef struct mulib_priceopt_result {
  double price, score, expected_demand;
  size_t evaluations;
} mulib_priceopt_result_t;
/** Returns NAN for invalid model/price. Negative/overflow demand is invalid.
 * Isoelastic price must be >0; logistic uses overflow-stable sigmoid. */
double mulib_priceopt_demand(const mulib_priceopt_demand_model_t *m, double price);
/** Deterministic 129-knot scan + local golden search for continuous prices;
 * tick mode exhaustively scans all feasible ticks. Scores are model estimates. */
int mulib_priceopt_optimize(const mulib_priceopt_demand_model_t *model,
    const mulib_priceopt_constraints_t *constraints,
    mulib_priceopt_objective_t objective, mulib_priceopt_result_t *out);
/** Multi-scenario risk score = E[objective] - risk_aversion*stddev(objective).
 * Nonnegative scenario probabilities (not required to sum to one) and risk.
 * Demand is probability-weighted; score includes the risk penalty.
 * Zero-weight scenarios are validated but do not constrain the price domain. */
int mulib_priceopt_scenarios(const mulib_priceopt_demand_model_t *models,
    const double *weights, size_t n, double risk_aversion,
    const mulib_priceopt_constraints_t *constraints,
    mulib_priceopt_objective_t objective, mulib_priceopt_result_t *out);
/** Exact argmax among feasible candidates and forecasted demand. All rows are
 * validated; infeasible rows are skipped. Equal scores favor lower prices. */
int mulib_priceopt_candidates(const double *prices, const double *predicted_units,
    size_t n, const mulib_priceopt_constraints_t *constraints,
    mulib_priceopt_objective_t objective, mulib_priceopt_result_t *out);
/** UCB1 exploration: choose untried arm first, else mean_reward +
 * exploration*sqrt(log(total_trials)/trials). Deterministic ties. */
int mulib_priceopt_ucb(const size_t *trials, const double *mean_reward,
    size_t arms, double exploration, size_t *selected);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PRICEOPT) && !defined(MULIB_PRICEOPT_IMPLEMENTED)
#define MULIB_PRICEOPT_IMPLEMENTED
#include <math.h>
#include <float.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
static int mulib_priceopt_valid_model_(const mulib_priceopt_demand_model_t *m) {
  if(!m||!isfinite(m->scale)||m->scale<0||!isfinite(m->shape)||!isfinite(m->reference))return 0;
  switch(m->kind) {
    case MULIB_PRICEOPT_LINEAR:return m->shape>=0;
    case MULIB_PRICEOPT_ISOELASTIC:return m->shape>=0&&m->reference>0;
    case MULIB_PRICEOPT_LOGISTIC:return m->shape>0;
    default:return 0;
  }
}
double mulib_priceopt_demand(const mulib_priceopt_demand_model_t *m,double p) {
  double v;long double z;
  if(!mulib_priceopt_valid_model_(m)||!isfinite(p)||p<0)return NAN;
  switch(m->kind) {
    case MULIB_PRICEOPT_LINEAR:v=(double)fmaxl(0.0L,(long double)m->scale-(long double)m->shape*p);break;
    case MULIB_PRICEOPT_ISOELASTIC:
      if(p<=0)return NAN;
      if(m->scale==0)return 0;
      v=(double)expl(logl(m->scale)-(long double)m->shape*(logl(p)-logl(m->reference)));break;
    case MULIB_PRICEOPT_LOGISTIC:
      z=((long double)p-m->reference)/m->shape;
      v=(double)(z>=0?(long double)m->scale*expl(-z)/(1+expl(-z)):m->scale/(1+expl(z)));break;
    default:return NAN;
  }
  return isfinite(v)&&v>=0?v:NAN;
}
static int mulib_priceopt_domain_(const mulib_priceopt_constraints_t *c,double *lo,double *hi) {
  double a,b;
  if(!c||!isfinite(c->min_price)||!isfinite(c->max_price)||
     !isfinite(c->unit_cost)||c->unit_cost<0||!isfinite(c->min_unit_margin)||c->min_unit_margin<0||
     !isfinite(c->previous_price)||c->previous_price<0||
     !isfinite(c->max_relative_change)||c->max_relative_change<0||c->max_relative_change>1||
     !isfinite(c->tick)||c->tick<0||c->min_price<0||c->max_price<c->min_price)return 0;
  a=fmax(c->min_price,c->unit_cost+c->min_unit_margin);b=c->max_price;
  if(c->previous_price>0&&c->max_relative_change>0) {
    a=fmax(a,c->previous_price*(1-c->max_relative_change));
    b=fmin(b,c->previous_price*(1+c->max_relative_change));
  }
  if(!isfinite(a)||!isfinite(b)||a>b)return 0;
  *lo=a;*hi=b;return 1;
}
static int mulib_priceopt_score_(const mulib_priceopt_demand_model_t *m,
    const double *weights,size_t n,double risk,const mulib_priceopt_constraints_t *c,
    mulib_priceopt_objective_t objective,double p,double *score,double *demand) {
  long double sw=0,mean=0,m2=0,units=0;size_t i;
  if(objective!=MULIB_PRICEOPT_PROFIT&&objective!=MULIB_PRICEOPT_REVENUE)return 0;
  for(i=0;i<n;++i){double d;
    long double weight=weights?weights[i]:1, v,delta,next;
    if(!isfinite(weight)||weight<0)return 0;
    if(weight==0)continue;
    d=mulib_priceopt_demand(&m[i],p);
    if(!isfinite(d))return 0;
    v=(objective==MULIB_PRICEOPT_PROFIT?(long double)p-c->unit_cost:p)*d;
    next=sw+weight;delta=v-mean;
    mean+=delta*(weight/next);
    m2+=weight*delta*(v-mean);
    units+=((long double)d-units)*(weight/next);sw=next;
  }
  if(sw<=0)return 0;
  {long double variance=m2/sw;
    if(variance<0)variance=0;
    *score=(double)(mean-risk*sqrtl(variance));*demand=(double)units;
  }
  return isfinite(*score)&&isfinite(*demand);
}
/* Snap only roundoff-sized tick-index errors, not genuinely off-grid prices.
 * Beyond 2^52, adjacent tick indices cannot be resolved reliably in double. */
static double mulib_priceopt_tick_index_(double price,double tick) {
  double index=price/tick,nearest,tolerance;
  if(!isfinite(index)||index>4503599627370495.0)return NAN;
  nearest=round(index);
  tolerance=fmin(1e-8,4*DBL_EPSILON*fmax(1.0,index));
  return fabs(index-nearest)<=tolerance?nearest:index;
}
static int mulib_priceopt_search_(const mulib_priceopt_demand_model_t *m,
    const double *weights,size_t n,double risk,const mulib_priceopt_constraints_t *c,
    mulib_priceopt_objective_t objective,mulib_priceopt_result_t *out) {
  double lo,hi,best=0,bestscore=-INFINITY,bestd=0,price,score,d,step;
  size_t evaluations=0,i,count=128;
  if(!out||!m||!n||!mulib_priceopt_domain_(c,&lo,&hi)||!isfinite(risk)||risk<0)return 0;
  for(i=0;i<n;++i) if(!mulib_priceopt_valid_model_(&m[i]) ||
       (weights&&(!isfinite(weights[i])||weights[i]<0)))return 0;
  if(c->tick>0) {
    double first=ceil(mulib_priceopt_tick_index_(lo,c->tick));
    double last=floor(mulib_priceopt_tick_index_(hi,c->tick));
    if(!isfinite(first)||!isfinite(last)||first>last||last-first>999999)return 0;
    count=(size_t)(last-first);
    for(i=0;i<=count;++i) {
      price=(first+(double)i)*c->tick;
      /* Boundary snapping never weakens the hard price/margin/movement band. */
      price=fmax(lo,fmin(hi,price));
      if(!mulib_priceopt_score_(m,weights,n,risk,c,objective,price,&score,&d))return 0;
      ++evaluations;
      if(score>bestscore||(score==bestscore&&price<best)) {bestscore=score;best=price;bestd=d;}
    }
  } else {
    size_t best_i=0;
    for(i=0;i<=count;++i) {
      price=(double)((long double)lo+((long double)hi-lo)*i/count);
      if(!mulib_priceopt_score_(m,weights,n,risk,c,objective,price,&score,&d))return 0;
      ++evaluations;
      if(score>bestscore||(score==bestscore&&price<best)) {bestscore=score;best=price;bestd=d;best_i=i;}
    }
    if(hi>lo){
      double a=(double)((long double)lo+((long double)hi-lo)*(best_i?best_i-1:0)/count);
      double b=(double)((long double)lo+((long double)hi-lo)*(best_i<count?best_i+1:count)/count);
      const double ratio=0.6180339887498948482;
      for(i=0;i<72&&b-a>1e-12*fmax(1.0,b);++i){
        double x=b-ratio*(b-a),y=a+ratio*(b-a),sx,sy,dx,dy;
        if(!mulib_priceopt_score_(m,weights,n,risk,c,objective,x,&sx,&dx)||
           !mulib_priceopt_score_(m,weights,n,risk,c,objective,y,&sy,&dy))return 0;
        evaluations+=2;
        if(sx>bestscore){bestscore=sx;best=x;bestd=dx;}
        if(sy>bestscore){bestscore=sy;best=y;bestd=dy;}
        if(sx>sy)b=y;else a=x;
      }
    }
  }
  step=best; /* publish only on success */
  if(!isfinite(step)||!isfinite(bestscore)||!isfinite(bestd))return 0;
  {mulib_priceopt_result_t answer={step,bestscore,bestd,evaluations};*out=answer;}
  return 1;
}
int mulib_priceopt_optimize(const mulib_priceopt_demand_model_t *m,
    const mulib_priceopt_constraints_t *c,mulib_priceopt_objective_t objective,
    mulib_priceopt_result_t *out) {
  return mulib_priceopt_search_(m,NULL,1,0,c,objective,out);
}
int mulib_priceopt_scenarios(const mulib_priceopt_demand_model_t *m,const double *w,
    size_t n,double risk,const mulib_priceopt_constraints_t *c,
    mulib_priceopt_objective_t objective,mulib_priceopt_result_t *out) {
  if(!w || !n)return 0;
  return mulib_priceopt_search_(m,w,n,risk,c,objective,out);
}
int mulib_priceopt_candidates(const double *p,const double *q,size_t n,
    const mulib_priceopt_constraints_t *c,mulib_priceopt_objective_t obj,
    mulib_priceopt_result_t *out) {
  double lo,hi,best=0,score=-INFINITY,units=0;size_t i,used=0;
  if(!p||!q||!n||!out||!mulib_priceopt_domain_(c,&lo,&hi)||
     (obj!=MULIB_PRICEOPT_PROFIT&&obj!=MULIB_PRICEOPT_REVENUE))return 0;
  for(i=0;i<n;++i){double s;
    if(!isfinite(p[i])||p[i]<0||!isfinite(q[i])||q[i]<0)return 0;
    if(p[i]<lo||p[i]>hi)continue;
    if(c->tick>0) {
      double index=mulib_priceopt_tick_index_(p[i],c->tick);
      if(!isfinite(index)||index!=round(index))continue;
    }
    s=(obj==MULIB_PRICEOPT_PROFIT?p[i]-c->unit_cost:p[i])*q[i];
    if(!isfinite(s))return 0;
    if(!used||s>score||(s==score&&p[i]<best)){best=p[i];score=s;units=q[i];}
    ++used;
  }
  if(!used)return 0;
  {mulib_priceopt_result_t r={best,score,units,used};*out=r;}
  return 1;
}
int mulib_priceopt_ucb(const size_t *trials,const double *mean,size_t n,
    double exploration,size_t *out) {
  size_t i,total=0,best=0;double score=-INFINITY;
  if(!trials||!mean||!out||!n||!isfinite(exploration)||exploration<0)return 0;
  for(i=0;i<n;++i){if(!isfinite(mean[i])||trials[i]>SIZE_MAX-total)return 0;total+=trials[i];}
  for(i=0;i<n;++i)if(trials[i]==0){*out=i;return 1;}
  for(i=0;i<n;++i){double u=mean[i]+exploration*sqrt(log((double)total)/(double)trials[i]);
    if(!isfinite(u))return 0;
    if(i==0||u>score){score=u;best=i;}
  }
  *out=best;return 1;
}
#ifdef __cplusplus
}
#endif
#endif
