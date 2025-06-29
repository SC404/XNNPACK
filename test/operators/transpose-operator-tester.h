// Copyright 2022 Google LLC
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#ifndef XNNPACK_TEST_OPERATORS_TRANSPOSE_OPERATOR_TESTER_H_
#define XNNPACK_TEST_OPERATORS_TRANSPOSE_OPERATOR_TESTER_H_

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <memory>
#include <numeric>
#include <vector>

#include <gtest/gtest.h>
#include "include/xnnpack.h"
#include "src/xnnpack/buffer.h"
#include "src/xnnpack/math.h"

inline size_t reference_index(const size_t* input_stride,
                              const size_t* output_stride, const size_t* perm,
                              const size_t num_dims, size_t pos) {
  size_t in_pos = 0;
  for (size_t j = 0; j < num_dims; ++j) {
    const size_t idx = pos / output_stride[j];
    pos = pos % output_stride[j];
    in_pos += idx * input_stride[perm[j]];
  }
  return in_pos;
}

class TransposeOperatorTester {
 public:
  TransposeOperatorTester& num_dims(size_t num_dims) {
    assert(num_dims != 0);
    this->num_dims_ = num_dims;
    return *this;
  }

  size_t num_dims() const { return this->num_dims_; }

  TransposeOperatorTester& shape(std::vector<size_t> shape) {
    assert(shape.size() <= XNN_MAX_TENSOR_DIMS);
    this->shape_ = shape;
    return *this;
  }

  const std::vector<size_t>& dims() const { return this->shape_; }

  TransposeOperatorTester& perm(std::vector<size_t> perm) {
    assert(perm.size() <= XNN_MAX_TENSOR_DIMS);
    this->perm_ = perm;
    return *this;
  }

  const std::vector<size_t>& perm() const { return this->perm_; }

  void TestX8() const {
    size_t count = std::accumulate(dims().cbegin(), dims().cend(), size_t{1},
                                   std::multiplies<size_t>());
    xnnpack::Buffer<uint8_t> input(count, xnnpack::XnnExtraBytes);
    xnnpack::Buffer<uint8_t> output(count);
    std::vector<size_t> input_stride(num_dims(), 1);
    std::vector<size_t> output_stride(num_dims(), 1);
    for (size_t i = num_dims() - 1; i > 0; --i) {
      input_stride[i - 1] = input_stride[i] * shape_[i];
      output_stride[i - 1] = output_stride[i] * shape_[perm()[i]];
    }
    ASSERT_EQ(xnn_status_success, xnn_initialize(nullptr /* allocator */));
    xnn_operator_t transpose_op = nullptr;
    std::iota(input.begin(), input.end(), 0);

    ASSERT_EQ(xnn_status_success, xnn_create_transpose_nd_x8(0, &transpose_op));
    ASSERT_NE(nullptr, transpose_op);

    // Smart pointer to automatically delete convert op.
    std::unique_ptr<xnn_operator, decltype(&xnn_delete_operator)>
        auto_transpose_op(transpose_op, xnn_delete_operator);

    ASSERT_EQ(xnn_status_success,
              xnn_reshape_transpose_nd_x8(transpose_op, num_dims(),
                                          shape_.data(), perm_.data(),
                                          /*threadpool=*/nullptr));

    ASSERT_EQ(
        xnn_status_success,
        xnn_setup_transpose_nd_x8(transpose_op, input.data(), output.data()));

    // Run operator.
    ASSERT_EQ(xnn_status_success,
              xnn_run_operator(transpose_op, /*threadpool=*/nullptr));

    // Verify results.
    for (size_t i = 0; i < count; ++i) {
      const size_t in_idx =
          reference_index(input_stride.data(), output_stride.data(),
                          perm_.data(), num_dims(), i);
      ASSERT_EQ(input[in_idx], output[i]);
    }
  }

  void TestRunX8() const {
    ASSERT_EQ(xnn_status_success, xnn_initialize(nullptr /* allocator */));
    const size_t count = std::accumulate(dims().cbegin(), dims().cend(),
                                         size_t{1}, std::multiplies<size_t>());
    xnnpack::Buffer<uint8_t> input(count, xnnpack::XnnExtraBytes);
    xnnpack::Buffer<uint8_t> output(count);
    std::vector<size_t> input_stride(input.size(), 1);
    std::vector<size_t> output_stride(input.size(), 1);
    for (size_t i = num_dims() - 1; i > 0; --i) {
      input_stride[i - 1] = input_stride[i] * shape_[i];
      output_stride[i - 1] = output_stride[i] * shape_[perm()[i]];
    }
    std::iota(input.begin(), input.end(), 0);

    // Call transpose eager API
    ASSERT_EQ(
        xnn_status_success,
        xnn_run_transpose_nd_x8(input.data(), output.data(), num_dims(),
                                shape_.data(), perm_.data(), 0 /* flags */,
                                /*threadpool=*/nullptr));

    // Verify results.
    for (size_t i = 0; i < count; ++i) {
      const size_t in_idx =
          reference_index(input_stride.data(), output_stride.data(),
                          perm_.data(), num_dims(), i);
      ASSERT_EQ(input[in_idx], output[i]);
    }
  }

