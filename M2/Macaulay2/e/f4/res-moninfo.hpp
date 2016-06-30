// Copyright 2016  Michael E. Stillman

#ifndef _res_moninfo_hpp_
#define _res_moninfo_hpp_

struct MonomialOrdering;
#include "../skew.hpp"

#include "res-monomial-types.hpp"
#include "res-varpower-monomial.hpp"

#include <vector>
#include <memory>

class ResMonoid
{
  int nvars;
  int nslots;
  std::unique_ptr<res_monomial_word[]> hashfcn; // array 0..nvars-1 of hash values for each variable
  res_monomial_word mask;
  M2_arrayint mVarDegrees; // array 0..nvars-1 of primary (heft) degrees for each variable.
  
  int firstvar; // = 2, if no weight vector, otherwise 2 + nweights
  int nweights; // number of weight vector values placed.  These should all be positve values?

  // flattened array 0..nweights of array 0..nvars-1 of longs
  std::vector<int> weight_vectors;
  ////  M2_arrayint weight_vectors; 

  // monomial format: [hashvalue, component, pack1, pack2, ..., packr]
  // other possible:
  //    [hashvalue, len, component, v1, v2, ..., vr] each vi is potentially packed too.
  //    [hashvalue, component, wt1, ..., wtr, pack1, ..., packr]

  mutable unsigned long ncalls_compare;
  mutable unsigned long ncalls_mult;
  mutable unsigned long ncalls_get_component;
  mutable unsigned long ncalls_from_exponent_vector;
  mutable unsigned long ncalls_to_exponent_vector;
  mutable unsigned long ncalls_to_varpower;
  mutable unsigned long ncalls_from_varpower;
  mutable unsigned long ncalls_is_equal;
  mutable unsigned long ncalls_is_equal_true;
  mutable unsigned long ncalls_divide;
  mutable unsigned long ncalls_weight;
  mutable unsigned long ncalls_unneccesary;
  mutable unsigned long ncalls_quotient_as_vp;
public:
  typedef res_packed_monomial monomial;
  typedef res_const_packed_monomial const_monomial;
  typedef monomial value;

  ResMonoid(int nvars,
            M2_arrayint var_degrees,
            const std::vector<int>& weightvecs,
            const MonomialOrdering *mo);

  ~ResMonoid();

  int n_vars() const { return nvars; }

  int max_monomial_size() const { return nslots; }

  int monomial_size(res_const_packed_monomial m) const { return nslots; }

  void show() const;

  res_monomial_word hash_value(res_const_packed_monomial m) const { return *m; }
  // This hash value is an ADDITIVE hash (trick due to A. Steel)

  void copy(res_const_packed_monomial src, res_packed_monomial target) const {
    for (int i=0; i<nslots; i++)
      *target++ = *src++;
  }

  long last_exponent(res_const_packed_monomial m) const {
    return m[nslots-1];
  }

  void set_component(long component, res_packed_monomial m) const { m[1] = component; }

  long get_component(res_const_packed_monomial m) const { ncalls_get_component++; return m[1]; }

  bool from_exponent_vector(res_const_ntuple_monomial e, component_index comp, res_packed_monomial result) const {
    // Pack the vector e[0]..e[nvars-1],comp.  Create the hash value at the same time.
    ncalls_from_exponent_vector++;
    result[0] = 0;
    result[1] = comp;

    for (int i=0; i<nvars; i++)
      {
        result[firstvar+i] = e[i];
        if (e[i] > 0)
          result[0] += hashfcn[i] * e[i];
      }

    const int *wt = weight_vectors.data();
    for (int j=0; j<nweights; j++, wt += nvars)
      {
        res_monomial_word val = 0;
        for (int i=0; i<nvars; i++)
          {
            long a = e[i];
            if (a > 0)
              val += a * wt[i];
          }
        result[2+j] = val;
      }
    return true;
  }

  int skew_vars(const SkewMultiplication* skew, res_const_packed_monomial m, int* skewvars) const {
    return skew->skew_vars(m + 2 + nweights, skewvars);
  }

  int skew_mult_sign(const SkewMultiplication* skew, res_const_packed_monomial m, res_const_packed_monomial n) const {
    return skew->mult_sign(m + 2 + nweights, n + 2 + nweights);
  }
  
  bool one(component_index comp, res_packed_monomial result) const {
    // Pack the vector (0,...,0,comp) with nvars zeroes.
    // Hash value = 0. ??? Should the hash-function take component into account ???
    result[0] = 0;
    result[1] = comp;
    for (int i=2; i<nslots; i++)
      result[i] = 0;
    return true;
  }

