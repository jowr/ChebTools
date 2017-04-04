#ifndef CHEBTOOLS_H
#define CHEBTOOLS_H

#include "Eigen/Dense"
#include <vector>

namespace ChebTools{

    typedef Eigen::VectorXd vectype;

    const Eigen::VectorXd &get_extrema(std::size_t N);

    class ChebyshevExpansion {
    private:
        vectype m_c;
        double m_xmin, m_xmax;

        vectype m_recurrence_buffer;
        Eigen::MatrixXd m_recurrence_buffer_matrix;
        void resize() {
            m_recurrence_buffer.resize(m_c.size());
        }

    public:

        ChebyshevExpansion(const vectype &c, double xmin = -1, double xmax = 1) : m_c(c), m_xmin(xmin), m_xmax(xmax) { resize(); };
        ChebyshevExpansion(const std::vector<double> &c, double xmin = -1, double xmax = 1) : m_xmin(xmin), m_xmax(xmax) {
            m_c = Eigen::Map<const Eigen::VectorXd>(&(c[0]), c.size());
            resize();
        };
        double xmin(){ return m_xmin; }
        double xmax(){ return m_xmax; }

        // Move constructor (C++11 only)
        ChebyshevExpansion(const vectype &&c, double xmin = -1, double xmax = 1) : m_c(c), m_xmin(xmin), m_xmax(xmax) { resize(); };

        ChebyshevExpansion operator+(const ChebyshevExpansion &ce2) const ;
        ChebyshevExpansion& operator+=(const ChebyshevExpansion &donor);
        ChebyshevExpansion operator*(double value) const;
        ChebyshevExpansion operator+(double value) const;
        ChebyshevExpansion operator-(double value) const;
        ChebyshevExpansion& operator*=(double value);
        // Multiply two Chebyshev expansions together; thanks to Julia code from Bradley Alpert, NIST
        ChebyshevExpansion operator*(const ChebyshevExpansion &ce2) const;
        ChebyshevExpansion times_x() const;

        /// Friend function that allows for pre-multiplication by a constant value
        friend ChebyshevExpansion operator*(double value, const ChebyshevExpansion &ce){
            return ChebyshevExpansion(std::move(ce.coef()*value),ce.m_xmin, ce.m_xmax);
        };

        const vectype &coef() const ;
        /**
        * @brief Do a single input/single output evaluation of the Chebyshev expansion with the inputs scaled in [xmin, xmax]
        * @param x A value scaled in the domain [xmin,xmax]
        */
        double y_recurrence(const double x);
        double y_Clenshaw(const double x) const;
        /**
        * @brief Do a vectorized evaluation of the Chebyshev expansion with the inputs scaled in [xmin, xmax]
        * @param x A vectype of values in the domain [xmin,xmax]
        */
        vectype y(const vectype &x) const ;
        /**
        * @brief Do a vectorized evaluation of the Chebyshev expansion with the input scaled in the domain [-1,1]
        * @param xscaled A vectype of values scaled to the domain [-1,1] (the domain of the Chebyshev basis functions)
        * @param y A vectype of values evaluated from the expansion
        *
        * By using vectorizable types like Eigen::MatrixXd, without
        * any additional work, "magical" vectorization is happening
        * under the hood, giving a significant speed improvement. From naive
        * testing, the increase was a factor of about 10x.
        */
        vectype y_recurrence_xscaled(const vectype &xscaled) const ;
        vectype y_Clenshaw_xscaled(const vectype &xscaled) const ;

        /**
        * @brief Construct and return the companion matrix of the Chebyshev expansion
        * @returns A The companion matrix of the expansion
        *
        * See Boyd, SIAM review, 2013, http://dx.doi.org/10.1137/110838297, Appendix A.2
        */
        Eigen::MatrixXd companion_matrix() const ;
        /**
        * @brief Return the real roots of the Chebyshev expansion
        * @param only_in_domain If true, only real roots that are within the domain
        *                       of the expansion will be returned, otherwise all real roots
        *
        * The roots are obtained based on the fact that the eigenvalues of the
        * companion matrix are the roots of the Chebyshev expansion.  Thus
        * this function is relatively slow, because an eigenvalue solve is required,
        * which takes O(n^3) FLOPs.  But it is numerically rather reliable.
        *
        * As the order of the expansion increases, the eigenvalue solver in Eigen becomes
        * progressively less and less able to obtain the roots properly. The eigenvalue
        * solver in numpy tends to be more reliable.
        */
        std::vector<double> real_roots(bool only_in_domain = true) const ;
        std::vector<ChebyshevExpansion> subdivide(std::size_t Nintervals, std::size_t Norder) const ;
        static std::vector<double> real_roots_intervals(const std::vector<ChebyshevExpansion> &segments, bool only_in_domain = true);

        double real_roots_time(long N);
        std::vector<double> real_roots_approx(long Npoints);

        //std::string toString() const {
        //    return "[" + std::to_string(x) + ", " + std::to_string(y) + "]";
        //}

        static ChebyshevExpansion factoryf(const int N, const Eigen::VectorXd &f, const double xmin, const double xmax) ;

        /**
        * @brief Given a callable function, construct the N-th order Chebyshev expansion in [xmin, xmax]
        * @param N The order of the expansion; there will be N+1 coefficients
        * @param func A callable object, taking the x value (in [xmin,xmax]) and returning the y value
        * @param xmin The minimum x value for the fit
        * @param xmax The maximum x value for the fit
        *
        * See Boyd, SIAM review, 2013, http://dx.doi.org/10.1137/110838297, Appendix A.
        */
        template<class double_function>
        static ChebyshevExpansion factory(const int N, double_function func, const double xmin, const double xmax)
        {
            // Get the precalculated extrema values
            const Eigen::VectorXd & x_extrema_n11 = get_extrema(N); 

            // Step 1&2: Grid points functional values (function evaluated at the
            // extrema of the Chebyshev polynomial of order N - there are N+1 of them)
            Eigen::VectorXd f(N + 1);
            for (int k = 0; k <= N; ++k) {
                // The extrema in [-1,1] scaled to real-world coordinates
                double x_k = ((xmax - xmin)*x_extrema_n11(k) + (xmax + xmin)) / 2.0;
                f(k) = func(x_k);
            }
            return factoryf(N, f, xmin, xmax);
        };

        /// Convert a monomial term in the form \f$x^n\f$ to a Chebyshev expansion
        static ChebyshevExpansion from_powxn(const std::size_t n, const double xmin, const double xmax);

        template<class vector_type>
        static ChebyshevExpansion from_polynomial(vector_type c, const double xmin, const double xmax) {
            vectype c0(1); c0 << 0;
            ChebyshevExpansion s(c0, xmin, xmax);
            for (std::size_t i = 0; i < c.size(); ++i) {
                s += c(i)*from_powxn(i, xmin, xmax);
            }
            return s;
        }
        /// Return the N-th derivative of this expansion, where N must be >= 1
        ChebyshevExpansion deriv(std::size_t Nderiv) const ;

        /// Get the Chebyshev-Lobatto nodes
        Eigen::VectorXd get_nodes_n11();
        /// Values of the function at the Chebyshev-Lobatto nodes 
        Eigen::VectorXd get_node_function_values();
    };

    class SumElement {
    public:
        double n_i; ///< The leading coefficient
        ChebyshevExpansion F, ///< Expansion in terms of variable #1
            G; ///< Expansion in terms of variable #2
        SumElement(double n_i, ChebyshevExpansion &F, ChebyshevExpansion &G) :n_i(n_i), F(F), G(G) {};
    };