  void TestX16() const {
    size_t count = std::accumulate(dims().cbegin(), dims().cend(), size_t{1},
                                   std::multiplies<size_t>());
    xnnpack::Buffer<xnn_float16> input(count, xnnpack::XnnExtraBytes);
    xnnpack::Buffer<xnn_float16> output(count);
    std::vector<size_t> input_stride(num_dims(), 1);
    std::vector<size_t> output_stride(num_dims(), 1);
    for (size_t i = num_dims() - 1; i > 0; --i) {
      input_stride[i - 1] = input_stride[i] * shape_[i];
      output_stride[i - 1] = output_stride[i] * shape_[perm()[i]];
    }
    ASSERT_EQ(xnn_status_success, xnn_initialize(nullptr /* allocator */));
    xnn_operator_t transpose_op = nullptr;
    std::iota(input.begin(), input.end(), 0.0f);

    ASSERT_EQ(xnn_status_success,
              xnn_create_transpose_nd_x16(0, &transpose_op));
    ASSERT_NE(nullptr, transpose_op);

    // Smart pointer to automatically delete convert op.
    std::unique_ptr<xnn_operator, decltype(&xnn_delete_operator)>
        auto_transpose_op(transpose_op, xnn_delete_operator);

    ASSERT_EQ(xnn_status_success,
              xnn_reshape_transpose_nd_x16(transpose_op, num_dims(),
                                           shape_.data(), perm_.data(),
                                           /*threadpool=*/nullptr));

    ASSERT_EQ(
        xnn_status_success,
        xnn_setup_transpose_nd_x16(transpose_op, input.data(), output.data()));

    // Run operator.
    ASSERT_EQ(xnn_status_success,
              xnn_run_operator(transpose_op, /*threadpool=*/nullptr));

    // Verify results.
    for (size_t i = 0; i < count; ++i) {
      const size_t in_idx =
          reference_index(input_stride.data(), output_stride.data(),
                          perm_.data(), num_dims(), i);
      ASSERT_EQ(input[in_idx], output[i]);
    }
  }

  void TestRunX16() const {
    const size_t count = std::accumulate(dims().cbegin(), dims().cend(),
                                         size_t{1}, std::multiplies<size_t>());
    xnnpack::Buffer<xnn_float16> input(count, xnnpack::XnnExtraBytes);
    xnnpack::Buffer<xnn_float16> output(count);
    std::vector<size_t> input_stride(input.size(), 1);
    std::vector<size_t> output_stride(input.size(), 1);
    for (size_t i = num_dims() - 1; i > 0; --i) {
      input_stride[i - 1] = input_stride[i] * shape_[i];
      output_stride[i - 1] = output_stride[i] * shape_[perm()[i]];
    }
    std::iota(input.begin(), input.end(), 0.0f);

    // Call transpose eager API
    ASSERT_EQ(
        xnn_status_success,
        xnn_run_transpose_nd_x16(input.data(), output.data(), num_dims(),
                                 shape_.data(), perm_.data(), 0 /* flags */,
                                 /*threadpool=*/nullptr));

    // Verify results.
    for (size_t i = 0; i < count; ++i) {
      const size_t in_idx =
          reference_index(input_stride.data(), output_stride.data(),
                          perm_.data(), num_dims(), i);
      ASSERT_EQ(input[in_idx], output[i]);
    }
  }

  void TestX32() const {
    size_t count = std::accumulate(dims().cbegin(), dims().cend(), size_t{1},
                                   std::multiplies<size_t>());
    xnnpack::Buffer<uint32_t> input(count, xnnpack::XnnExtraBytes);
    xnnpack::Buffer<uint32_t> output(count);
    std::vector<size_t> input_stride(num_dims(), 1);
    std::vector<size_t> output_stride(num_dims(), 1);
    for (size_t i = num_dims() - 1; i > 0; --i) {
      input_stride[i - 1] = input_stride[i] * shape_[i];
      output_stride[i - 1] = output_stride[i] * shape_[perm()[i]];
    }
    ASSERT_EQ(xnn_status_success, xnn_initialize(nullptr /* allocator */));
    xnn_operator_t transpose_op = nullptr;
    std::iota(input.begin(), input.end(), 0);

    ASSERT_EQ(xnn_status_success,
              xnn_create_transpose_nd_x32(0, &transpose_op));
    ASSERT_NE(nullptr, transpose_op);

    // Smart pointer to automatically delete convert op.
    std::unique_ptr<xnn_operator, decltype(&xnn_delete_operator)>
        auto_transpose_op(transpose_op, xnn_delete_operator);

    ASSERT_EQ(xnn_status_success,
              xnn_reshape_transpose_nd_x32(transpose_op, num_dims(),
                                           shape_.data(), perm_.data(),
                                           /*threadpool=*/nullptr));

    ASSERT_EQ(
        xnn_status_success,
        xnn_setup_transpose_nd_x32(transpose_op, input.data(), output.data()));

    // Run operator.
    ASSERT_EQ(xnn_status_success,
              xnn_run_operator(transpose_op, /*threadpool=*/nullptr));

    // Verify results.
    for (size_t i = 0; i < count; ++i) {
      const size_t in_idx =
          reference_index(input_stride.data(), output_stride.data(),
                          perm_.data(), num_dims(), i);
      ASSERT_EQ(input[in_idx], output[i]);
    }
  }