  bool to_exponent_vector(res_const_packed_monomial m, res_ntuple_monomial result, component_index &result_comp) const {
    // Unpack the monomial m.
    ncalls_to_exponent_vector++;
    result_comp = m[1];
    m += 2 + nweights;
    for (int i=0; i<nvars; i++)
      *result++ = *m++;
    return true;
  }

  bool to_intstar_vector(res_const_packed_monomial m, int * result, int &result_comp) const {
    // Unpack the monomial m into result, which should already be allocated 0..nvars-1
    // this is to connect with older 'int *' monomials.
    ncalls_to_exponent_vector++;
    result_comp = static_cast<int>(m[1]);
    m += 2;
    for (int i=0; i<nvars; i++)
      *result++ = static_cast<int>(*m++);
    return true;
  }

  void to_varpower_monomial(res_const_packed_monomial m, res_varpower_monomial result) const {
    // 'result' must have enough space allocated
    ncalls_to_varpower++;
    res_varpower_word *t = result+1;
    res_const_packed_monomial m1 = m + nslots;
    int len = 0;
    for (int i=nvars-1; i>=0; i--)
      {
        if (*--m1 > 0)
          {
            *t++ = i;
            *t++ = *m1;
            len++;
          }
      }
    *result = len;
  }

  void from_varpower_monomial(res_const_varpower_monomial m, long comp, res_packed_monomial result) const {
    // 'result' must have enough space allocated
    ncalls_from_varpower++;
    result[0] = 0;
    result[1] = comp;
    for (int i=2; i<nslots; i++)
      {
        result[i] = 0;
      }
    for (index_res_varpower_monomial j = m; j.valid(); ++j)
      {
        res_varpower_word v = j.var();
        res_varpower_word e = j.exponent();
        result[firstvar + v] = e;
        if (e == 1)
          result[0] += hashfcn[v];
        else
          result[0] += e * hashfcn[v];
      }

    const int *wt = weight_vectors.data();
    for (int j=0; j<nweights; j++, wt += nvars)
      {
        long val = 0;
        for (index_res_varpower_monomial i = m; i.valid(); ++i)
          {
            res_varpower_word v = i.var();
            res_varpower_word e = i.exponent();
            long w = wt[v];
            if (e == 1)
              val += w;
            else
              val += w * e;
            result[2+j] = val;
          }
      }
  }

  bool is_equal(res_const_packed_monomial m, res_const_packed_monomial n) const {
    ncalls_is_equal++;
    for (int j=nslots; j>0; --j)
      if (*m++ != *n++) return false;
    ncalls_is_equal_true++;
    return true;
  }

  bool monomial_part_is_equal(res_const_packed_monomial m, res_const_packed_monomial n) const {
    ncalls_is_equal++;
    if (*m++ != *n++) return false;
    m++; n++;
    for (int j=nslots-2; j>0; --j)
      if (*m++ != *n++) return false;
    ncalls_is_equal_true++;
    return true;
  }

  bool check_monomial(res_const_packed_monomial m) const {
    // Determine if m represents a well-formed monomial.
    m++;
    for (int j=nslots-1; j>0; --j)
      if (mask & (*m++)) return false;
    return true;
  }

  void unchecked_mult(res_const_packed_monomial m, res_const_packed_monomial n, res_packed_monomial result) const {
    ncalls_mult++;
    for (int j=nslots; j>0; --j) *result++ = *m++ + *n++;
  }

  void unchecked_divide(res_const_packed_monomial m, res_const_packed_monomial n, res_packed_monomial result) const {
    ncalls_divide++;
    for (int j=nslots; j>0; --j) *result++ = *m++ - *n++;
  }

  bool divide(res_const_packed_monomial m, res_const_packed_monomial n, res_packed_monomial result) const {
    ncalls_divide++;
    // First, divide monomials
    // Then, if the division is OK, set the component, hash value and rest of the monomial
    if (m[1] != n[1]) // components are not equal
      return false;
    res_const_packed_monomial m1 = m+nslots;
    res_const_packed_monomial n1 = n+nslots;
    res_packed_monomial result1 = result+nslots;
    for (int i=nslots-2; i>0; i--) {
      res_varpower_word cmp = *--m1 - *--n1;
      if (cmp < 0) return false;
      *--result1 = cmp;
    }
    result[1] = 0; // the component of a division is in the ring (comp 0).
    result[0] = m[0] - n[0]; // subtract hash codes
    return true;
  }

  bool mult(res_const_packed_monomial m, res_const_packed_monomial n, res_packed_monomial result) const {
    unchecked_mult(m,n,result);
    return check_monomial(result);
  }

