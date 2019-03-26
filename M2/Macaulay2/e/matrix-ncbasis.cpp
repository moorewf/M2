// Copyright 2018  Michael E. Stillman

#include "M2FreeAlgebra.hpp"
#include "ntuple.hpp"
#include "matrix.hpp"
#include "matrix-con.hpp"

#if 0
class NCBasis
{
private:
  const M2FreeAlgebra *mRing;
  const NCMonoid& mMonoid;
  MatrixConstructor mMatrixConstructor; // result is collected here
  enum { KB_SINGLE, KB_MULTI } mComputationType;
  const Matrix *mZeroInitialTerms; //TODO: currently, not considered!!
  std::vector<int> mHeftVector; // length: degreeRank := degreeMonoid().numVars()
  std::vector<int> mVariables; // the variable indices being used.
  std::vector<int> mVariableHefts; // matches mVariables array.
  
  // Construction of monomials: these are used in the recursion.
  int mLimit; // if >= 0, then number of monomials to collect.
  int mComponent; // component of monomials currently being constructed
  std::vector<int> mMonomial; // being constructed.
  int mCurrentIndex; // into mMonomial... this is where we place next variable.
  int mCurrentHeftValue; // heft value so far, of monomial being constructed.
  const int* mCurrentMultiDegree; // only used in multi-grading case?

  int mLoHeft; // 
  int mHiHeft; //
  const int* mMultiDegree; // target multi-degree in the multi-graded case.

  // do we need these?
  const int* mLoDegree; // either nullptr (if -infinity), or an array of length = degreeRank
  const int* mHiDegree; // an array of length = degreeRank
private:
  void insert();
  void basis0_singly_graded();
  void basis0_multi_graded();

  NCBasis(const Matrix *zeroInitialTerms,
          const int *lo_degree,
          const int *hi_degree,
          std::vector<int> heftVector,
          std::vector<int> variables,
          bool do_truncation,
          int limit);

  ~NCBasis() {}

  void compute();

  Matrix *value() { return mMatrixConstructor.to_matrix()); }
public:
  friend Matrix *ncBasis(const Matrix *leadTerms,
                         M2_arrayint lo_degree,
                         M2_arrayint hi_degree,
                         M2_arrayint heft,
                         M2_arrayint vars,
                         bool do_truncation,
                         int limit);
};

NCBasis::NCBasis(
                 const M2FreeAlgebra* P,
                 const Matrix *zeroInitialTerms,
                 const int *lo_degree,
                 const int *hi_degree,
                 std::vector<int> heftVector,
                 std::vector<int> variables,
                 bool do_truncation,
                 int limit)
  : mRing(P),
    mZeroInitialTerms(zeroInitialTerms),
    mHeftVector(heftVector),
    mVariables(variables),
    mDoTruncation(do_truncation),
    mLimit(limit),
    mLoDegree(lo_degree),
    mHiDegree(hi_degree)
{
  assert(P == zeroInitialTerms->get_ring());
  assert(hi_degree != nullptr);
  int degree_rank = P->degree_monoid()->n_vars();
  mComputationType = (degree_rank == 1 ? KB_SINGLE : KB_MULTI);

  // Set heft values for the variables.  
  for (auto v : mVariables)
    {
      int heft = ntuple::weight(degree_rank, P->degreeExponents(v), XXX);
      mVariableHefts.push_back(heft);
    }

  
}