  void TestX64() const {
    size_t count = std::accumulate(dims().cbegin(), dims().cend(), size_t{1},
                                   std::multiplies<size_t>());
    xnnpack::Buffer<uint64_t> input(count, xnnpack::XnnExtraBytes);
    xnnpack::Buffer<uint64_t> output(count);
    std::vector<size_t> input_stride(num_dims(), 1);
    std::vector<size_t> output_stride(num_dims(), 1);
    for (size_t i = num_dims() - 1; i > 0; --i) {
      input_stride[i - 1] = input_stride[i] * shape_[i];
      output_stride[i - 1] = output_stride[i] * shape_[perm()[i]];
    }
    ASSERT_EQ(xnn_status_success, xnn_initialize(nullptr /* allocator */));
    xnn_operator_t transpose_op = nullptr;
    std::iota(input.begin(), input.end(), 0);

    ASSERT_EQ(xnn_status_success,
              xnn_create_transpose_nd_x64(0, &transpose_op));
    ASSERT_NE(nullptr, transpose_op);

    // Smart pointer to automatically delete convert op.
    std::unique_ptr<xnn_operator, decltype(&xnn_delete_operator)>
        auto_transpose_op(transpose_op, xnn_delete_operator);

    ASSERT_EQ(xnn_status_success,
              xnn_reshape_transpose_nd_x64(transpose_op, num_dims(),
                                           shape_.data(), perm_.data(),
                                           /*threadpool=*/nullptr));

    ASSERT_EQ(
        xnn_status_success,
        xnn_setup_transpose_nd_x64(transpose_op, input.data(), output.data()));

    // Run operator.
    ASSERT_EQ(xnn_status_success,
              xnn_run_operator(transpose_op, /*threadpool=*/nullptr));

    // Verify results.
    for (size_t i = 0; i < count; ++i) {
      const size_t in_idx =
          reference_index(input_stride.data(), output_stride.data(),
                          perm_.data(), num_dims(), i);
      ASSERT_EQ(input[in_idx], output[i]);
    }
  }

  void TestRunX32() const {
    const size_t count = std::accumulate(dims().cbegin(), dims().cend(),
                                         size_t{1}, std::multiplies<size_t>());
    xnnpack::Buffer<uint32_t> input(count, xnnpack::XnnExtraBytes);
    xnnpack::Buffer<uint32_t> output(count);
    std::vector<size_t> input_stride(input.size(), 1);
    std::vector<size_t> output_stride(input.size(), 1);
    for (size_t i = num_dims() - 1; i > 0; --i) {
      input_stride[i - 1] = input_stride[i] * shape_[i];
      output_stride[i - 1] = output_stride[i] * shape_[perm()[i]];
    }
    std::iota(input.begin(), input.end(), 0);

    // Call transpose eager API
    ASSERT_EQ(
        xnn_status_success,
        xnn_run_transpose_nd_x32(input.data(), output.data(), num_dims(),
                                 shape_.data(), perm_.data(), 0 /* flags */,
                                 /*threadpool=*/nullptr));

    // Verify results.
    for (size_t i = 0; i < count; ++i) {
      const size_t in_idx =
          reference_index(input_stride.data(), output_stride.data(),
                          perm_.data(), num_dims(), i);
      ASSERT_EQ(input[in_idx], output[i]);
    }
  }

  void TestRunX64() const {
    const size_t count = std::accumulate(dims().cbegin(), dims().cend(),
                                         size_t{1}, std::multiplies<size_t>());
    xnnpack::Buffer<uint64_t> input(count, xnnpack::XnnExtraBytes);
    xnnpack::Buffer<uint64_t> output(count);
    std::vector<size_t> input_stride(input.size(), 1);
    std::vector<size_t> output_stride(input.size(), 1);
    for (size_t i = num_dims() - 1; i > 0; --i) {
      input_stride[i - 1] = input_stride[i] * shape_[i];
      output_stride[i - 1] = output_stride[i] * shape_[perm()[i]];
    }
    std::iota(input.begin(), input.end(), 0);

    // Call transpose eager API
    ASSERT_EQ(
        xnn_status_success,
        xnn_run_transpose_nd_x64(input.data(), output.data(), num_dims(),
                                 shape_.data(), perm_.data(), 0 /* flags */,
                                 /*threadpool=*/nullptr));

    // Verify results.
    for (size_t i = 0; i < count; ++i) {
      const size_t in_idx =
          reference_index(input_stride.data(), output_stride.data(),
                          perm_.data(), num_dims(), i);
      ASSERT_EQ(input[in_idx], output[i]);
    }
  }

 private:
  size_t num_dims_ = 1;
  std::vector<size_t> shape_;
  std::vector<size_t> perm_;
};

#endif  // XNNPACK_TEST_OPERATORS_TRANSPOSE_OPERATOR_TESTER_H_
