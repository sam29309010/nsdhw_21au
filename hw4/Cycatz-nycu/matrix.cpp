#include "mkl.h"
#include "matrix.h"
#include <sstream>
#include <stdexcept>
#include <iostream>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/operators.h>




static bool can_multiply(const Matrix &A, const Matrix &B)
{
    if (A.ncol() != B.nrow()) {
        throw std::invalid_argument("The two matrices cannot be multiplied");
        return false;
    }
    return true;
}

std::pair<size_t, size_t> Matrix::size() const {
    return std::make_pair(m_nrow, m_ncol);
}


bool Matrix::operator== (const Matrix &rhs) const {
    if (this->size() != rhs.size()) {
        return false;
    }

    for (size_t i = 0; i < m_nrow; i++) {
        for (size_t j = 0; j < m_ncol; j++) {
            if ((*this)(i, j) != rhs(i, j)) {
                return false;
            }
        }
    }

    return true;
}

std::string Matrix::to_string() const {
    std::stringstream ss;

    ss << "[";
    for (size_t i = 0; i < m_nrow; i++) {
        for (size_t j = 0; j < m_ncol; j++) {
            ss << " " <<( *this)(i, j);
        }
        if (i == m_nrow - 1) { ss << " ]"; }
        else { ss << std::endl; }
    }
    return ss.str();
}

bool Matrix::check_range(size_t i, size_t j) const {
    if (i >= m_nrow || j >= m_ncol) {
        throw std::out_of_range("matrix index out of range");
        return false;
    }
    return true;
}

Matrix multiply_naive(const Matrix &A, const Matrix &B) {
    if (A.ncol() != B.nrow())
        throw std::invalid_argument("The two matrices cannot be multiplied");
    Matrix C(A.nrow(), B.ncol());

    for (size_t i = 0; i < A.nrow(); i++) {
        for (size_t j = 0; j < B.ncol(); j++) {
            double sum = 0.0;
            for (size_t k = 0; k < A.ncol(); k++) {
                sum += A(i, k) * B(k, j); 
            }
            C(i, j) = sum;
        }
    }
    return C;
}



Matrix multiply_tile(const Matrix &A, const Matrix &B, size_t tile_width)
{

    size_t A_row = A.nrow();
    size_t A_col = A.ncol();
    size_t B_row = B.nrow();
    size_t B_col = B.ncol();

    if (A_col != B_row)
        throw std::invalid_argument("The two matrices cannot be multiplied");
    Matrix C(A_row, B_col);

    for (size_t i = 0; i < A_row; i += tile_width) {
        for (size_t j = 0; j < B_col; j += tile_width) {

            /* Tile bounds */  
            size_t rb = std::min(A_row, i + tile_width);
            size_t cb = std::min(B_col, j + tile_width);

            /* Calculation for tiles */    
            for (size_t k = 0; k < A_col; k += tile_width) {
                size_t tb = std::min(A_col, k + tile_width);

                for (size_t tk = k; tk < tb; tk++) {
                    for (size_t ti = i; ti < rb; ti++) {
                        const double A_ti_tk = A(ti, tk);
                        for (size_t tj = j; tj < cb; tj++) {
                            C(ti, tj) += A_ti_tk * B(tk, tj);
                        }
                    }
                }
            }
        }
    }

    return C;
}

Matrix multiply_mkl(const Matrix &A, const Matrix &B)
{
    if (A.ncol() != B.nrow())
        throw std::invalid_argument("The two matrices cannot be multiplied");
    Matrix C(A.nrow(), B.ncol());
    
    cblas_dgemm(CblasRowMajor,
                CblasNoTrans,
                CblasNoTrans,
                A.nrow(),                /* m */
                B.ncol(),                /* n */
                A.ncol(),                /* k */
                1.0,                     /* alpha */ 
                A.m_elems.data(),        /* A */
                A.ncol(),                /* lda */
                B.m_elems.data(),        /* B */
                B.ncol(),                /* ldb */
                0.0,                     /* beta */
                C.m_elems.data(),        /* C */
                C.ncol());               /* ldc */
    
    return C;
}

namespace py = pybind11;
PYBIND11_MODULE(_matrix, m) {
    m.def("multiply_tile", &multiply_tile, "Multiply two matrices by tiling");
    m.def("multiply_naive", &multiply_naive, "Multiply two matrices with the naive method");
    m.def("multiply_mkl", &multiply_mkl, "Multiply two matrices with mkl");
    m.def("bytes", &bytes);
    m.def("allocated", &allocated);
    m.def("deallocated", &deallocated);
    py::class_<Matrix>(m, "Matrix")
        .def(py::init<>())
        .def(py::init<size_t, size_t>())
        .def("__getitem__", [] (const Matrix &mat, std::array<int, 2> indices) {
            return mat(indices[0], indices[1]);
         })
        .def("__setitem__", [] (Matrix &mat, std::array<int, 2> idx, double val) {
            mat(idx[0], idx[1]) = val;
         })
        .def("__str__", [] (Matrix &mat) {
            return mat.to_string();
         })
        .def("__eq__", &Matrix::operator==)
        .def_property_readonly("nrow", &Matrix::nrow, "Number of rows")
        .def_property_readonly("ncol", &Matrix::ncol, "number of columns");
}

/*
int main()
{
    Matrix m(5, 5); 
    for (size_t i = 0; i < 5; i++) {
        for (size_t j = 0; j < 5; j++) {
            m(i, j) = i * j + 1;  
            std::cout << m(i, j) << " ";
        }
        std::cout << std::endl;
    }
    Matrix n = m;

    // std::cout << n.size().first <<  ' ' << n.size().second << std::endl; 

    Matrix k = multiply_tile(n, m, 10);
    for (size_t i = 0; i < 5; i++) {
        for (size_t j = 0; j < 5; j++) {
            std::cout << k(i, j) << " ";
        }
        std::cout << std::endl;
    }
}
*/