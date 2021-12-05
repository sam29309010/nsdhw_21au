#include "MyAllocator.hpp"

#include <mkl.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/operators.h>

#include <vector>

namespace py = pybind11;

static MyAllocator<double> alloc;

class Matrix {
    friend Matrix multiply_naive(Matrix &m1, Matrix &m2);
    friend Matrix multiply_tile(Matrix &m1, Matrix &m2, const size_t tsize);
    friend Matrix multiply_mkl(Matrix const & mat1, Matrix const & mat2);
public:
    Matrix(const size_t nrow, const size_t ncol): nrow(nrow), ncol(ncol), data(alloc) {
        reset_buffer(nrow, ncol);
    };
    Matrix(Matrix const &other);
    Matrix(Matrix && other) : nrow(std::exchange(other.nrow, 0)), ncol(std::exchange(other.ncol, 0)),
                            data(std::move(other.data)) {}
    bool operator== (Matrix const &m) const;
    Matrix& operator=(const Matrix &m);
    double operator()(size_t row, size_t col) const {
        return data[index(row, col)];
    }
    double& operator()(size_t row, size_t col) {
        return data[index(row, col)];
    }
    double operator[](size_t idx) const { return data[idx]; }
    double& operator[](size_t idx) { return data[idx]; }

    void reset_buffer(size_t _nrow, size_t _ncol) {
        const size_t nelement = _nrow * _ncol;
        data.reserve(nelement);
        nrow = _nrow;
        ncol = _ncol;
    }

    size_t index(size_t row, size_t col) const { return row * ncol + col; }

    double   elapsed() const { return m_elapsed; }
    double & elapsed()       { return m_elapsed; }

    size_t   nflo() const { return m_nflo; }
    size_t & nflo()       { return m_nflo; }

    double gflops() const { return m_nflo / m_elapsed / 1.e9; }

    size_t nrow;
    size_t ncol;
    std::vector<double, MyAllocator<double>> data;
    double m_elapsed = 0;
    size_t m_nflo = 0; // number of floating-point operations.
};

Matrix::Matrix(Matrix const &other) : nrow(other.nrow), ncol(other.ncol), data(alloc) {
    reset_buffer(other.nrow, other.ncol);
    for (size_t i = 0; i < nrow; ++i) {
        for (size_t j = 0; j < ncol; ++j) {
            (*this)(i, j) = other(i, j);
        }
    }
}

size_t calc_nflo(Matrix const & mat1, Matrix const & mat2){
        return mat1.nrow * mat1.ncol * mat2.ncol;
}

Matrix multiply_naive(Matrix &m1, Matrix &m2){
    if (m1.ncol != m2.nrow)
        throw std::runtime_error("m1.ncol != m2.nrow");

    Matrix m(m1.nrow, m2.ncol);

    for (size_t i = 0; i < m1.nrow; ++i){
        for (size_t j = 0; j < m2.ncol; ++j){
            double result = 0;
            for (size_t k = 0; k < m1.ncol; ++k){
                result += m1(i, k) * m2(k, j); 
            }
            m(i, j) = result;
        }
    }

    return m;
}

Matrix multiply_tile(Matrix &m1, Matrix &m2, const size_t tsize){
    Matrix m(m1.nrow, m2.ncol);

    const size_t nrow1 = m1.nrow;
    const size_t ncol1 = m1.ncol;
    const size_t ncol2 = m2.ncol;
    for (size_t it = 0; it < nrow1; it += tsize){
        for (size_t jt = 0; jt < ncol2; jt += tsize){
            for (size_t kt = 0; kt < ncol1; kt += tsize){
                for (size_t i = 0; i < tsize && it + i < nrow1; ++i){
                    const size_t m_i = it + i;
                    for (size_t k = 0; k < tsize && kt + k < ncol1; ++k){
                        const size_t m2_k = kt + k;
                        const double m1_tmp = m1(it + i, kt + k);
                        for (size_t j = 0; j < tsize && jt + j < ncol2; ++j){
                            m(m_i, jt + j) += m1_tmp * m2(m2_k, jt + j);
                        }
                    }
                }
            }
        }
    }
    
    return m;
}

/*
 * Use MKL for the matrix matrix multiplication.
 */
Matrix multiply_mkl(Matrix const & mat1, Matrix const & mat2)
{
    mkl_set_num_threads(1);

    Matrix ret(mat1.nrow, mat2.ncol);

    cblas_dgemm(
        CblasRowMajor /* const CBLAS_LAYOUT Layout */
      , CblasNoTrans /* const CBLAS_TRANSPOSE transa */
      , CblasNoTrans /* const CBLAS_TRANSPOSE transb */
      , mat1.nrow /* const MKL_INT m */
      , mat2.ncol /* const MKL_INT n */
      , mat1.ncol /* const MKL_INT k */
      , 1.0 /* const double alpha */
      , mat1.data.data() /* const double *a */
      , mat1.ncol /* const MKL_INT lda */
      , mat2.data.data() /* const double *b */
      , mat2.ncol /* const MKL_INT ldb */
      , 0.0 /* const double beta */
      , ret.data.data() /* double * c */
      , ret.ncol /* const MKL_INT ldc */
    );

    return ret;
}

bool Matrix::operator== (Matrix const &m) const{
    for (size_t i = 0; i < nrow; ++i){
        for (size_t j = 0; j < ncol; ++j){
            if (data[index(i, j)] != m(i,j))
                return false;
        }
    }
    return true;
}

Matrix& Matrix::operator= (const Matrix &m){    
    if (this == &m) {
            return *this;
    }
    if (nrow != m.nrow || ncol != m.ncol) {
        reset_buffer(m.nrow, m.ncol);
    }
    for (size_t i = 0; i < nrow; ++i) {
        for (size_t j = 0; j < ncol; ++j) {
            (*this)(i, j) = m(i, j);
        }
    }
    return *this;
}


std::size_t bytes() { return alloc.bytes(); }
std::size_t allocated() { return alloc.allocated(); }
std::size_t deallocated() { return alloc.deallocated(); }


PYBIND11_MODULE(_matrix, m) {
    m.def("bytes", &bytes);
    m.def("allocated", &allocated);
    m.def("deallocated", &deallocated);
    m.def("multiply_naive", &multiply_naive);
    m.def("multiply_tile", &multiply_tile);
    m.def("multiply_mkl", &multiply_mkl);
    py::class_<Matrix>(m, "Matrix")
        .def(py::init<const size_t, const size_t>())
        .def_readwrite("nrow", &Matrix::nrow)
        .def_readwrite("ncol", &Matrix::ncol)
        .def("__getitem__", [](Matrix &m,  std::pair<size_t, size_t> i){
            return m(i.first, i.second);
        }, py::is_operator())
        .def("__setitem__", [](Matrix &m,  std::pair<size_t, size_t> i, const double v){
            m(i.first, i.second) = v;
        }, py::is_operator())
        .def(py::self == py::self)
        .def("assign", &Matrix::operator=);
}