NCBasis::NCBasis(const Matrix *leadTerms,
               const int *lo_degree0,
               const int *hi_degree0,
               M2_arrayint heft_vector0,
               M2_arrayint vars0,
               bool do_truncation0,
               int limit0)
    : bottom_matrix(bottom),
      heft_vector(heft_vector0),
      vars(vars0),
      do_truncation(do_truncation0),
      mLimit(limit0),
      lo_degree(lo_degree0),
      hi_degree(hi_degree0)
{
  P = bottom->get_ring()->cast_to_PolynomialRing();
  M = P->getMonoid();
  D = P->get_degree_ring()->getMonoid();

  if (lo_degree == 0 && hi_degree == 0)
    {
      computation_type = KB_FULL;
    }
  else if (heft_vector->len == 1)
    {
      computation_type = KB_SINGLE;
    }
  else
    {
      computation_type = KB_MULTI;
    }

  // Compute the (positive) weights of each of the variables in 'vars'.

  var_wts = newarray_atomic(int, vars->len);
  var_degs = newarray_atomic(int, vars->len * heft_vector->len);
  int *exp =
      newarray_atomic(int, D->n_vars());  // used to hold exponent vectors
  int next = 0;
  for (int i = 0; i < vars->len; i++, next += heft_vector->len)
    {
      int v = vars->array[i];
      D->to_expvector(M->degree_of_var(v), exp);
      var_wts[i] = ntuple::weight(heft_vector->len, exp, heft_vector);
      ntuple::copy(heft_vector->len, exp, var_degs + next);
    }
  deletearray(exp);

  // Set the recursion variables
  kb_exp = newarray_atomic_clear(int, P->n_vars());
  kb_exp_weight = 0;

  if (lo_degree != NULL)
    kb_target_lo_weight =
        ntuple::weight(heft_vector->len, lo_degree, heft_vector);
  if (hi_degree != NULL)
    kb_target_hi_weight =
        ntuple::weight(heft_vector->len, hi_degree, heft_vector);

  if (lo_degree && hi_degree && heft_vector->len == 1 &&
      heft_vector->array[0] < 0)
    {
      int t = kb_target_lo_weight;
      kb_target_lo_weight = kb_target_hi_weight;
      kb_target_hi_weight = t;
    }

  kb_mon = M->make_one();

  mat = MatrixConstructor(bottom->rows(), 0);
  kb_exp_multidegree = D->make_one();

  if (heft_vector->len > 1 && lo_degree != NULL)
    {
      kb_target_multidegree = D->make_one();
      ntuple::copy(heft_vector->len, lo_degree, kb_target_multidegree);
    }
  else
    {
      kb_target_multidegree = 0;
    }
}

void NCBasis::insert()
{
  // We have a new basis element

  M->from_expvector(kb_exp, kb_mon);
  ring_elem r = mRing->make_flat_term(mRing->getCoefficients()->one(), kb_mon);
  vec v = P->make_vec(mComponent, r);
  mat.append(v);
  if (mLimit > 0) mLimit--;
}

void NCBasis::basis0_singly_graded()
{
  if (system_interrupted()) return;

  //TODO: check that the currently constructed monomial isn't a lead term in this ring (2-sided)
  //  or a lead monomial in the module (1-sided here!?)
  // for now: we don't have any such terms, so we always accept the monomial.

  if (hi_degree && kb_exp_weight > kb_target_hi_weight)
    {
      if (do_truncation) insert();
      return;
    }

  if (lo_degree == 0 || kb_exp_weight >= kb_target_lo_weight) insert();

  if (hi_degree && kb_exp_weight == kb_target_hi_weight) return;

  for (int i = 0; i < mVariables.size(); i++)
    {
      int v = mVariables[i];

      kb_exp[v]++;
      kb_exp_weight += var_wts[i];

      basis0_singly_graded();

      kb_exp[v]--;
      kb_exp_weight -= var_wts[i];

      if (mLimit == 0) return;
    }
}

void NCBasis::basis0_multi_graded()
{
  if (system_interrupted()) return;

  //TODO: check that the currently constructed monomial isn't a lead term in this ring (2-sided)
  //  or a lead monomial in the module (1-sided here!?)
  // for now: we don't have any such terms, so we always accept the monomial.

  if (kb_exp_weight > kb_target_lo_weight) return;

  if (kb_exp_weight == kb_target_lo_weight)
    {
      if (EQ == ntuple::lex_compare(heft_vector->len,
                                    kb_target_multidegree,
                                    kb_exp_multidegree))
        insert();
      return;
    }

  for (int i = 0; i < mVariables.size(); i++)
    {
      int v = mVariables[i];

      kb_exp[v]++;
      kb_exp_weight += var_wts[i];
      ntuple::mult(heft_vector->len,
                   kb_exp_multidegree,
                   var_degs + (heft_vector->len * v),
                   kb_exp_multidegree);

      basis0_multi_graded();

      kb_exp[v]--;
      kb_exp_weight -= var_wts[i];
      ntuple::divide(heft_vector->len,
                     kb_exp_multidegree,
                     var_degs + (heft_vector->len * v),
                     kb_exp_multidegree);
      if (mLimit == 0) return;
    }
}

