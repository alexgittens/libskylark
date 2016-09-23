.. highlight:: rst

Numerical Linear Algebra Primitives
***********************************

This layer implements various numerical linear algebra primitives that are
accelerated using sketching.

.. _simple-linearls:

Randomized Linear Least-Squares
===============================

Based on the regression framework in the algorithmic layer, this functionality
provides sketching based linear least-squares regression routines. That is,
solve equations of the form:

.. math::
   \arg\min_X \|A * X - B\|_F

Note that the various algorithms can be executed directly using the regression API.
This layer just provides an easy-to-use functional interface that mirror Elemental's
`LeastSquares <http://libelemental.org/documentation/0.83/lapack-like/solve.html>`_
function, but uses sketching to accelerate the computation.

A running example is provided in *libskylark/examples/least_squares.cpp*.

Approximate Least-squares
-------------------------

Solve the problem approximately using a sktech-and-solve strategy.
Specifically, solve :math:`\arg\min_X \|S A X -  S B\|_F` where :math:`S \in R^{s\times n}`
is a Fast Johnson-Lindenstrauss Transform matrix
(`Ailon and Chazelle, 2009 <http://www.cs.princeton.edu/~chazelle/pubs/FJLT-sicomp09.pdf>`_).
It can be shown that if :math:`s` is large enough (as a function of :math:`\epsilon`), then

.. math::
   \|A * X_{approx} - B\|_F \leq (1+\epsilon) \|A * X - B\|_F

where :math:`X=\arg\min_X \|A * X - B\|_F` and :math:`X_{approx}` is the approximate solution.
For the best known bounds see `Boutsidis and Gittens (2013) <http://arxiv.org/abs/1204.0062>`_.

The algorithm used is the one described in:
 * | P. Drineas, M. W. Mahoney, S. Muthukrishnan, and T. Sarlos
   | `Faster Least Squares Approximation <http://arxiv.org/abs/0710.1435>`_
   | Numerische Mathematik, 117, 219-249 (2011).

Unlike the algorithm described in the paper we allow the user to set the size of :math:`s`.
There is also a default value, but it is much lower than the one suggested by that paper.

Note: it is assume that a :math:`s \times n` matrix can fit in the memory of a single node
(:math:`n` is the number of columns in :math:`A`).

*****

.. cpp:function:: void ApproximateLeastSquares(El::Orientation orientation, const El::Matrix<T>& A, const El::Matrix<T>& B, El::Matrix<T>& X, base::context_t& context, int sketch_size = -1)
.. cpp:function:: void ApproximateLeastSquares(El::Orientation orientation, const El::DistMatrix<T, El::VC, El::STAR>& A, const El::DistMatrix<T, El::VC, El::STAR>& B, El::DistMatrix<T, El::STAR, El::STAR>& X, base::context_t& context, int sketch_size = -1)

If `orientation` is set to ``NORMAL``, then approximate :math:`\arg\min_X \|A * X - B\|_F`, otherwise
`orientation` must be equal to ``ADJOINT`` and :math:`\arg\min_X \|A^H * X - B\|_F` is approximated.

sketch_size controls the number of rows in :math:`S`.

*****

A flavor of usage is given in the code snippet below.

.. code-block:: cpp

     #include <elemental.hpp>
     #include <skylark.hpp>
     ...
     // Setup regression problem with coefficient matrix A and target matrix B
     ...

     skybase::context_t context(23234);

     // Solve the Least Squres problem of minimizing || AX - B||_2 over X
     skylark::nla::ApproximateLeastSquares(El::NORMAL, A, X, B, context);

Faster Least-squares
--------------------

Solve the linear least-squares problem using a sketching-accelerated algorithm.
This algorithm uses sketching to build a preconditioner, and then uses the preconditioner
in an iterative method. While technically the solution found is approximate (due to the use
of an iterative method), the threshold is set close to machine precision
so the solution's accuracy is close to the full accuracy possible on a machine.

