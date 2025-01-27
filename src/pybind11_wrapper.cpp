#include "ChebTools/ChebTools.h"
#include "ChebTools/speed_tests.h"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/operators.h>
#include <pybind11/eigen.h>
#include <pybind11/functional.h>

#include "ChebToolsVersion.hpp"

namespace py = pybind11;
using namespace ChebTools;

template<typename vectype>
auto Clenshaw1D(const vectype &c, double ind){
    int N = static_cast<int>(c.size()) - 1;
    typename vectype::Scalar u_k = 0, u_kp1 = 0, u_kp2 = 0;
    for (int k = N; k >= 0; --k){
        // Do the recurrent calculation
        u_k = 2.0*ind*u_kp1 - u_kp2 + c[k];
        if (k > 0){
            // Update the values
            u_kp2 = u_kp1; u_kp1 = u_k;
        }
    }
    return (u_k - u_kp2)/2.0;
}

template<typename MatType, int Cols = MatType::ColsAtCompileTime>
auto Clenshaw1DByRow(const MatType& c, double ind) {
    int N = static_cast<int>(c.rows()) - 1;
    static Eigen::Array<typename MatType::Scalar, 1, Cols> u_k, u_kp1, u_kp2;
    // Not statically sized    
    if constexpr (Cols < 0) {
        int M = static_cast<int>(c.rows());
        u_k.resize(M); 
        u_kp1.resize(M);
        u_kp2.resize(M);
    }
    u_k.setZero(); u_kp1.setZero(); u_kp2.setZero();
    
    for (int k = N; k >= 0; --k) {
        // Do the recurrent calculation
        u_k = 2.0 * ind * u_kp1 - u_kp2 + c.row(k);
        if (k > 0) {
            // Update the values
            u_kp2 = u_kp1; u_kp1 = u_k;
        }
    }
    return (u_k - u_kp2) / 2.0;
}

/// With Eigen datatypes
template<typename MatType>
auto Clenshaw2DEigen(const MatType& a, double x, double y) {
    auto b = Clenshaw1DByRow(a, y);
    return Clenshaw1D(b.matrix(), x);
}

void init_ChebTools(py::module &m){

    m.def("mult_by", &mult_by);
    m.def("mult_by_inplace", &mult_by_inplace);
    m.def("evaluation_speed_test", &evaluation_speed_test);
    m.def("eigs_speed_test", &eigs_speed_test);
    m.def("eigenvalues", &eigenvalues);
    m.def("eigenvalues_upperHessenberg", &eigenvalues_upperHessenberg);
    m.def("factoryfDCT", &ChebyshevExpansion::factoryf); 
    m.def("factoryfFFT", &ChebyshevExpansion::factoryfFFT);
    m.def("generate_Chebyshev_expansion", &ChebyshevExpansion::factory<std::function<double(double)> >);
    m.def("dyadic_splitting", &ChebyshevExpansion::dyadic_splitting<std::vector<ChebyshevExpansion>>);
    m.def("Eigen_nbThreads", []() { return Eigen::nbThreads(); });
    m.def("Eigen_setNbThreads", [](int Nthreads) { return Eigen::setNbThreads(Nthreads); });
    m.def("make_Taylor_extrapolator", &make_Taylor_extrapolator);

    py::class_<ChebyshevExpansion>(m, "ChebyshevExpansion")
        .def(py::init<const std::vector<double> &, double, double>())
        .def(py::self + py::self)
        .def(py::self += py::self)
        .def(py::self + double())
        .def(py::self - double())
        .def(py::self * double())
        .def(double() * py::self)
        .def(py::self *= double())
        .def(py::self * py::self)
        // Unary operators
        .def(-py::self)

        .def("times_x", &ChebyshevExpansion::times_x)
        .def("times_x_inplace", &ChebyshevExpansion::times_x_inplace)
        .def("apply", &ChebyshevExpansion::apply)
        //.def("__repr__", &Vector2::toString);
        .def("coef", &ChebyshevExpansion::coef)
        .def("companion_matrix", &ChebyshevExpansion::companion_matrix)
        .def("y", (vectype(ChebyshevExpansion::*)(const vectype &) const) &ChebyshevExpansion::y)
        .def("y", (double (ChebyshevExpansion::*)(const double) const) &ChebyshevExpansion::y)
        .def("y_Clenshaw", &ChebyshevExpansion::y_Clenshaw)
        .def("real_roots", &ChebyshevExpansion::real_roots)
        .def("real_roots_time", &ChebyshevExpansion::real_roots_time)
        .def("real_roots_approx", &ChebyshevExpansion::real_roots_approx)
        .def("subdivide", &ChebyshevExpansion::subdivide)
        .def("real_roots_intervals", &ChebyshevExpansion::real_roots_intervals)
        .def("deriv", &ChebyshevExpansion::deriv)
        .def("integrate", &ChebyshevExpansion::integrate)
        .def("xmin", &ChebyshevExpansion::xmin)
        .def("xmax", &ChebyshevExpansion::xmax)
        .def("get_nodes_n11", py::overload_cast<>(&ChebyshevExpansion::get_nodes_n11, py::const_), "Get the Chebyshev-Lobatto nodes in [-1,1]")
        .def("get_nodes_realworld", py::overload_cast<>(&ChebyshevExpansion::get_nodes_realworld, py::const_), "Get the Chebyshev-Lobatto nodes in [xmin, xmax]")
        .def("get_node_function_values", &ChebyshevExpansion::get_node_function_values)
        .def("monotonic_solvex", &ChebyshevExpansion::monotonic_solvex)
        ;

    using Container = ChebyshevCollection::Container;
    py::class_<ChebyshevCollection>(m, "ChebyshevCollection")
        .def(py::init<const Container&>())
        .def("__call__", [](const ChebyshevCollection& c, const double x) { return c(x); }, py::is_operator())
        .def("integrate", &ChebyshevCollection::integrate)
        .def("get_exps", &ChebyshevCollection::get_exps)
        .def("get_extrema", &ChebyshevCollection::get_extrema)
        .def("solve_for_x", &ChebyshevCollection::solve_for_x)
        .def("make_inverse", &ChebyshevCollection::make_inverse)
        .def("get_hinted_index", &ChebyshevCollection::get_hinted_index)
        ;

    using TE = TaylorExtrapolator<Eigen::ArrayXd>;
    py::class_<TE>(m, "TaylorExtrapolator")
        .def("__call__", [](const TE& c, const Eigen::ArrayXd &x) { return c(x); }, py::is_operator())
        .def("__call__", [](const TE& c, const double& x) { return c(x); }, py::is_operator())
        .def("get_coef", &TaylorExtrapolator<Eigen::ArrayXd>::get_coef)
        ;

    m.def("Clenshaw2DEigen", &Clenshaw2DEigen<Eigen::Ref<const Eigen::ArrayXXd>>);
    m.def("Clenshaw2DEigencomplex", &Clenshaw2DEigen<Eigen::Ref<const Eigen::ArrayXXcd>>);
}

PYBIND11_MODULE(ChebTools, m) {
    m.doc() = "C++ tools for working with Chebyshev expansions";
    m.attr("__version__") = CHEBTOOLSVERSION;
    init_ChebTools(m);
}