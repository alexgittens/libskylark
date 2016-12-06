#ifndef SKYLARK_RANDOM_MATRICES_HPP
#define SKYLARK_RANDOM_MATRICES_HPP

#include <boost/random/normal_distribution.hpp>
#include <boost/random/uniform_01.hpp>

#include "../utility/typer.hpp"
#include "../utility/types.hpp"

namespace skylark { namespace base {

/**
 * Generate random matrix using specificed distribution
 * (i.i.d samples).
 *
 * Implementation for local matrices.
 *
 * \param A Output matrix
 * \param m,n Number of rows and colunt
 * \param dist Distribution object
 * \param context Skylark context.
 */
template<typename T, template<typename> class DistributionType>
void RandomMatrix(El::Matrix<T> &A, El::Int m, El::Int n,
    DistributionType<T> &dist, context_t &context) {

    random_samples_array_t< DistributionType<T> > entries =
        context.allocate_random_samples_array(m * n, dist);

    A.Resize(m, n);
    T *data = A.Buffer();

#   ifdef SKYLARK_HAVE_OPENMP
#   pragma omp parallel for collapse(2)
#   endif
    for(size_t j = 0; j < n; j++)
        for(size_t i = 0; i < m; i++)
            data[j * m + i] = entries[j * m + i];
}

template<typename T, template<typename, typename> class DistributionType>
void RandomMatrix(El::Matrix<T> &A, El::Int m, El::Int n,
    DistributionType<T, T> &dist, context_t &context) {

    random_samples_array_t< DistributionType<T, T> > entries =
        context.allocate_random_samples_array(m * n, dist);

    A.Resize(m, n);
    T *data = A.Buffer();

#   ifdef SKYLARK_HAVE_OPENMP
#   pragma omp parallel for collapse(2)
#   endif
    for(size_t j = 0; j < n; j++)
        for(size_t i = 0; i < m; i++)
            data[j * m + i] = entries[j * m + i];
}

/**
 * Generate random matrix using specificed distribution
 * (i.i.d samples).
 *
 * Implementation for distributed matrices.
 *
 * \param A Output matrix
 * \param m,n Number of rows and colunt
 * \param dist Distribution object
 * \param context Skylark context.
 */
template<typename T, El::Distribution CD, El::Distribution RD,
         template<typename> class DistributionType>
void RandomMatrix(El::DistMatrix<T, CD, RD> &A, El::Int m, El::Int n,
    DistributionType<T> &dist, context_t &context) {

    random_samples_array_t< DistributionType<T> > entries =
        context.allocate_random_samples_array(m * n, dist);

    A.Resize(m, n);

    size_t m0 = A.LocalHeight();
    size_t n0 = A.LocalWidth();
    T *data = A.Buffer();

#   ifdef SKYLARK_HAVE_OPENMP
#   pragma omp parallel for collapse(2)
#   endif
    for(size_t j = 0; j < n0; j++)
        for(size_t i = 0; i < m0; i++)
            data[j * m0 + i] = entries[A.GlobalCol(j) * m + A.GlobalRow(i)];
}

/**
 * Generate random matrix using specificed distribution
 * (i.i.d samples).
 *
 * Implementation for distributed matrices.
 *
 * \param A Output matrix
 * \param m,n Number of rows and colunt
 * \param dist Distribution object
 * \param context Skylark context.
 */
template<typename T, El::Distribution CD, El::Distribution RD,
         template<typename, typename> class DistributionType>
void RandomMatrix(El::DistMatrix<T, CD, RD> &A, El::Int m, El::Int n,
    DistributionType<T, T> &dist, context_t &context) {

    random_samples_array_t< DistributionType<T, T> > entries =
        context.allocate_random_samples_array(m * n, dist);

    A.Resize(m, n);

    size_t m0 = A.LocalHeight();
    size_t n0 = A.LocalWidth();
    T *data = A.Buffer();

#   ifdef SKYLARK_HAVE_OPENMP
#   pragma omp parallel for collapse(2)
#   endif
    for(size_t j = 0; j < n0; j++)
        for(size_t i = 0; i < m0; i++)
            data[j * m0 + i] = entries[A.GlobalCol(j) * m + A.GlobalRow(i)];
}

/**
 * Generate random matrix with i.i.d standard Gaussian entries.
 *
 * \param A Output matrix.
 * \param m,n Number of rows and columns.
 * \param context Skylark context.
 */
template<typename MatrixType>
void GaussianMatrix(MatrixType &A, El::Int m, El::Int n,
    context_t &context) {
    typedef typename utility::typer_t<MatrixType>::value_type value_type;

    boost::random::normal_distribution<value_type> dist;
    RandomMatrix(A, m, n, dist, context);
}

void GaussianMatrix(const boost::any &A, El::Int m, El::Int n,
    context_t &context) {
#define SKYLARK_GAUSSIANMATRIX_ANY_APPLY_DISPATCH(AT)   \
    if (A.type() == typeid(AT*)) {                      \
        GaussianMatrix(*boost::any_cast<AT*>(A), m, n,  \
            context);                                   \
        return;                                         \
    }

#if !(defined SKYLARK_NO_ANY)

    SKYLARK_GAUSSIANMATRIX_ANY_APPLY_DISPATCH(mdtypes::matrix_t);
    //SKYLARK_GAUSSIANMATRIX_ANY_APPLY_DISPATCH(mdtypes::el_matrix_t);
    SKYLARK_GAUSSIANMATRIX_ANY_APPLY_DISPATCH(mdtypes::dist_matrix_t);
    SKYLARK_GAUSSIANMATRIX_ANY_APPLY_DISPATCH(mdtypes::shared_matrix_t);
    SKYLARK_GAUSSIANMATRIX_ANY_APPLY_DISPATCH(mdtypes::root_matrix_t);
    SKYLARK_GAUSSIANMATRIX_ANY_APPLY_DISPATCH(mdtypes::dist_matrix_vc_star_t);
    SKYLARK_GAUSSIANMATRIX_ANY_APPLY_DISPATCH(mdtypes::dist_matrix_vr_star_t);
    SKYLARK_GAUSSIANMATRIX_ANY_APPLY_DISPATCH(mdtypes::dist_matrix_star_vc_t);
    SKYLARK_GAUSSIANMATRIX_ANY_APPLY_DISPATCH(mdtypes::dist_matrix_star_vr_t);

    SKYLARK_GAUSSIANMATRIX_ANY_APPLY_DISPATCH(mftypes::matrix_t);
    //SKYLARK_GAUSSIANMATRIX_ANY_APPLY_DISPATCH(mftypes::el_matrix_t);
    SKYLARK_GAUSSIANMATRIX_ANY_APPLY_DISPATCH(mftypes::dist_matrix_t);
    SKYLARK_GAUSSIANMATRIX_ANY_APPLY_DISPATCH(mftypes::shared_matrix_t);
    SKYLARK_GAUSSIANMATRIX_ANY_APPLY_DISPATCH(mftypes::root_matrix_t);
    SKYLARK_GAUSSIANMATRIX_ANY_APPLY_DISPATCH(mftypes::dist_matrix_vc_star_t);
    SKYLARK_GAUSSIANMATRIX_ANY_APPLY_DISPATCH(mftypes::dist_matrix_vr_star_t);
    SKYLARK_GAUSSIANMATRIX_ANY_APPLY_DISPATCH(mftypes::dist_matrix_star_vc_t);
    SKYLARK_GAUSSIANMATRIX_ANY_APPLY_DISPATCH(mftypes::dist_matrix_star_vr_t);
#endif

    SKYLARK_THROW_EXCEPTION (
            base::unsupported_base_operation()
            << base::error_msg(
            "GaussianMatrix for this combination of matrices is not supported in any interface"));

#undef SKYLARK_GAUSSIANMATRIX_ANY_APPLY_DISPATCH
}

/**
 * Generate random matrix with i.i.d [0,1) uniform entries.
 *
 * \param A Output matrix.
 * \param m,n Number of rows and columns.
 * \param context Skylark context.
 */
template<typename MatrixType>
void UniformMatrix(MatrixType &A, El::Int m, El::Int n,
    context_t &context) {
    typedef typename utility::typer_t<MatrixType>::value_type value_type;

    boost::random::uniform_01<value_type, value_type> dist;
    RandomMatrix(A, m, n, dist, context);
}

template<typename T>
void UniformMatrix(sparse_matrix_t<T> &A, El::Int m, El::Int n,
    context_t &context) {

    SKYLARK_THROW_EXCEPTION(unsupported_base_operation() <<
       error_msg("Uniform sparse matrix not supported "
           "and does not make sense."));
}

template<typename T>
void UniformMatrix(sparse_vc_star_matrix_t<T> &A, El::Int m, El::Int n,
    context_t &context) {

    SKYLARK_THROW_EXCEPTION(unsupported_base_operation() <<
        error_msg("Uniform sparse matrix not supported "
           "and does not make sense."));
}

void UniformMatrix(const boost::any &A, El::Int m, El::Int n,
    context_t &context) {
#define SKYLARK_UNIFORMMATRIX_ANY_APPLY_DISPATCH(AT)   \
    if (A.type() == typeid(AT*)) {                      \
        UniformMatrix(*boost::any_cast<AT*>(A), m, n,  \
            context);                                   \
        return;                                         \
    }

#if !(defined SKYLARK_NO_ANY)

    SKYLARK_UNIFORMMATRIX_ANY_APPLY_DISPATCH(mdtypes::matrix_t);
    //SKYLARK_UNIFORMMATRIX_ANY_APPLY_DISPATCH(mdtypes::el_matrix_t);
    SKYLARK_UNIFORMMATRIX_ANY_APPLY_DISPATCH(mdtypes::dist_matrix_t);
    SKYLARK_UNIFORMMATRIX_ANY_APPLY_DISPATCH(mdtypes::shared_matrix_t);
    SKYLARK_UNIFORMMATRIX_ANY_APPLY_DISPATCH(mdtypes::root_matrix_t);
    SKYLARK_UNIFORMMATRIX_ANY_APPLY_DISPATCH(mdtypes::dist_matrix_vc_star_t);
    SKYLARK_UNIFORMMATRIX_ANY_APPLY_DISPATCH(mdtypes::dist_matrix_vr_star_t);
    SKYLARK_UNIFORMMATRIX_ANY_APPLY_DISPATCH(mdtypes::dist_matrix_star_vc_t);
    SKYLARK_UNIFORMMATRIX_ANY_APPLY_DISPATCH(mdtypes::dist_matrix_star_vr_t);

    SKYLARK_UNIFORMMATRIX_ANY_APPLY_DISPATCH(mftypes::matrix_t);
    //SKYLARK_UNIFORMMATRIX_ANY_APPLY_DISPATCH(mftypes::el_matrix_t);
    SKYLARK_UNIFORMMATRIX_ANY_APPLY_DISPATCH(mftypes::dist_matrix_t);
    SKYLARK_UNIFORMMATRIX_ANY_APPLY_DISPATCH(mftypes::shared_matrix_t);
    SKYLARK_UNIFORMMATRIX_ANY_APPLY_DISPATCH(mftypes::root_matrix_t);
    SKYLARK_UNIFORMMATRIX_ANY_APPLY_DISPATCH(mftypes::dist_matrix_vc_star_t);
    SKYLARK_UNIFORMMATRIX_ANY_APPLY_DISPATCH(mftypes::dist_matrix_vr_star_t);
    SKYLARK_UNIFORMMATRIX_ANY_APPLY_DISPATCH(mftypes::dist_matrix_star_vc_t);
    SKYLARK_UNIFORMMATRIX_ANY_APPLY_DISPATCH(mftypes::dist_matrix_star_vr_t);
#endif

    SKYLARK_THROW_EXCEPTION (
            base::unsupported_base_operation()
            << base::error_msg(
            "UniformMatrix for this combination of matrices is not supported in any interface"));

#undef SKYLARK_UNIFORMMATRIX_ANY_APPLY_DISPATCH

}
} } // namespace skylark::base

#endif // SKYLARK_RANDOM_MATRICES_HPP
