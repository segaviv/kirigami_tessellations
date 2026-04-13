#pragma once

#include <Eigen/Eigen>
#include <unordered_map>
#ifdef __EMSCRIPTEN__
#include <emscripten/bind.h>
using namespace emscripten;
#endif
#include <vector>

namespace convert {

template <typename T>
std::vector<std::vector<T>> to_vec_vec(const Eigen::MatrixX<T> &m) {
  std::vector<std::vector<T>> vv(m.rows());
  for (int i = 0; i < m.rows(); ++i) {
    std::vector<T> v(m.cols());
    for (int j = 0; j < m.cols(); ++j) {
      v[j] = m(i, j);
    }
    vv[i] = v;
  }
  return vv;
}

template <typename T>
std::vector<Eigen::VectorX<T>> to_vec_mat(const Eigen::MatrixX<T> &m) {
  std::vector<Eigen::VectorX<T>> v(m.rows());
  for (int i = 0; i < m.rows(); ++i) {
    v[i] = m.row(i);
  }
  return v;
}

template <typename T, int R, int C>
Eigen::MatrixX<T> to_eig_mat(const std::vector<Eigen::Matrix<T, R, C>> &v) {
  Eigen::MatrixX<T> m(v.size(), v[0].size());
  for (int i = 0; i < v.size(); ++i) {
    m.row(i) = v[i];
  }
  return m;
}
template <typename T>
Eigen::MatrixX<T> to_eig_mat(const std::vector<std::vector<T>> &mat) {
  Eigen::MatrixX<T> m(mat.size(), mat[0].size());
  for (int i = 0; i < mat.size(); ++i) {
    for (int j = 0; j < mat[0].size(); ++j) {
      m(i, j) = mat[i][j];
    }
  }
  return m;
}

template <typename T> Eigen::MatrixX<T> to_3d(const Eigen::MatrixX<T> &m) {
  if (m.cols() == 3) {
    return m;
  }
  Eigen::MatrixX<T> m3d(m.rows(), 3);
  m3d << m.col(0), Eigen::MatrixX<T>::Zero(m.rows(), 1), m.col(1);
  return m3d;
}

template <typename T, typename Container> T to(const Container &v) {
  return T(v.begin(), v.end());
}

template <typename T, typename G>
std::vector<T> to_vector(const std::unordered_map<T, G> &m) {
  std::vector<T> v;
  v.reserve(m.size());
  for (const auto &p : m)
    v.push_back(p.first);
  return v;
}

template <typename T> auto eigen_vec_map(const std::vector<T> &vec) {
  return Eigen::VectorX<T>::Map(vec.data(), vec.size());
}

#ifdef __EMSCRIPTEN__
template<typename T>
Eigen::MatrixX<T> js_array_to_eig(val js_array) {
    int rows = js_array["length"].as<unsigned>();
    if (rows == 0) return Eigen::MatrixX<T>::Zero(0, 2);

    int cols = js_array[0]["length"].as<unsigned>();
    Eigen::MatrixX<T> mat(rows, cols);
    
    for (int i = 0; i < rows; i++) {
        val row = js_array[i];
        for (int j = 0; j < cols; j++) {
            mat(i, j) = row[j].as<T>();
        }
    }
    return mat;
}
#endif

} // namespace convert