void NCBasis::compute()
// Only the lead monomials of the two matrices 'this' and 'bottom' are
// considered.  Thus, you must perform the required GB's elsewhere.
// Find a basis for (image this)/(image bottom) in degree d.
// If 'd' is NULL, first check that (image this)/(image bottom) has
// finite dimension, and if so, return a basis.
// If 'd' is not NULL, it is an element of the degree monoid.
{
  if (mLimit == 0) return;
  for (int i = 0; i < mLeadTerms->n_rows(); i++)
    {
      if (system_interrupted()) return;
      mComponent = i;

      // TODO: make the 2-sided ideal of zero lead monomials, and the 1-sided ideal of zero module monomials.
      //   over ZZ: don't include any monomial with non-unit as lead coefficient.

      // TODO: if 1 is 0 in this component, continue here (no monomials added).

      // note: mHiDegree should not be nullptr...

      D->to_expvector(mLeadTerms->rows()->degree(i),
                      mCurrentMultiDegreeExponents);
      mCurrentHeftValue = 
        ntuple::weight(mHeftVector->size(), mCurrentMultiDegreeExponents, mHeftVector);

      // Do the recursion
      switch (mComputationType)
        {
          case KB_SINGLE:
            basis0_singly_graded();
            break;
          case KB_MULTI:
            basis0_multi_graded();
            break;
        }
    }
}
#endif

Matrix* ncBasis(const Matrix *leadTerms,
                M2_arrayint lo_degree,
                M2_arrayint hi_degree,
                M2_arrayint heft,
                M2_arrayint vars,
                bool do_truncation,
                int limit)
{
  ERROR("not implemented yet");
  return nullptr;
#if 0
  const M2FreeAlgebra *P = leadTerms->get_ring()->cast_to_M2FreeAlgebra();

  // Now we check and set inputs to the algorithm.
  const PolynomialRing *D = P->get_degree_ring();
  const int *lo = lo_degree->len > 0 ? lo_degree->array : 0;
  const int *hi = hi_degree->len > 0 ? hi_degree->array : 0;

  if (heft->len > D->n_vars())
    {
      ERROR("expected heft vector of length <= %d", D->n_vars());
      return 0;
    }

  if (lo && heft->len != lo_degree->len)
    {
      ERROR("expected degrees of length %d", heft->len);
      return 0;
    }

  if (hi && heft->len != hi_degree->len)
    {
      ERROR("expected degrees of length %d", heft->len);
      return 0;
    }

  // If heft->len is > 1, and both lo and hi are non-null,
  // they need to be the same
  if (heft->len > 1 && lo && hi)
    for (int i = 0; i < heft->len; i++)
      if (lo_degree->array[i] != hi_degree->array[i])
        {
          ERROR("expected degree bounds to be equal");
          return 0;
        }

  NCBasis KB(bottom, lo, hi, heft, vars, do_truncation, limit);

  // If either a low degree, or high degree is given, then we require a positive
  // heft vector:
  if (lo || hi)
    for (int i = 0; i < vars->len; i++)
      if (KB.var_wts[i] <= 0)
        {
          ERROR(
              "basis: computation requires a heft form positive on the degrees "
              "of the variables");
          return 0;
        }

  // This next line will happen if both lo,hi degrees are given, and they are
  // different.  This can only be the singly generated case, and in that case
  // the degrees are in the wrong order, so return with 0 basis.
  if (lo != NULL && hi != NULL && lo[0] > hi[0]) return KB.value();

  KB.compute();
  if (system_interrupted()) return nullptr;
  return KB.value();
#endif
}


// Local Variables:
// compile-command: "make -C $M2BUILDDIR/Macaulay2/e matrix-kbasis.o "
// indent-tabs-mode: nil
// End: