#ifndef SKYLARK_RLSC_HPP
#define SKYLARK_RLSC_HPP

namespace skylark { namespace ml {

struct rlsc_params_t : public base::params_t {

    // For iterative methods (FasterRLSC)
    int iter_lim;
    int res_print;
    double tolerance;

    rlsc_params_t(bool am_i_printing = 0,
        int log_level = 0,
        std::ostream &log_stream = std::cout,
        std::string prefix = "", 
        int debug_level = 0) :
        base::params_t(am_i_printing, log_level, log_stream, prefix, debug_level) {

        tolerance = 1e-3;
        res_print = 10;
        iter_lim = 1000;
  }

};

template<typename T, typename R, typename KernelType>
void KernelRLSC(base::direction_t direction, const KernelType &k, 
    const El::DistMatrix<T> &X, const El::DistMatrix<R> &L, T lambda, 
    El::DistMatrix<T> &A, std::vector<R> &rcoding,
    rlsc_params_t params = rlsc_params_t()) {

    bool log_lev1 = params.am_i_printing && params.log_level >= 1;
    bool log_lev2 = params.am_i_printing && params.log_level >= 2;

    boost::mpi::timer timer;

    // Form right hand side
    if (log_lev1) {
        params.log_stream << params.prefix
                          << "Dummy coding... ";
        params.log_stream.flush();
        timer.restart();
    }

    El::DistMatrix<T> Y;
    std::unordered_map<R, El::Int> coding;
    DummyCoding(El::NORMAL, Y, L, coding, rcoding);

    if (log_lev1)
        std::cout << "took " << boost::format("%.2e") % timer.elapsed()
                  << " sec\n";

    // Solve
    if (log_lev1) {
        params.log_stream << params.prefix
                          << "Solving... " << std::endl;
        timer.restart();
    }

    krr_params_t krr_params;
    krr_params.am_i_printing = params.am_i_printing;
    krr_params.log_level = params.log_level;
    krr_params.iter_lim = params.iter_lim;
    krr_params.res_print = params.res_print;
    krr_params.tolerance = params.tolerance;
    krr_params.prefix = params.prefix + "\t";

    KernelRidge(base::COLUMNS, k, X, Y, T(lambda), A, krr_params);

    if (log_lev1)
        params.log_stream << params.prefix
                          <<"Solve took " << boost::format("%.2e") % timer.elapsed()
                          << " sec\n";

}

template<typename T, typename R, typename KernelType>
void FasterKernelRLSC(base::direction_t direction, const KernelType &k,
    const El::DistMatrix<T> &X, const El::DistMatrix<R> &L, T lambda,
    El::DistMatrix<T> &A, std::vector<R> &rcoding,
    El::Int s, base::context_t &context,
    rlsc_params_t params = rlsc_params_t()) {

    bool log_lev1 = params.am_i_printing && params.log_level >= 1;
    bool log_lev2 = params.am_i_printing && params.log_level >= 2;

    boost::mpi::timer timer;

    // Form right hand side
    if (log_lev1) {
        params.log_stream << params.prefix
                          << "Dummy coding... ";
        params.log_stream.flush();
        timer.restart();
    }

    El::DistMatrix<T> Y;
    std::unordered_map<R, El::Int> coding;
    DummyCoding(El::NORMAL, Y, L, coding, rcoding);

    if (log_lev1)
        params.log_stream  << "took " << boost::format("%.2e") % timer.elapsed()
                           << " sec\n";

    // Solve
    if (log_lev1) {
        params.log_stream << params.prefix
                          << "Solving... " << std::endl;
        timer.restart();
    }

    krr_params_t krr_params;
    krr_params.am_i_printing = params.am_i_printing;
    krr_params.log_level = params.log_level;
    krr_params.iter_lim = params.iter_lim;
    krr_params.res_print = params.res_print;
    krr_params.tolerance = params.tolerance;
    krr_params.prefix = params.prefix + "\t";

    FasterKernelRidge(direction, k, X, Y,
        T(lambda), A, s, context, krr_params);

    if (log_lev1)
        params.log_stream << params.prefix
                          <<"Solve took " << boost::format("%.2e") % timer.elapsed()
                          << " sec\n";
}


} } // namespace skylark::ml

#endif