  res_monomial_word monomial_weight(res_const_packed_monomial m, const M2_arrayint wts) const;

  void show(res_const_packed_monomial m) const;

  void showAlpha(res_const_packed_monomial m) const;

  int compare_grevlex(res_const_packed_monomial m, res_const_packed_monomial n) const {
    ncalls_compare++;
    res_const_packed_monomial m1 = m+nslots;
    res_const_packed_monomial n1 = n+nslots;
    for (int i=nslots-2; i>0; i--) {
      res_varpower_word cmp = *--m1 - *--n1;
      if (cmp < 0) return -1;
      if (cmp > 0) return 1;
    }
    res_monomial_word cmp = m[1]-n[1];
    if (cmp < 0) return 1;
    if (cmp > 0) return -1;
    return 0;
  }

  int compare_schreyer(res_const_packed_monomial m, res_const_packed_monomial n,
                       res_const_packed_monomial m0, res_const_packed_monomial n0,
                       long tie1, long tie2) const {
    ncalls_compare++;
    #if 0
    printf("compare_schreyer: ");
    printf("  m=");
    showAlpha(m);
    printf("  n=");    
    showAlpha(n);
    printf("  m0=");    
    showAlpha(m0);
    printf("  n0=");    
    showAlpha(n0);
    printf("  tiebreakers: %ld %ld\n", tie1, tie2);
    #endif
    res_const_packed_monomial m1 = m+nslots;
    res_const_packed_monomial n1 = n+nslots;
    res_const_packed_monomial m2 = m0+nslots;
    res_const_packed_monomial n2 = n0+nslots;
    for (int i=nslots-2; i>0; i--) {
      res_varpower_word cmp = *--m1 - *--n1 + *--m2 - *--n2;
      if (cmp < 0) return -1;
      if (cmp > 0) return 1;
    }
    res_monomial_word cmp = tie1-tie2;
    if (cmp < 0) return 1;
    if (cmp > 0) return -1;
    return 0;
  }

  int compare_lex(res_const_packed_monomial m, res_const_packed_monomial n) const {
    ncalls_compare++;
    res_const_packed_monomial m1 = m+2;
    res_const_packed_monomial n1 = n+2;
    for (int i=nslots-2; i>0; i--) {
      res_varpower_word cmp = *m1++ - *n1++;
      if (cmp > 0) return -1;
      if (cmp < 0) return 1;
    }
    res_monomial_word cmp = m[1]-n[1];
    if (cmp < 0) return 1;
    if (cmp > 0) return -1;
    return 0;
  }

  int compare_weightvector(res_const_packed_monomial m, res_const_packed_monomial n) const {
    ncalls_compare++;
    res_const_packed_monomial m1 = m+2;
    res_const_packed_monomial n1 = n+2;
    for (int i=0; i<nweights; i++) {
      res_varpower_word cmp = *m1++ - *n1++;
      if (cmp > 0) return -1;
      if (cmp < 0) return 1;
    }
    m1 = m+nslots;
    n1 = n+nslots;
    for (int i=nvars-1; i>0; i--) {
      res_varpower_word cmp = *--m1 - *--n1;
      if (cmp < 0) return -1;
      if (cmp > 0) return 1;
    }
    res_monomial_word cmp = m[1]-n[1];
    if (cmp < 0) return 1;
    if (cmp > 0) return -1;
    return 0;
  }

  int (ResMonoid::*compare)(res_const_packed_monomial m, res_const_packed_monomial n) const;

  void variable_as_vp(int v,
                      res_varpower_monomial result) const
  {
    result[0] = 1;
    result[1] = v;
    result[2] = 1;
  }

  int degree_of_vp(res_const_varpower_monomial a) const
  {
    return static_cast<int>(res_varpower_monomials::weight(a, mVarDegrees));
  }
  
  void quotient_as_vp(res_const_packed_monomial a,
                      res_const_packed_monomial b,
                      res_varpower_monomial result) const
  {
    // sets result
    ncalls_quotient_as_vp++;
    a += firstvar;
    b += firstvar;
    int len = 0;
    res_varpower_word *r = result+1;
    for (int i=nvars-1; i>=0; --i)
      {
        res_varpower_word c = a[i] - b[i];
        if (c > 0)
          {
            *r++ = i;
            *r++ = c;
            len++;
          }
      }
    result[0] = len;
  }

};
#endif

// Local Variables:
// compile-command: "make -C $M2BUILDDIR/Macaulay2/e "
// indent-tabs-mode: nil
// End:
