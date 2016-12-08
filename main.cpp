//#if defined(_MSC_VER)
//#   if _MSC_VER < 1900 
//    // Do nothing, not C++11 compliant
//#   else
//#       define CHEBTOOLS_CPP11
//#   endif
//#elif __cplusplus <= 199711L
//  // Do nothing, not C++11 compliant
//#else
//# define CHEBTOOLS_CPP11
//#endif

#define CHEBTOOLS_CPP11

#include <algorithm>
#include <functional>
#include <vector>
#include <chrono>
#include <iostream>
#include <valarray>

#include "Eigen/Dense"

typedef Eigen::VectorXd vectype;

// From CoolProp
template<class T> bool is_in_closed_range(T x1, T x2, T x){ return (x >= std::min(x1, x2) && x <= std::max(x1, x2)); };

class ChebyshevExpansion {
private:
     double m_xmin, m_xmax;
     vectype m_c;
     vectype m_recurrence_buffer;
     Eigen::MatrixXd m_recurrence_buffer_matrix;
     void resize(){
         m_recurrence_buffer.resize(m_c.size());
     }

public:
    ChebyshevExpansion(const vectype &c, double xmin = -1, double xmax = 1) : m_c(c), m_xmin(xmin), m_xmax(xmax) { resize(); };
    ChebyshevExpansion(const std::vector<double> &c, double xmin = -1, double xmax = 1) : m_xmin(xmin), m_xmax(xmax) {
        m_c = Eigen::Map<const Eigen::VectorXd>(&(c[0]), c.size());
        resize();
    };

#if defined(CHEBTOOLS_CPP11)
    // Move constructor (C++11 only)
    ChebyshevExpansion(const vectype &&c, double xmin = -1, double xmax = 1) : m_c(c), m_xmin(xmin), m_xmax(xmax) { resize(); };
#endif
    
public:
    ChebyshevExpansion operator+(const ChebyshevExpansion &ce2) const {
        // TODO: when m_c and ce2.coef() not the same size, resize shorter one and pad the longer one
        if (m_c.size() != ce2.coef().size()) { throw std::exception("lengths not the same"); }
#if defined(CHEBTOOLS_CPP11) 
        return ChebyshevExpansion(std::move(ce2.coef()+m_c));
#else
        return ChebyshevExpansion(c);
#endif
    };
    ChebyshevExpansion& operator+=(const ChebyshevExpansion &donor) {
        std::size_t Ndonor = donor.coef().size(), N1 = m_c.size();
        std::size_t Nmin = std::min(N1, Ndonor), Nmax = std::max(N1, Ndonor);
        // The first Nmin terms overlap between the two vectors
        m_c.head(Nmin) += donor.coef().head(Nmin);
        // If the donor vector is longer than the current vector, resizing is needed
        if (Ndonor > N1) {
            m_c.resize(Ndonor);
            // Copy the last Nmax-Nmin values from the donor
            m_c.tail(Nmax-Nmin) = donor.coef().tail(Nmax-Nmin);
        }
        return *this;
    }
    ChebyshevExpansion operator*(double value) const { 
#if defined(CHEBTOOLS_CPP11) 
        return ChebyshevExpansion(std::move(m_c*value));
#else
        return ChebyshevExpansion(m_c*value);
#endif
    }
    ChebyshevExpansion& operator*=(double value) {
        m_c *= value;
        return *this; 
    }
    /// Friend function that allows for pre-multiplication by a constant value
    friend ChebyshevExpansion operator*(double value, const ChebyshevExpansion &ce) {
#if defined(CHEBTOOLS_CPP11) 
        return ChebyshevExpansion(std::move(ce.coef()*value));
#else
        return ChebyshevExpansion(mult_coeffs_by_double(ce.coef(), value));
#endif
    }

    const vectype &coef() const {
        return m_c; 
    };
    double y(const double x) {
        // Use the recurrence relationships to evaluate the Chebyshev expansion
        std::size_t Norder = m_c.size()-1;
        // Scale x linearly into the domain [-1, 1]
        double xscaled = (2*x - (m_xmax+ m_xmin))/(m_xmax- m_xmin);
        vectype &o = m_recurrence_buffer;
        o(0) = 1;
        o(1) = xscaled;
        for (int n = 1; n < Norder; ++n){
            o(n+1) = 2*xscaled*o(n) - o(n-1);
        }
        return m_c.dot(o);
    }
    vectype y(const vectype &x) {
        // Scale x linearly into the domain [-1, 1]
        vectype xscaled = (2*x.array() - (m_xmax + m_xmin)) / (m_xmax - m_xmin);
        // Then call the function that takes the scaled x values
        return y_xscaled(xscaled);        
    }
    vectype y_xscaled(const vectype &xscaled) {

        // Use the recurrence relationships to evaluate the Chebyshev expansion
        std::size_t Norder = m_c.size() - 1;
        Eigen::MatrixXd &A = m_recurrence_buffer_matrix;

        A.resize(xscaled.size(), Norder + 1);
        A.col(0).fill(1);
        A.col(1) = xscaled;
        for (int n = 1; n < Norder; ++n) {
            A.col(n + 1).array() = 2 * xscaled.array()*A.col(n).array() - A.col(n - 1).array();
        }
        return A*m_c;
    }

    /** 
     * See Boyd, SIAM review, 2013, http://dx.doi.org/10.1137/110838297, Appendix A.2
     */
    Eigen::MatrixXd companion_matrix() {
        std::size_t N = m_c.size()-1;
        Eigen::MatrixXd A = Eigen::MatrixXd::Zero(N, N);
        // First row
        A(0, 1) = 1;
        
        // Last row
        A(N-1, N-2) = 0.5;
        for (int k = 0; k < N; ++k){
            A(N - 1, k) -= m_c(k)/(2.0*m_c(N));
        }
        // All the other rows
        for (int j = 1; j < N-1; ++j) {
            A(j, j-1) = 0.5;
            A(j, j+1) = 0.5;
        }
        return A;
    }
    std::vector<double> real_roots(bool only_in_domain = true) {
        std::vector<double> roots;

        // Roots of the Chebyshev expansion are eigenvalues of the companion matrix
        // obtained from the companion_matrix function
        Eigen::VectorXcd eigvals = companion_matrix().eigenvalues();

        for (int i = 0; i < eigvals.size(); ++i) {
            if (std::abs(eigvals(i).imag()) < 10*DBL_EPSILON) {
                // Rescale back into real-world values
                double x = ((m_xmax - m_xmin)*eigvals(i).real() + (m_xmax + m_xmin)) / 2.0;
                // Keep it if in the domain or if you want all real roots
                if (!only_in_domain || (x <= m_xmax && x >= m_xmin)) {
                    roots.push_back(x);
                }
            }
        }
        return roots;
    }
    std::vector<double> real_roots_approx(long Npoints)
    {
        std::vector<double> roots;
        // Vector of values in the range [-1,1] as roots of a high-order Chebyshev 
        Eigen::VectorXd xpts_n11 = (Eigen::VectorXd::LinSpaced(Npoints+1, 0, Npoints)*EIGEN_PI / Npoints).array().cos();
        // Scale values into real-world values
        Eigen::VectorXd ypts = y_xscaled(xpts_n11);
        // Eigen::MatrixXd buf(Npoints+1, 2); buf.col(0) = xpts; buf.col(1) = ypts; std::cout << buf << std::endl;
        for (size_t i = 0; i < Npoints - 1; ++i) {
            // The change of sign guarantees at least one root between indices i and i+1
            double y1 = ypts(i), y2 = ypts(i+1);
            bool signchange = (std::signbit(y1) != std::signbit(y2));
            if (signchange){
                // Fit a quadratic given three points; i and i+1 bracket the root, so need one more constraint
                // i0 is the leftmost of the three indices that will be used; when i == 0, use 
                // indices i,i+1,i+2, otherwise i-1,i,i+1
                size_t i0 = (i >= 1) ? i-1 : i; 
                Eigen::Vector3d r;
                r << ypts(i0), ypts(i0+1), ypts(i0+2);
                Eigen::Matrix3d A;
                for (std::size_t irow = 0; irow < 3; ++irow){
                    double _x = xpts_n11(i0 + irow);
                    A.row(irow) << _x*_x, _x, 1;
                }
                // abc holds the coefficients a,b,c for y = a*x^2 + b*x + c
                Eigen::VectorXd abc = A.colPivHouseholderQr().solve(r);
                double a = abc[0], b = abc[1], c = abc[2];

                // Solve the quadratic and find the root you want
                double x1 = (-b + sqrt(b*b - 4*a*c))/(2*a);
                double x2 = (-b - sqrt(b*b - 4*a*c))/(2*a);
                bool x1_in_range = is_in_closed_range(xpts_n11(i), xpts_n11(i+1), x1);
                bool x2_in_range = is_in_closed_range(xpts_n11(i), xpts_n11(i+1), x2);

                double xscaled;
                if (x1_in_range && !x2_in_range) {
                    xscaled = x1;
                }
                else if(x2_in_range && !x1_in_range) {
                    xscaled = x2;
                }
                else {
                    xscaled = 1e99;
                }

                // Rescale back into real-world values
                double x = ((m_xmax - m_xmin)*xscaled + (m_xmax + m_xmin)) / 2.0;
                roots.push_back(x);
            }
            else {
                // TODO: locate other roots based on derivative considerations
            }
        }
        return roots;
    }

    //std::string toString() const {
    //    return "[" + std::to_string(x) + ", " + std::to_string(y) + "]";
    //}
};


int p_i(int i, int N){
    if (i == 0 || i == N)
        return 2;
    else
        return 1;
}

/**
 * See Boyd, SIAM review, 2013, http://dx.doi.org/10.1137/110838297, Appendix A.
 */
template<class double_function>
ChebyshevExpansion generate_Chebyshev_expansion(int N, double_function func, double xmin, double xmax)
{
    Eigen::VectorXd f(N + 1);

    // Step 1&2: Grid points functional values (function evaluated at the
    // roots of the Chebyshev polynomial of order N)
    for (int k = 0; k <= N; ++k){
        double x_k = (xmax - xmin)/2.0*cos((EIGEN_PI*k)/N) + (xmax + xmin) / 2.0;
        f(k) = func(x_k);
    }

    // Step 3: Constrct the matrix of coefficients used to obtain a
    Eigen::MatrixXd L = Eigen::MatrixXd::Zero(N + 1, N + 1); ///< Matrix of coefficients
    for (int j = 0; j <= N; ++j){
        for (int k = 0; k <= N; ++k){
            L(j, k) = 2.0/(p_i(j, N)*p_i(k, N)*N)*cos((j*EIGEN_PI*k)/N);
        }
    }

    // Step 4: Obtain coefficients from vector - matrix product
    Eigen::VectorXd c = (L*f).rowwise().sum();
    return ChebyshevExpansion(c,xmin,xmax);
}

double plus_by_inplace(ChebyshevExpansion &ce, const ChebyshevExpansion &ce2, int N) {
    for (std::size_t i = 0; i < N; ++i) {
        ce += ce2;
    }
    return ce.coef()(0);
}

double mult_by_inplace(ChebyshevExpansion &ce, double val, int N) {
    for (std::size_t i = 0; i < N; ++i) {
        ce *= val;
    }
    return ce.coef()(0);
}

void mult_by(ChebyshevExpansion &ce, double val, int N) {
    Eigen::VectorXd c(2); c << 1, 0;
    ChebyshevExpansion ce2(c);
    for (std::size_t i = 0; i < N; ++i) {
        ce2 = ce*val;
    }
    //return ce2;
}

double f(double x){
    return exp(-pow(x,1));
}

#if defined(PYBIND11)

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/operators.h>
#include <pybind11/eigen.h>
#include <pybind11/functional.h>

namespace py = pybind11;

PYBIND11_PLUGIN(ChebTools) {
    py::module m("ChebTools", "C++ tools for working with Chebyshev expansions");

    m.def("mult_by", &mult_by);
    m.def("mult_by_inplace", &mult_by_inplace);
    m.def("generate_Chebyshev_expansion", &generate_Chebyshev_expansion<std::function<double(double)> >);
    
    py::class_<ChebyshevExpansion>(m, "ChebyshevExpansion")
        .def(py::init<const std::vector<double> &>())
        .def(py::self + py::self)
        .def(py::self += py::self)
        .def(py::self * double())
        .def(double() * py::self)
        .def(py::self *= double())
        //.def("__repr__", &Vector2::toString);
        .def("coef", &ChebyshevExpansion::coef)
        .def("companion_matrix", &ChebyshevExpansion::companion_matrix)
        .def("y", (vectype (ChebyshevExpansion::*)(const vectype &)) &ChebyshevExpansion::y)
        .def("y", (double (ChebyshevExpansion::*)(const double)) &ChebyshevExpansion::y)
        .def("real_roots", &ChebyshevExpansion::real_roots)
        .def("real_roots_approx", &ChebyshevExpansion::real_roots_approx)
        ;
    return m.ptr();
}
#else

// Monolithic build
int main(){

    auto ff = [](int N, std::function<double(double)> &func, double xmin, double xmax) {
        return generate_Chebyshev_expansion(N, *func.target<double(*)(double)>(), xmin, xmax);
    };
    auto cerrrrr0 = ff(10, f, 0, 6);

    ChebyshevExpansion ce0 = generate_Chebyshev_expansion(10, f, 0, 6);
    std::cout << ce0.coef() << std::endl;

    long N = 10000;
    Eigen::VectorXd c(50);
    c.fill(1);
    ChebyshevExpansion ce(c);

    auto startTime = std::chrono::system_clock::now();
        mult_by_inplace(ce, 1.001, N);
    auto endTime = std::chrono::system_clock::now();
    auto elap_us = std::chrono::duration<double>(endTime - startTime).count()/N*1e6;
    std::cout << elap_us << " us/call (mult inplace)\n";
    
    startTime = std::chrono::system_clock::now();
    plus_by_inplace(ce, ce, N);
    endTime = std::chrono::system_clock::now();
    elap_us = std::chrono::duration<double>(endTime - startTime).count()/N*1e6;
    std::cout << elap_us << " us/call (plus inplace)\n";

    startTime = std::chrono::system_clock::now();
        mult_by(ce, 1.001, N);
    endTime = std::chrono::system_clock::now();
    elap_us = std::chrono::duration<double>(endTime - startTime).count()/N*1e6;
    std::cout << elap_us << " us/call (mult)\n";

    int Norder = 50, Npoints = 200;
    Eigen::VectorXd a = Eigen::VectorXd::Random(Norder + 1);
    Eigen::VectorXd xpts = (Eigen::VectorXd::LinSpaced(Npoints, 0, Npoints-1)*EIGEN_PI / Npoints).array().cos(), ypts;
    Eigen::VectorXi signchange;
    ChebyshevExpansion cee(a);
    Eigen::MatrixXd buf(Npoints, 3);
    buf.col(0) = xpts;

    startTime = std::chrono::system_clock::now();
        for (int i = 0; i < N; ++i){
            ypts = cee.y(xpts);
        }
        std::cout << "y[0]:" << ypts[0] << std::endl;
        signchange = (1-(ypts.segment(0, Npoints).cwiseSign().cast<int>().array()*ypts.segment(1, Npoints).cwiseSign().cast<int>().array()))/2;
        std::cout << "this many roots:" << signchange.count() << std::endl;
    endTime = std::chrono::system_clock::now();
    elap_us = std::chrono::duration<double>(endTime - startTime).count()/N*1e6;
    std::cout << elap_us << " us/call (yvals)\n";
    buf.col(1) = ypts;

     startTime = std::chrono::system_clock::now();
         ypts.resize(xpts.size());
         for (int i = 0; i < N; ++i) {
             for (int n = 0; n < Npoints; ++n) {
                 ypts(n) = cee.y(xpts(n));
             }
         }
         std::cout << "y[0]:" << ypts[0] << std::endl;
         signchange = (1-(ypts.segment(0, Npoints-1).cwiseSign().cast<int>().array()*ypts.segment(1, Npoints-1).cwiseSign().cast<int>().array()))/2;
         std::cout << "this many roots:" << signchange.count() << std::endl;
     endTime = std::chrono::system_clock::now();
     elap_us = std::chrono::duration<double>(endTime - startTime).count() / N*1e6;
     std::cout << elap_us << " us/call (yvals, one-by-one)\n";
     buf.col(2) = ypts;
     std::cout << buf << std::endl;

    Eigen::MatrixXd B = Eigen::MatrixXd::Random(50, 50);
    N = 100;
    startTime = std::chrono::system_clock::now();
        const bool computeEigenvectors = false; 
        for (int i = 0; i < N; ++i){
            Eigen::EigenSolver<Eigen::MatrixXd> es(B, computeEigenvectors);
        }
    endTime = std::chrono::system_clock::now();
    elap_us = std::chrono::duration<double>(endTime - startTime).count()/N*1e6;
    std::cout << elap_us << " us/call (eigs 50x50)\n";

    return EXIT_SUCCESS;
}
#endif