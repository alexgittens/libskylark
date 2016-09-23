#ifndef SKYLARK_WZT_HPP
#define SKYLARK_WZT_HPP

#ifndef SKYLARK_SKETCH_HPP
#error "Include top-level sketch.hpp instead of including individuals headers"
#endif

#include <boost/random.hpp>

namespace skylark { namespace sketch {

/**
 * Woodruff-Zhang Transform (data)
 *
 * Woodruff-Zhang Transform is very similar to the Clarkson-Woodruff Transform:
 * it replaces the +1/-1 diagonal with reciprocal exponentia random enteries. 
 * It is sutiable for lp regression with 1 <= p <= 2.
 *
 * Reference:
 * D. Woodruff and Q. Zhang
 * Subspace Embeddings and L_p Regression Using Exponential Random
 * COLT 2013
 *
 * TODO current implementation is only one sketch index, when for 1 <= p <= 2
 *      you want more than one.
 */

template < typename InputMatrixType,
           typename OutputMatrixType = InputMatrixType >
struct WZT_t :
        public WZT_data_t,
        virtual public sketch_transform_t<InputMatrixType, OutputMatrixType > {

public:

    // We use composition to defer calls to hash_transform_t
    typedef hash_transform_t< InputMatrixType, OutputMatrixType,
                              boost::random::uniform_int_distribution,
                              boost::random::exponential_distribution > transform_t;

    typedef WZT_data_t data_type;
    typedef data_type::params_t params_t;

    WZT_t(int N, int S, double p, base::context_t& context)
        : data_type(N, S, p, context), _transform(*this) {

    }

    WZT_t(int N, int S, const params_t& params, base::context_t& context)
        : data_type(N, S, params, context),
          _transform(*this) {

    }

    WZT_t(const boost::property_tree::ptree &pt)
        : data_type(pt), _transform(*this) {

    }

    template< typename OtherInputMatrixType,
              typename OtherOutputMatrixType >
    WZT_t(const WZT_t<OtherInputMatrixType,OtherOutputMatrixType>& other)
        : data_type(other), _transform(*this) {

    }

    WZT_t(const data_type& other)
        : data_type(other), _transform(*this) {

    }

    /**
     * Apply columnwise the sketching transform that is described by the
     * the transform with output sketch_of_A.
     */
    void apply (const typename transform_t::matrix_type& A,
                typename transform_t::output_matrix_type& sketch_of_A,
                columnwise_tag dimension) const {
        _transform.apply(A, sketch_of_A, dimension);
    }

    /**
     * Apply rowwise the sketching transform that is described by the
     * the transform with output sketch_of_A.
     */
    void apply (const typename transform_t::matrix_type& A,
                typename transform_t::output_matrix_type& sketch_of_A,
                rowwise_tag dimension) const {
        _transform.apply(A, sketch_of_A, dimension);
    }

    int get_N() const { return this->_N; } /**< Get input dimesion. */
    int get_S() const { return this->_S; } /**< Get output dimesion. */

    const sketch_transform_data_t* get_data() const { return this; }

private:
    transform_t _transform;
};

template<>
class WZT_t<boost::any, boost::any> :
  public WZT_data_t,
  virtual public sketch_transform_t<boost::any, boost::any > {

public:

    typedef WZT_data_t data_type;
    typedef data_type::params_t params_t;

    WZT_t(int N, int S, double p, base::context_t& context)
        : data_type(N, S, p, context) {

    }

    WZT_t(int N, int S, const params_t& params, base::context_t& context)
        : data_type(N, S, params, context) {

    }


    WZT_t(const boost::property_tree::ptree &pt)
        : data_type(pt) {

    }

    /**
     * Copy constructor
     */
    template <typename OtherInputMatrixType,
              typename OtherOutputMatrixType>
    WZT_t (const WZT_t<OtherInputMatrixType, OtherOutputMatrixType>& other)
        : data_type(other) {

    }

    /**
     * Constructor from data
     */
    WZT_t (const data_type& other)
        : data_type(other) {

    }

    /**
     * Apply columnwise the sketching transform that is described by the
     * the transform with output sketch_of_A.
     */
    void apply(const boost::any &A, const boost::any &sketch_of_A,
                columnwise_tag dimension) const {

#if     !(defined SKYLARK_NO_ANY)

        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mdtypes::matrix_t, mdtypes::matrix_t,
            WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mdtypes::sparse_matrix_t,
            mdtypes::matrix_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mdtypes::sparse_matrix_t,
            mdtypes::sparse_matrix_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mdtypes::shared_matrix_t,
            mdtypes::shared_matrix_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mdtypes::root_matrix_t,
            mdtypes::root_matrix_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mdtypes::dist_matrix_t,
            mdtypes::shared_matrix_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mdtypes::dist_matrix_t,
            mdtypes::root_matrix_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mdtypes::dist_matrix_vc_star_t,
            mdtypes::root_matrix_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mdtypes::dist_matrix_vc_star_t,
            mdtypes::shared_matrix_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mdtypes::dist_matrix_vr_star_t,
            mdtypes::root_matrix_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mdtypes::dist_matrix_vr_star_t,
            mdtypes::shared_matrix_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mdtypes::dist_matrix_t,
            mdtypes::dist_matrix_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mdtypes::dist_matrix_vc_star_t,
            mdtypes::dist_matrix_vc_star_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mdtypes::dist_matrix_vr_star_t,
            mdtypes::dist_matrix_vr_star_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mdtypes::dist_matrix_star_vc_t,
            mdtypes::dist_matrix_star_vc_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mdtypes::dist_matrix_star_vr_t,
            mdtypes::dist_matrix_star_vr_t, WZT_t);

        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mftypes::matrix_t, mftypes::matrix_t,
            WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mftypes::sparse_matrix_t,
            mftypes::matrix_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mftypes::sparse_matrix_t,
            mftypes::sparse_matrix_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mftypes::shared_matrix_t,
            mftypes::shared_matrix_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mftypes::root_matrix_t,
            mftypes::root_matrix_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mftypes::dist_matrix_t,
            mftypes::shared_matrix_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mftypes::dist_matrix_t,
            mftypes::root_matrix_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mftypes::dist_matrix_vc_star_t,
            mftypes::root_matrix_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mftypes::dist_matrix_vc_star_t,
            mftypes::shared_matrix_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mftypes::dist_matrix_vr_star_t,
            mftypes::root_matrix_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mftypes::dist_matrix_vr_star_t,
            mftypes::shared_matrix_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mftypes::dist_matrix_t,
            mftypes::dist_matrix_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mftypes::dist_matrix_vc_star_t,
            mftypes::dist_matrix_vc_star_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mftypes::dist_matrix_vr_star_t,
            mftypes::dist_matrix_vr_star_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mftypes::dist_matrix_star_vc_t,
            mftypes::dist_matrix_star_vc_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mftypes::dist_matrix_star_vr_t,
            mftypes::dist_matrix_star_vr_t, WZT_t);

#endif

