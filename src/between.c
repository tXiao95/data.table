#include "data.table.h"

SEXP between(SEXP x, SEXP lower, SEXP upper, SEXP bounds) {

  R_len_t nx = length(x), nl = length(lower), nu = length(upper);
  if (!nx || !nl || !nu)
    return (allocVector(LGLSXP, 0));
  if (nl != 1 && nl != nx)
    error("length(lower) (%d) must be either 1 or length(x) (%d)", nl, nx);
  if (nu != 1 && nu != nx)
    error("length(upper) (%d) must be either 1 or length(x) (%d)", nu, nx);
  if (!isLogical(bounds) || LOGICAL(bounds)[0] == NA_LOGICAL)
    error("incbounds must be logical TRUE/FALSE.");  // # nocov

  int nprotect = 0;
  bool integer=true;
  if (isReal(x) || isReal(lower) || isReal(upper)) {
    if (inherits(x,"integer64") || inherits(lower,"integer64") || inherits(upper,"integer64")) {
      error("Internal error: one or more of x, lower and upper is type integer64 but this should have been caught by between() at R level.");  // # nocov
    }
    integer=false;
    lower = PROTECT(coerceVector(lower, REALSXP));  // these coerces will convert NA appropriately
    upper = PROTECT(coerceVector(upper, REALSXP));
    x     = PROTECT(coerceVector(x, REALSXP));
    nprotect += 3;
  }
  // TODO: sweep through lower and upper ensuring lower<=upper (inc bounds) and no lower>upper or lower==INT_MAX

  const bool recycleLow = LENGTH(lower)==1;
  const bool recycleUpp = LENGTH(upper)==1;
  const bool open = !LOGICAL(bounds)[0];
  SEXP ans = PROTECT(allocVector(LGLSXP, nx)); nprotect++;
  int *restrict ansp = LOGICAL(ans);
  if (integer) {
    const int *lp = INTEGER(lower);
    const int *up = INTEGER(upper);
    const int *xp = INTEGER(x);
    if (recycleLow && recycleUpp) {
      const int l = lp[0] + open;  // +open so we can always use >= and <=.  NA_INTEGER+1 == -INT_MAX == INT_MIN+1 (so NA limit handled by this too)
      const int u = up[0]==NA_INTEGER ? INT_MAX : up[0] - open;
      #pragma omp parallel for num_threads(getDTthreads())
      for (int i=0; i<nx; i++) {
        int elem = xp[i];
        ansp[i] = elem==NA_INTEGER ? NA_LOGICAL : (l<=elem && elem<=u);
      }
    }
    else {
      const int lowMask = recycleLow ? 0 : INT_MAX;
      const int uppMask = recycleUpp ? 0 : INT_MAX;
      #pragma omp parallel for num_threads(getDTthreads())
      for (int i=0; i<nx; i++) {
        int elem = xp[i];
        int l = lp[i&lowMask] +open;
        int u = up[i&uppMask];
        u = (u==NA_INTEGER) ? INT_MAX : u-open;
        ansp[i] = elem==NA_INTEGER ? NA_LOGICAL : (l<=elem && elem<=u);
      }
    }
  } else {
    // type real
    const double *lp = REAL(lower);
    const double *up = REAL(upper);
    const double *xp = REAL(x);
    if (recycleLow && recycleUpp) {
      const double l = isnan(lp[0]) ? -INFINITY : lp[0];
      const double u = isnan(up[0]) ?  INFINITY : up[0];
      if (open) {
        #pragma omp parallel for num_threads(getDTthreads())
        for (int i=0; i<nx; i++) {
          double elem = xp[i];
          ansp[i] = isnan(elem) ? NA_LOGICAL : (l<elem && elem<u);
        }
      } else {
        #pragma omp parallel for num_threads(getDTthreads())
        for (int i=0; i<nx; i++) {
          double elem = xp[i];
          ansp[i] = isnan(elem) ? NA_LOGICAL : (l<=elem && elem<=u);
        }
      }
    }
    else {
      const int lowMask = recycleLow ? 0 : INT_MAX;
      const int uppMask = recycleUpp ? 0 : INT_MAX;
      #pragma omp parallel for num_threads(getDTthreads())
      for (int i=0; i<nx; i++) {
        double elem = xp[i];
        double l = lp[i&lowMask];
        double u = up[i&uppMask];
        if (isnan(l)) l=-INFINITY;
        if (isnan(u)) u= INFINITY;
        ansp[i] = isnan(elem) ? NA_LOGICAL : (open ? l<elem && elem<u : l<=elem && elem<=u);
      }
    }
  }
  UNPROTECT(nprotect);
  return(ans);
}

