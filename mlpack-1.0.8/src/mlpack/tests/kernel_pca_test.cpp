/**
 * @file kernel_pca_test.cpp
 * @author Ryan Curtin
 *
 * Test file for Kernel PCA.
 *
 * This file is part of MLPACK 1.0.8.
 *
 * MLPACK is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option) any
 * later version.
 *
 * MLPACK is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 * details (LICENSE.txt).
 *
 * You should have received a copy of the GNU General Public License along with
 * MLPACK.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <mlpack/core.hpp>
#include <mlpack/core/kernels/linear_kernel.hpp>
#include <mlpack/core/kernels/gaussian_kernel.hpp>
#include <mlpack/methods/kernel_pca/kernel_pca.hpp>

#include <boost/test/unit_test.hpp>
#include "old_boost_test_definitions.hpp"

BOOST_AUTO_TEST_SUITE(KernelPCATest);

using namespace mlpack;
using namespace mlpack::math;
using namespace mlpack::kpca;
using namespace mlpack::kernel;
using namespace std;
using namespace arma;

/**
 * If KernelPCA is working right, then it should turn a circle dataset into a
 * linearly separable dataset in one dimension (which is easy to check).
 */
BOOST_AUTO_TEST_CASE(CircleTransformationTest)
{
  // The dataset, which will have three concentric rings in three dimensions.
  arma::mat dataset;

  // Now, there are 750 points centered at the origin with unit variance.
  dataset.randn(3, 750);
  dataset *= 0.05;

  // Take the second 250 points and spread them away from the origin.
  for (size_t i = 250; i < 500; ++i)
  {
    // Push the point away from the origin by 2.
    const double pointNorm = norm(dataset.col(i), 2);

    dataset(0, i) += 2.0 * (dataset(0, i) / pointNorm);
    dataset(1, i) += 2.0 * (dataset(1, i) / pointNorm);
    dataset(2, i) += 2.0 * (dataset(2, i) / pointNorm);
  }

  // Take the third 500 points and spread them away from the origin.
  for (size_t i = 500; i < 750; ++i)
  {
    // Push the point away from the origin by 5.
    const double pointNorm = norm(dataset.col(i), 2);

    dataset(0, i) += 5.0 * (dataset(0, i) / pointNorm);
    dataset(1, i) += 5.0 * (dataset(1, i) / pointNorm);
    dataset(2, i) += 5.0 * (dataset(2, i) / pointNorm);
  }

  data::Save("circle.csv", dataset);

  // Now we have a dataset; we will use the GaussianKernel to perform KernelPCA
  // to take it down to one dimension.
  KernelPCA<GaussianKernel> p;
  p.Apply(dataset, 1);

  // Get the ranges of each "class".  These are all initialized as empty ranges
  // containing no points.
  Range ranges[3];
  ranges[0] = Range();
  ranges[1] = Range();
  ranges[2] = Range();

  // Expand the ranges to hold all of the points in the class.
  for (size_t i = 0; i < 250; ++i)
    ranges[0] |= dataset(0, i);
  for (size_t i = 250; i < 500; ++i)
    ranges[1] |= dataset(0, i);
  for (size_t i = 500; i < 750; ++i)
    ranges[2] |= dataset(0, i);

  // None of these ranges should overlap -- the classes should be linearly
  // separable.
  BOOST_REQUIRE_EQUAL(ranges[0].Contains(ranges[1]), false);
  BOOST_REQUIRE_EQUAL(ranges[0].Contains(ranges[2]), false);
  BOOST_REQUIRE_EQUAL(ranges[1].Contains(ranges[2]), false);
}

BOOST_AUTO_TEST_SUITE_END();