        SKYLARK_THROW_EXCEPTION (
          base::sketch_exception()
              << base::error_msg(
                 "This combination has not yet been implemented for WZT"));

    }

    /**
     * Apply rowwise the sketching transform that is described by the
     * the transform with output sketch_of_A.
     */
    void apply (const boost::any &A, const boost::any &sketch_of_A,
        rowwise_tag dimension) const {

#if     !(defined SKYLARK_NO_ANY)

        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mdtypes::matrix_t, mdtypes::matrix_t,
            WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mdtypes::sparse_matrix_t,
            mdtypes::matrix_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mdtypes::sparse_matrix_t,
            mdtypes::sparse_matrix_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mdtypes::shared_matrix_t,
            mdtypes::shared_matrix_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mdtypes::root_matrix_t,
            mdtypes::root_matrix_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mdtypes::dist_matrix_t,
            mdtypes::shared_matrix_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mdtypes::dist_matrix_t,
            mdtypes::root_matrix_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mdtypes::dist_matrix_vc_star_t,
            mdtypes::root_matrix_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mdtypes::dist_matrix_vc_star_t,
            mdtypes::shared_matrix_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mdtypes::dist_matrix_vr_star_t,
            mdtypes::root_matrix_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mdtypes::dist_matrix_vr_star_t,
            mdtypes::shared_matrix_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mdtypes::dist_matrix_t,
            mdtypes::dist_matrix_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mdtypes::dist_matrix_vc_star_t,
            mdtypes::dist_matrix_vc_star_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mdtypes::dist_matrix_vr_star_t,
            mdtypes::dist_matrix_vr_star_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mdtypes::dist_matrix_star_vc_t,
            mdtypes::dist_matrix_star_vc_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mdtypes::dist_matrix_star_vr_t,
            mdtypes::dist_matrix_star_vr_t, WZT_t);

        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mftypes::matrix_t, mftypes::matrix_t,
            WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mftypes::sparse_matrix_t,
            mftypes::matrix_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mftypes::sparse_matrix_t,
            mftypes::sparse_matrix_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mftypes::shared_matrix_t,
            mftypes::shared_matrix_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mftypes::root_matrix_t,
            mftypes::root_matrix_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mftypes::dist_matrix_t,
            mftypes::shared_matrix_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mftypes::dist_matrix_t,
            mftypes::root_matrix_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mftypes::dist_matrix_vc_star_t,
            mftypes::root_matrix_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mftypes::dist_matrix_vc_star_t,
            mftypes::shared_matrix_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mftypes::dist_matrix_vr_star_t,
            mftypes::root_matrix_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mftypes::dist_matrix_vr_star_t,
            mftypes::shared_matrix_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mftypes::dist_matrix_t,
            mftypes::dist_matrix_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mftypes::dist_matrix_vc_star_t,
            mftypes::dist_matrix_vc_star_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mftypes::dist_matrix_vr_star_t,
            mftypes::dist_matrix_vr_star_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mftypes::dist_matrix_star_vc_t,
            mftypes::dist_matrix_star_vc_t, WZT_t);
        SKYLARK_SKETCH_ANY_APPLY_DISPATCH(mftypes::dist_matrix_star_vr_t,
            mftypes::dist_matrix_star_vr_t, WZT_t);

#endif

        SKYLARK_THROW_EXCEPTION (
          base::sketch_exception()
              << base::error_msg(
                 "This combination has not yet been implemented for WZT"));

    }

    int get_N() const { return this->_N; } /**< Get input dimesion. */
    int get_S() const { return this->_S; } /**< Get output dimesion. */

    const sketch_transform_data_t* get_data() const { return this; }
};

} } /** namespace skylark::sketch */

#endif // SKYLARK_WZT_HPP