The algorithm used is the one described in:
 * | H. Avron, P. Maymounkov, and S. Toledo
   | `Blendenpik: Supercharging LAPACK's Least-Squares Solver <http://epubs.siam.org/doi/abs/10.1137/090767911>`_
   | SIAM Journal on Scientific Computing 32(3), 1217-1236, 2010

Note: it is assume that a :math:`4 n^2` matrix can fit in the memory of a single node
(:math:`n` is the number of columns in :math:`A`).

*****

.. cpp:function:: void FasterLeastSquares(El::Orientation orientation, const El::DistMatrix<T, El::VC, El::STAR>& A, const El::DistMatrix<T, El::VC, El::STAR>& B, El::DistMatrix<T, El::STAR, El::STAR>& X, base::context_t& context)

If `orientation` is set to ``NORMAL``, then approximate :math:`\arg\min_X \|A * X - B\|_F`, otherwise
`orientation` must be equal to ``ADJOINT`` and :math:`\arg\min_X \|A^H * X - B\|_F` is approximated.

*****

A flavor of usage is given in the code snippet below.

.. code-block:: cpp

     #include <elemental.hpp>
     #include <skylark.hpp>
     ...
     // Setup regression problem with coefficient matrix A and target matrix B
     ...

     skybase::context_t context(23234);

     // Solve the Least Squres problem of minimizing || AX - B||_2 over X
     skylark::nla::FasterLeastSquares(El::NORMAL, A, X, B, context);

Randomized Singular Value Decomposition
========================================
The randomized SVD functionality provides a distributed implementation of algorithms described in

	* | Halko, N. and Martinsson, P.G, and Tropp J.
          | `Finding structure with randomness: Probabilistic algorithms for constructing approximate matrix decompositions <http://arxiv.org/abs/0909.4061>`_
          | SIAM Rev., Survey and Review section, Vol. 53, num. 2, pp. 217-288, 2011

 The prototypical algorithm involves the following steps, given a matrix :math:`A`
	* Compute an approximate orthonormal basis for the range of :math:`A`, as specified by the columns of an orthonormal matrix :math:`Q`.
        * Use :math:`Q` to compute a standard factorization of :math:`A`.

The first step is accelerated using sketching.

Approximate SVD
---------------

Compute an approximate SVD in the sense that :math:`A \approx U S V^T`. The inner 
dimension :math:`k` is specified by the user as parameter.

.. cpp:function:: void ApproximateSVD(InputType &A, UType &U, SType &S, VType &V, int rank, base::context_t& context, approximate_svd_params_t params = approximate_svd_params_t())

For a running example, please see *libskylark/examples/svd.cpp*. This example provides a
complete driver for running this functionality from command-line.

Condition Number Estimation
===========================

Estimate the condition number of a matrix using a low-memory iterative method. Only
the ability to multiply the matrix by a vector, and its transpose by a vector is
required. No factorization is involved, so can estimate the condition number of very
large and unstructured matrices.

The algorithm used is the one described in:
 * | H. Avron, A. Durinsky, and S. Toledo
   | `Spectral Condition-Number Estimation of Large Sparse Matrices <http://arxiv.org/pdf/1301.1107v3.pdf>`_

.. cpp:function:: int CondEst(const MatrixType& A, double &cond, double &sigma_max, RightType &v_max, LeftType &u_max, double &sigma_min, double &sigma_min_c, RightType &v_min, LeftType &u_min, base::context_t &context, condest_params_t params = condest_params_t())

   Estimate the condition number of :math:`A`: estimate the largest and smallest 
   singular value. The estimate of the largest is highly accurate, and the 
   estimate of the lowest is typically a  not-too-bad upper bound. Returns also  
   certificate left and right vectors. For the 
   smallest singular vector, two estimates are returned: one with a certificate, and 
   one that is potentially (but not always) smaller but without certificate (it will 
   never be bigger than the estimate with certificate).