    class ChebyshevSummation {
    private:
        Eigen::MatrixXd TAUmat; ///< Coefficient matrix for the coefficients associated with each ChebyshevExpansion in the non-provided variable
        Eigen::MatrixXd DELTAmat; ///< matrix of coefficients for each expansion
        Eigen::VectorXd N; ///< Vector of coefficients n_i
        std::vector<SumElement> terms;
        Eigen::VectorXd givenvec; ///< Buffer for calculated values
        bool F_SPECIFIED = true;
        bool matrix_indep_built = false, matrix_dep_built = false;
        double m_xmin, m_xmax;
        void build_matrices() {
            build_dependent_matrix();
            build_independent_matrix();
        }
    public:
        ChebyshevSummation(const std::vector<SumElement> &terms, double xmin, double xmax) : terms(terms), m_xmin(xmin), m_xmax(xmax) {build_matrices();};
        ChebyshevSummation(const std::vector<SumElement> &&terms, double xmin, double xmax) : terms(terms), m_xmin(xmin), m_xmax(xmax) { build_matrices(); };
        /// Once you specify which variable will be given, you can build the independent variable matrix
        void build_independent_matrix();
        void build_dependent_matrix();
        Eigen::MatrixXd get_dependent_matrix(){ return DELTAmat; }
        Eigen::MatrixXd get_independent_matrix() { return TAUmat; }
        Eigen::VectorXd get_coefficients(double input);
        Eigen::VectorXd get_nFcoefficients_parallel(double input);
        Eigen::VectorXd get_nFcoefficients_serial(double input);
        std::vector<SumElement> &get_terms(){ return terms; };
        double xmin(){ return m_xmin; }
        double xmax(){ return m_xmax; }
    };

    class ChebyshevMixture {
    private:
        std::vector<std::vector<ChebyshevSummation> > interval_expansions; ///< A vector of intervals, in each interval, there is a vector of summations, each summation corresponding to one fluid
        std::vector<double> m_roots;
        Eigen::MatrixXd A;
        bool all_same_order = true;
        double previous_tau;
        void allocate(){
            std::vector<int> Nrows;
            // Determine the order of each of the expansions for delta
            for (auto &interval : interval_expansions) {
                for (auto &fluid : interval) {
                    for (auto &term : fluid.get_terms()) {
                        Nrows.push_back(static_cast<int>(term.G.coef().size()));
                    }
                }
            }
            // Determine the maximum and minimum orders; they must be the same!
            Eigen::Map<const Eigen::VectorXi> N_wrap(&(Nrows[0]), Nrows.size());
            Eigen::Index Nmax = N_wrap.maxCoeff(), Nmin = N_wrap.minCoeff();
            if (Nmax != Nmin) {
                throw std::range_error("All delta expansions are not of the same order");
            }
            A.resize(Nmax, interval_expansions[0].size());
            previous_tau = 1e20;
        }
    public:
        ChebyshevMixture(const std::vector<std::vector<ChebyshevSummation> > &intervals) : interval_expansions(intervals) {allocate();};
        ChebyshevMixture(const std::vector<std::vector<ChebyshevSummation> > &&intervals) : interval_expansions(intervals) {allocate();};

        Eigen::MatrixXd get_A() { return A; }
        ChebyshevExpansion get_expansion_of_interval(std::vector<ChebyshevSummation> &interval, double tau, const Eigen::VectorXd &z, double xmin, double xmax);
        void calc_real_roots(double rhoRT, double p, double tau, const Eigen::VectorXd &z, double ptolerance);
        double time_calc_real_roots(double rhorRT, double p_target, double tau, const Eigen::VectorXd &z, double ptolerance);
        ChebyshevExpansion get_p(std::vector<ChebyshevSummation> &interval, double rhorRT, double tau, const Eigen::VectorXd &z);
        double time_get(std::string &thing, double rhorRT, double tau, double p, const Eigen::VectorXd &z);
        ChebyshevExpansion get_dalphar_ddelta(std::size_t i, double rhorRT, double tau, const Eigen::VectorXd &z);
        std::vector<double> get_real_roots();
        bool unlikely_root(ChebyshevExpansion &pdiff, double ptolerance);
        std::size_t Nintervals(){ return interval_expansions.size(); };
        std::vector<std::vector<ChebyshevSummation> > get_intervals(){ return interval_expansions; };
        std::vector<Eigen::MatrixXd> calc_companion_matrices(double rhorRT, double p_target, double tau, const Eigen::VectorXd &z);
        Eigen::VectorXcd eigenvalues(Eigen::MatrixXd &mat, bool balance);
    };

}; /* namespace ChebTools */
#endif