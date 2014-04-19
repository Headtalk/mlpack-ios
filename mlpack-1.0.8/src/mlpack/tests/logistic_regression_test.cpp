/**
 * @file logistic_regression_test.cpp
 * @author Ryan Curtin
 *
 * Test for LogisticFunction and LogisticRegression.
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
#include <mlpack/methods/logistic_regression/logistic_regression.hpp>
#include <mlpack/core/optimizers/sgd/sgd.hpp>

#include <boost/test/unit_test.hpp>
#include "old_boost_test_definitions.hpp"

using namespace mlpack;
using namespace mlpack::regression;
using namespace mlpack::optimization;
using namespace mlpack::distribution;

BOOST_AUTO_TEST_SUITE(LogisticRegressionTest);

/**
 * Test the LogisticRegressionFunction on a simple set of points.
 */
BOOST_AUTO_TEST_CASE(LogisticRegressionFunctionEvaluate)
{
  // Very simple fake dataset.
  arma::mat data("1 2 3;"
                 "1 2 3");
  arma::vec responses("1 1 0");

  // Create a LogisticRegressionFunction.
  LogisticRegressionFunction lrf(data, responses, 0.0 /* no regularization */);

  // These were hand-calculated using Octave.
  BOOST_REQUIRE_CLOSE(lrf.Evaluate(arma::vec("1 1 1")), 7.0562141665, 1e-5);
  BOOST_REQUIRE_CLOSE(lrf.Evaluate(arma::vec("0 0 0")), 2.0794415417, 1e-5);
  BOOST_REQUIRE_CLOSE(lrf.Evaluate(arma::vec("-1 -1 -1")), 8.0562141665, 1e-5);
  BOOST_REQUIRE_CLOSE(lrf.Evaluate(arma::vec("200 -40 -40")), 0.0, 1e-5);
  BOOST_REQUIRE_CLOSE(lrf.Evaluate(arma::vec("200 -80 0")), 0.0, 1e-5);
  BOOST_REQUIRE_CLOSE(lrf.Evaluate(arma::vec("200 -100 20")), 0.0, 1e-5);
}

/**
 * A more complicated test for the LogisticRegressionFunction.
 */
BOOST_AUTO_TEST_CASE(LogisticRegressionFunctionRandomEvaluate)
{
  const size_t points = 1000;
  const size_t dimension = 10;
  const size_t trials = 50;

  // Create a random dataset.
  arma::mat data;
  data.randu(dimension, points);
  // Create random responses.
  arma::vec responses(points);
  for (size_t i = 0; i < points; ++i)
    responses[i] = math::RandInt(0, 2);

  LogisticRegressionFunction lrf(data, responses, 0.0 /* no regularization */);

  // Run a bunch of trials.
  for (size_t i = 0; i < trials; ++i)
  {
    // Generate a random set of parameters.
    arma::vec parameters;
    parameters.randu(dimension + 1);

    // Hand-calculate the loss function.
    double loglikelihood = 0.0;
    for (size_t j = 0; j < points; ++j)
    {
      const double sigmoid = (1.0 / (1.0 + exp(-parameters[0] -
          arma::dot(data.col(j), parameters.subvec(1, dimension)))));
      if (responses[j] == 1.0)
        loglikelihood += log(std::pow(sigmoid, responses[j]));
      else
        loglikelihood += log(std::pow(1.0 - sigmoid, 1.0 - responses[j]));
    }

    BOOST_REQUIRE_CLOSE(lrf.Evaluate(parameters), -loglikelihood, 1e-5);
  }
}

/**
 * Test regularization for the LogisticRegressionFunction Evaluate() function.
 */
BOOST_AUTO_TEST_CASE(LogisticRegressionFunctionRegularizationEvaluate)
{
  const size_t points = 5000;
  const size_t dimension = 25;
  const size_t trials = 10;

  // Create a random dataset.
  arma::mat data;
  data.randu(dimension, points);
  // Create random responses.
  arma::vec responses(points);
  for (size_t i = 0; i < points; ++i)
    responses[i] = math::RandInt(0, 2);

  LogisticRegressionFunction lrfNoReg(data, responses, 0.0);
  LogisticRegressionFunction lrfSmallReg(data, responses, 0.5);
  LogisticRegressionFunction lrfBigReg(data, responses, 20.0);

  for (size_t i = 0; i < trials; ++i)
  {
    arma::vec parameters(dimension + 1);
    parameters.randu();

    // Regularization term: 0.5 * lambda * || parameters ||_2^2 (but note that
    // the first parameters term is ignored).
    const double smallRegTerm = 0.25 * std::pow(arma::norm(parameters, 2), 2.0)
        - 0.25 * std::pow(parameters[0], 2.0);
    const double bigRegTerm = 10.0 * std::pow(arma::norm(parameters, 2), 2.0)
        - 10.0 * std::pow(parameters[0], 2.0);

    BOOST_REQUIRE_CLOSE(lrfNoReg.Evaluate(parameters) + smallRegTerm,
        lrfSmallReg.Evaluate(parameters), 1e-5);
    BOOST_REQUIRE_CLOSE(lrfNoReg.Evaluate(parameters) + bigRegTerm,
        lrfBigReg.Evaluate(parameters), 1e-5);
  }
}

/**
 * Test gradient of the LogisticRegressionFunction.
 */
BOOST_AUTO_TEST_CASE(LogisticRegressionFunctionGradient)
{
  // Very simple fake dataset.
  arma::mat data("1 2 3;"
                 "1 2 3");
  arma::vec responses("1 1 0");

  // Create a LogisticRegressionFunction.
  LogisticRegressionFunction lrf(data, responses, 0.0 /* no regularization */);
  arma::vec gradient;

  // If the model is at the optimum, then the gradient should be zero.
  lrf.Gradient(arma::vec("200 -40 -40"), gradient);

  BOOST_REQUIRE_EQUAL(gradient.n_elem, 3);
  BOOST_REQUIRE_SMALL(gradient[0], 1e-15);
  BOOST_REQUIRE_SMALL(gradient[1], 1e-15);
  BOOST_REQUIRE_SMALL(gradient[2], 1e-15);

  // Perturb two elements in the wrong way, so they need to become smaller.
  lrf.Gradient(arma::vec("200 -20 -20"), gradient);

  // The actual values are less important; the gradient just needs to be pointed
  // the right way.
  BOOST_REQUIRE_EQUAL(gradient.n_elem, 3);
  BOOST_REQUIRE_GE(gradient[1], 0.0);
  BOOST_REQUIRE_GE(gradient[2], 0.0);

  // Perturb two elements in the wrong way, so they need to become larger.
  lrf.Gradient(arma::vec("200 -60 -60"), gradient);

  // The actual values are less important; the gradient just needs to be pointed
  // the right way.
  BOOST_REQUIRE_EQUAL(gradient.n_elem, 3);
  BOOST_REQUIRE_LE(gradient[1], 0.0);
  BOOST_REQUIRE_LE(gradient[2], 0.0);

  // Perturb the intercept element.
  lrf.Gradient(arma::vec("250 -40 -40"), gradient);

  // The actual values are less important; the gradient just needs to be pointed
  // the right way.
  BOOST_REQUIRE_EQUAL(gradient.n_elem, 3);
  BOOST_REQUIRE_GE(gradient[0], 0.0);
}

/**
 * Test individual Evaluate() functions for SGD.
 */
BOOST_AUTO_TEST_CASE(LogisticRegressionSeparableEvaluate)
{
  // Very simple fake dataset.
  arma::mat data("1 2 3;"
                 "1 2 3;");
  arma::vec responses("1 1 0");

  // Create a LogisticRegressionFunction.
  LogisticRegressionFunction lrf(data, responses, 0.0 /* no regularization */);

  // These were hand-calculated using Octave.
  BOOST_REQUIRE_CLOSE(lrf.Evaluate(arma::vec("1 1 1"), 0), 4.85873516e-2, 1e-5);
  BOOST_REQUIRE_CLOSE(lrf.Evaluate(arma::vec("1 1 1"), 1), 6.71534849e-3, 1e-5);
  BOOST_REQUIRE_CLOSE(lrf.Evaluate(arma::vec("1 1 1"), 2), 7.00091146645, 1e-5);

  BOOST_REQUIRE_CLOSE(lrf.Evaluate(arma::vec("0 0 0"), 0), 0.6931471805, 1e-5);
  BOOST_REQUIRE_CLOSE(lrf.Evaluate(arma::vec("0 0 0"), 1), 0.6931471805, 1e-5);
  BOOST_REQUIRE_CLOSE(lrf.Evaluate(arma::vec("0 0 0"), 2), 0.6931471805, 1e-5);

  BOOST_REQUIRE_CLOSE(lrf.Evaluate(arma::vec("-1 -1 -1"), 0), 3.0485873516,
      1e-5);
  BOOST_REQUIRE_CLOSE(lrf.Evaluate(arma::vec("-1 -1 -1"), 1), 5.0067153485,
      1e-5);
  BOOST_REQUIRE_CLOSE(lrf.Evaluate(arma::vec("-1 -1 -1"), 2), 9.1146645377e-4,
      1e-5);

  BOOST_REQUIRE_SMALL(lrf.Evaluate(arma::vec("200 -40 -40"), 0), 1e-5);
  BOOST_REQUIRE_SMALL(lrf.Evaluate(arma::vec("200 -40 -40"), 1), 1e-5);
  BOOST_REQUIRE_SMALL(lrf.Evaluate(arma::vec("200 -40 -40"), 2), 1e-5);

  BOOST_REQUIRE_SMALL(lrf.Evaluate(arma::vec("200 -80 0"), 0), 1e-5);
  BOOST_REQUIRE_SMALL(lrf.Evaluate(arma::vec("200 -80 0"), 1), 1e-5);
  BOOST_REQUIRE_SMALL(lrf.Evaluate(arma::vec("200 -80 0"), 2), 1e-5);

  BOOST_REQUIRE_SMALL(lrf.Evaluate(arma::vec("200 -100 20"), 0), 1e-5);
  BOOST_REQUIRE_SMALL(lrf.Evaluate(arma::vec("200 -100 20"), 1), 1e-5);
  BOOST_REQUIRE_SMALL(lrf.Evaluate(arma::vec("200 -100 20"), 2), 1e-5);
}

/**
 * Test regularization for the separable LogisticRegressionFunction Evaluate()
 * function.
 */
BOOST_AUTO_TEST_CASE(LogisticRegressionFunctionRegularizationSeparableEvaluate)
{
  const size_t points = 5000;
  const size_t dimension = 25;
  const size_t trials = 10;

  // Create a random dataset.
  arma::mat data;
  data.randu(dimension, points);
  // Create random responses.
  arma::vec responses(points);
  for (size_t i = 0; i < points; ++i)
    responses[i] = math::RandInt(0, 2);

  LogisticRegressionFunction lrfNoReg(data, responses, 0.0);
  LogisticRegressionFunction lrfSmallReg(data, responses, 0.5);
  LogisticRegressionFunction lrfBigReg(data, responses, 20.0);

  // Check that the number of functions is correct.
  BOOST_REQUIRE_EQUAL(lrfNoReg.NumFunctions(), points);
  BOOST_REQUIRE_EQUAL(lrfSmallReg.NumFunctions(), points);
  BOOST_REQUIRE_EQUAL(lrfBigReg.NumFunctions(), points);

  for (size_t i = 0; i < trials; ++i)
  {
    arma::vec parameters(dimension + 1);
    parameters.randu();

    // Regularization term: 0.5 * lambda * || parameters ||_2^2 (but note that
    // the first parameters term is ignored).
    const double smallRegTerm = (0.25 * std::pow(arma::norm(parameters, 2), 2.0)
        - 0.25 * std::pow(parameters[0], 2.0)) / points;
    const double bigRegTerm = (10.0 * std::pow(arma::norm(parameters, 2), 2.0)
        - 10.0 * std::pow(parameters[0], 2.0)) / points;

    for (size_t j = 0; j < points; ++j)
    {
      BOOST_REQUIRE_CLOSE(lrfNoReg.Evaluate(parameters, j) + smallRegTerm,
          lrfSmallReg.Evaluate(parameters, j), 1e-5);
      BOOST_REQUIRE_CLOSE(lrfNoReg.Evaluate(parameters, j) + bigRegTerm,
          lrfBigReg.Evaluate(parameters, j), 1e-5);
    }
  }
}

/**
 * Test separable gradient of the LogisticRegressionFunction.
 */
BOOST_AUTO_TEST_CASE(LogisticRegressionFunctionSeparableGradient)
{
  // Very simple fake dataset.
  arma::mat data("1 2 3;"
                 "1 2 3");
  arma::vec responses("1 1 0");

  // Create a LogisticRegressionFunction.
  LogisticRegressionFunction lrf(data, responses, 0.0 /* no regularization */);
  arma::vec gradient;

  // If the model is at the optimum, then the gradient should be zero.
  lrf.Gradient(arma::vec("200 -40 -40"), 0, gradient);

  BOOST_REQUIRE_EQUAL(gradient.n_elem, 3);
  BOOST_REQUIRE_SMALL(gradient[0], 1e-15);
  BOOST_REQUIRE_SMALL(gradient[1], 1e-15);
  BOOST_REQUIRE_SMALL(gradient[2], 1e-15);

  lrf.Gradient(arma::vec("200 -40 -40"), 1, gradient);
  BOOST_REQUIRE_EQUAL(gradient.n_elem, 3);
  BOOST_REQUIRE_SMALL(gradient[0], 1e-15);
  BOOST_REQUIRE_SMALL(gradient[1], 1e-15);
  BOOST_REQUIRE_SMALL(gradient[2], 1e-15);

  lrf.Gradient(arma::vec("200 -40 -40"), 2, gradient);
  BOOST_REQUIRE_EQUAL(gradient.n_elem, 3);
  BOOST_REQUIRE_SMALL(gradient[0], 1e-15);
  BOOST_REQUIRE_SMALL(gradient[1], 1e-15);
  BOOST_REQUIRE_SMALL(gradient[2], 1e-15);

  // Perturb two elements in the wrong way, so they need to become smaller.  For
  // the first two data points, classification is still correct so the gradient
  // should be zero.
  lrf.Gradient(arma::vec("200 -30 -30"), 0, gradient);
  BOOST_REQUIRE_EQUAL(gradient.n_elem, 3);
  BOOST_REQUIRE_SMALL(gradient[0], 1e-15);
  BOOST_REQUIRE_SMALL(gradient[1], 1e-15);
  BOOST_REQUIRE_SMALL(gradient[2], 1e-15);

  lrf.Gradient(arma::vec("200 -30 -30"), 1, gradient);
  BOOST_REQUIRE_EQUAL(gradient.n_elem, 3);
  BOOST_REQUIRE_SMALL(gradient[0], 1e-15);
  BOOST_REQUIRE_SMALL(gradient[1], 1e-15);
  BOOST_REQUIRE_SMALL(gradient[2], 1e-15);

  lrf.Gradient(arma::vec("200 -30 -30"), 2, gradient);
  BOOST_REQUIRE_EQUAL(gradient.n_elem, 3);
  BOOST_REQUIRE_GE(gradient[1], 0.0);
  BOOST_REQUIRE_GE(gradient[2], 0.0);

  // Perturb two elements in the other wrong way, so they need to become larger.
  // For the first and last data point, classification is still correct so the
  // gradient should be zero.
  lrf.Gradient(arma::vec("200 -60 -60"), 0, gradient);
  BOOST_REQUIRE_EQUAL(gradient.n_elem, 3);
  BOOST_REQUIRE_SMALL(gradient[0], 1e-15);
  BOOST_REQUIRE_SMALL(gradient[1], 1e-15);
  BOOST_REQUIRE_SMALL(gradient[2], 1e-15);

  lrf.Gradient(arma::vec("200 -30 -30"), 1, gradient);
  BOOST_REQUIRE_EQUAL(gradient.n_elem, 3);
  BOOST_REQUIRE_LE(gradient[1], 0.0);
  BOOST_REQUIRE_LE(gradient[2], 0.0);

  lrf.Gradient(arma::vec("200 -60 -60"), 2, gradient);
  BOOST_REQUIRE_EQUAL(gradient.n_elem, 3);
  BOOST_REQUIRE_SMALL(gradient[0], 1e-15);
  BOOST_REQUIRE_SMALL(gradient[1], 1e-15);
  BOOST_REQUIRE_SMALL(gradient[2], 1e-15);
}

/**
 * Test Gradient() function when regularization is used.
 */
BOOST_AUTO_TEST_CASE(LogisticRegressionFunctionRegularizationGradient)
{
  const size_t points = 5000;
  const size_t dimension = 25;
  const size_t trials = 10;

  // Create a random dataset.
  arma::mat data;
  data.randu(dimension, points);
  // Create random responses.
  arma::vec responses(points);
  for (size_t i = 0; i < points; ++i)
    responses[i] = math::RandInt(0, 2);

  LogisticRegressionFunction lrfNoReg(data, responses, 0.0);
  LogisticRegressionFunction lrfSmallReg(data, responses, 0.5);
  LogisticRegressionFunction lrfBigReg(data, responses, 20.0);

  for (size_t i = 0; i < trials; ++i)
  {
    arma::vec parameters(dimension + 1);
    parameters.randu();

    // Regularization term: 0.5 * lambda * || parameters ||_2^2 (but note that
    // the first parameters term is ignored).  Now we take the gradient of this
    // to obtain
    //   g[i] = lambda * parameters[i]
    // although g(0) == 0 because we are not regularizing the intercept term of
    // the model.
    arma::vec gradient;
    arma::vec smallRegGradient;
    arma::vec bigRegGradient;

    lrfNoReg.Gradient(parameters, gradient);
    lrfSmallReg.Gradient(parameters, smallRegGradient);
    lrfBigReg.Gradient(parameters, bigRegGradient);

    // Check sizes of gradients.
    BOOST_REQUIRE_EQUAL(gradient.n_elem, parameters.n_elem);
    BOOST_REQUIRE_EQUAL(smallRegGradient.n_elem, parameters.n_elem);
    BOOST_REQUIRE_EQUAL(bigRegGradient.n_elem, parameters.n_elem);

    // Make sure first term has zero regularization.
    BOOST_REQUIRE_CLOSE(gradient[0], smallRegGradient[0], 1e-5);
    BOOST_REQUIRE_CLOSE(gradient[0], bigRegGradient[0], 1e-5);

    // Check other terms.
    for (size_t j = 1; j < parameters.n_elem; ++j)
    {
      const double smallRegTerm = 0.5 * parameters[j];
      const double bigRegTerm = 20.0 * parameters[j];

      BOOST_REQUIRE_CLOSE(gradient[j] + smallRegTerm, smallRegGradient[j],
          1e-5);
      BOOST_REQUIRE_CLOSE(gradient[j] + bigRegTerm, bigRegGradient[j], 1e-5);
    }
  }
}

/**
 * Test separable Gradient() function when regularization is used.
 */
BOOST_AUTO_TEST_CASE(LogisticRegressionFunctionRegularizationSeparableGradient)
{
  const size_t points = 2000;
  const size_t dimension = 25;
  const size_t trials = 3;

  // Create a random dataset.
  arma::mat data;
  data.randu(dimension, points);
  // Create random responses.
  arma::vec responses(points);
  for (size_t i = 0; i < points; ++i)
    responses[i] = math::RandInt(0, 2);

  LogisticRegressionFunction lrfNoReg(data, responses, 0.0);
  LogisticRegressionFunction lrfSmallReg(data, responses, 0.5);
  LogisticRegressionFunction lrfBigReg(data, responses, 20.0);

  for (size_t i = 0; i < trials; ++i)
  {
    arma::vec parameters(dimension + 1);
    parameters.randu();

    // Regularization term: 0.5 * lambda * || parameters ||_2^2 (but note that
    // the first parameters term is ignored).  Now we take the gradient of this
    // to obtain
    //   g[i] = lambda * parameters[i]
    // although g(0) == 0 because we are not regularizing the intercept term of
    // the model.
    arma::vec gradient;
    arma::vec smallRegGradient;
    arma::vec bigRegGradient;

    // Test separable gradient for each point.  Regularization will be the same.
    for (size_t k = 0; k < points; ++k)
    {
      lrfNoReg.Gradient(parameters, k, gradient);
      lrfSmallReg.Gradient(parameters, k, smallRegGradient);
      lrfBigReg.Gradient(parameters, k, bigRegGradient);

      // Check sizes of gradients.
      BOOST_REQUIRE_EQUAL(gradient.n_elem, parameters.n_elem);
      BOOST_REQUIRE_EQUAL(smallRegGradient.n_elem, parameters.n_elem);
      BOOST_REQUIRE_EQUAL(bigRegGradient.n_elem, parameters.n_elem);

      // Make sure first term has zero regularization.
      BOOST_REQUIRE_CLOSE(gradient[0], smallRegGradient[0], 1e-5);
      BOOST_REQUIRE_CLOSE(gradient[0], bigRegGradient[0], 1e-5);

      // Check other terms.
      for (size_t j = 1; j < parameters.n_elem; ++j)
      {
        const double smallRegTerm = 0.5 * parameters[j] / points;
        const double bigRegTerm = 20.0 * parameters[j] / points;

        BOOST_REQUIRE_CLOSE(gradient[j] + smallRegTerm, smallRegGradient[j],
            1e-5);
        BOOST_REQUIRE_CLOSE(gradient[j] + bigRegTerm, bigRegGradient[j], 1e-5);
      }
    }
  }
}

// Test training of logistic regression on a simple dataset.
BOOST_AUTO_TEST_CASE(LogisticRegressionLBFGSSimpleTest)
{
  // Very simple fake dataset.
  arma::mat data("1 2 3;"
                 "1 2 3");
  arma::vec responses("1 1 0");

  // Create a logistic regression object using L-BFGS (that is the default).
  LogisticRegression<> lr(data, responses);

  // Test sigmoid function.
  arma::vec sigmoids = 1 / (1 + arma::exp(-lr.Parameters()[0]
      - data.t() * lr.Parameters().subvec(1, lr.Parameters().n_elem - 1)));

  // Large 0.1% error tolerance is because the optimizer may terminate before
  // the predictions converge to 1.
  BOOST_REQUIRE_CLOSE(sigmoids[0], 1.0, 0.1);
  BOOST_REQUIRE_CLOSE(sigmoids[1], 1.0, 5.0);
  BOOST_REQUIRE_SMALL(sigmoids[2], 0.1);
}

// Test training of logistic regression on a simple dataset using SGD.
BOOST_AUTO_TEST_CASE(LogisticRegressionSGDSimpleTest)
{
  // Very simple fake dataset.
  arma::mat data("1 2 3;"
                 "1 2 3");
  arma::vec responses("1 1 0");

  // Create a logistic regression object using SGD.
  LogisticRegression<SGD> lr(data, responses);

  // Test sigmoid function.
  arma::vec sigmoids = 1 / (1 + arma::exp(-lr.Parameters()[0]
      - data.t() * lr.Parameters().subvec(1, lr.Parameters().n_elem - 1)));

  // Large 0.1% error tolerance is because the optimizer may terminate before
  // the predictions converge to 1.  SGD tolerance is larger because its default
  // convergence tolerance is larger.
  BOOST_REQUIRE_CLOSE(sigmoids[0], 1.0, 3.0);
  BOOST_REQUIRE_CLOSE(sigmoids[1], 1.0, 12.0);
  BOOST_REQUIRE_SMALL(sigmoids[2], 0.1);
}

// Test training of logistic regression on a simple dataset with regularization.
BOOST_AUTO_TEST_CASE(LogisticRegressionLBFGSRegularizationSimpleTest)
{
  // Very simple fake dataset.
  arma::mat data("1 2 3;"
                 "1 2 3");
  arma::vec responses("1 1 0");

  // Create a logistic regression object using L-BFGS (that is the default).
  LogisticRegression<> lr(data, responses, 0.001);

  // Test sigmoid function.
  arma::vec sigmoids = 1 / (1 + arma::exp(-lr.Parameters()[0]
      - data.t() * lr.Parameters().subvec(1, lr.Parameters().n_elem - 1)));

  // Large error tolerance is because the optimizer may terminate before
  // the predictions converge to 1.
  BOOST_REQUIRE_CLOSE(sigmoids[0], 1.0, 5.0);
  BOOST_REQUIRE_CLOSE(sigmoids[1], 1.0, 10.0);
  BOOST_REQUIRE_SMALL(sigmoids[2], 0.1);
}

// Test training of logistic regression on a simple dataset using SGD with
// regularization.
BOOST_AUTO_TEST_CASE(LogisticRegressionSGDRegularizationSimpleTest)
{
  // Very simple fake dataset.
  arma::mat data("1 2 3;"
                 "1 2 3");
  arma::vec responses("1 1 0");

  // Create a logistic regression object using SGD.
  LogisticRegression<SGD> lr(data, responses, 0.001);

  // Test sigmoid function.
  arma::vec sigmoids = 1 / (1 + arma::exp(-lr.Parameters()[0]
      - data.t() * lr.Parameters().subvec(1, lr.Parameters().n_elem - 1)));

  // Large error tolerance is because the optimizer may terminate before
  // the predictions converge to 1.  SGD tolerance is wider because its default
  // convergence tolerance is larger.
  BOOST_REQUIRE_CLOSE(sigmoids[0], 1.0, 7.0);
  BOOST_REQUIRE_CLOSE(sigmoids[1], 1.0, 14.0);
  BOOST_REQUIRE_SMALL(sigmoids[2], 0.1);
}

// Test training of logistic regression on two Gaussians and ensure it's
// properly separable.
BOOST_AUTO_TEST_CASE(LogisticRegressionLBFGSGaussianTest)
{
  // Generate a two-Gaussian dataset.
  GaussianDistribution g1(arma::vec("1.0 1.0 1.0"), arma::eye<arma::mat>(3, 3));
  GaussianDistribution g2(arma::vec("9.0 9.0 9.0"), arma::eye<arma::mat>(3, 3));

  arma::mat data(3, 1000);
  arma::vec responses(1000);
  for (size_t i = 0; i < 500; ++i)
  {
    data.col(i) = g1.Random();
    responses[i] = 0;
  }
  for (size_t i = 501; i < 1000; ++i)
  {
    data.col(i) = g2.Random();
    responses[i] = 1;
  }

  // Now train a logistic regression object on it.
  LogisticRegression<> lr(data, responses, 0.5);

  // Ensure that the error is close to zero.
  const double acc = lr.ComputeAccuracy(data, responses);

  BOOST_REQUIRE_CLOSE(acc, 100.0, 0.3); // 0.3% error tolerance.

  // Create a test set.
  for (size_t i = 0; i < 500; ++i)
  {
    data.col(i) = g1.Random();
    responses[i] = 0;
  }
  for (size_t i = 501; i < 1000; ++i)
  {
    data.col(i) = g2.Random();
    responses[i] = 1;
  }

  // Ensure that the error is close to zero.
  const double testAcc = lr.ComputeAccuracy(data, responses);

  BOOST_REQUIRE_CLOSE(testAcc, 100.0, 0.6); // 0.6% error tolerance.
}

// Test training of logistic regression on two Gaussians and ensure it's
// properly separable using SGD.
BOOST_AUTO_TEST_CASE(LogisticRegressionSGDGaussianTest)
{
  // Generate a two-Gaussian dataset.
  GaussianDistribution g1(arma::vec("1.0 1.0 1.0"), arma::eye<arma::mat>(3, 3));
  GaussianDistribution g2(arma::vec("9.0 9.0 9.0"), arma::eye<arma::mat>(3, 3));

  arma::mat data(3, 1000);
  arma::vec responses(1000);
  for (size_t i = 0; i < 500; ++i)
  {
    data.col(i) = g1.Random();
    responses[i] = 0;
  }
  for (size_t i = 501; i < 1000; ++i)
  {
    data.col(i) = g2.Random();
    responses[i] = 1;
  }

  // Now train a logistic regression object on it.
  LogisticRegression<SGD> lr(data, responses, 0.5);

  // Ensure that the error is close to zero.
  const double acc = lr.ComputeAccuracy(data, responses);

  BOOST_REQUIRE_CLOSE(acc, 100.0, 0.3); // 0.3% error tolerance.

  // Create a test set.
  for (size_t i = 0; i < 500; ++i)
  {
    data.col(i) = g1.Random();
    responses[i] = 0;
  }
  for (size_t i = 501; i < 1000; ++i)
  {
    data.col(i) = g2.Random();
    responses[i] = 1;
  }

  // Ensure that the error is close to zero.
  const double testAcc = lr.ComputeAccuracy(data, responses);

  BOOST_REQUIRE_CLOSE(testAcc, 100.0, 0.6); // 0.6% error tolerance.
}

/**
 * Test constructor that takes an already-instantiated optimizer.
 */
BOOST_AUTO_TEST_CASE(LogisticRegressionInstantiatedOptimizer)
{
  // Very simple fake dataset.
  arma::mat data("1 2 3;"
                 "1 2 3");
  arma::vec responses("1 1 0");

  // Create an optimizer and function.
  LogisticRegressionFunction lrf(data, responses, 0.0005);
  L_BFGS<LogisticRegressionFunction> lbfgsOpt(lrf);
  lbfgsOpt.MinGradientNorm() = 1e-50;
  LogisticRegression<L_BFGS> lr(lbfgsOpt);

  // Test sigmoid function.
  arma::vec sigmoids = 1 / (1 + arma::exp(-lr.Parameters()[0]
      - data.t() * lr.Parameters().subvec(1, lr.Parameters().n_elem - 1)));

  // Error tolerance is small because we tightened the optimizer tolerance.
  BOOST_REQUIRE_CLOSE(sigmoids[0], 1.0, 0.1);
  BOOST_REQUIRE_CLOSE(sigmoids[1], 1.0, 0.6);
  BOOST_REQUIRE_SMALL(sigmoids[2], 0.1);

  // Now do the same with SGD.
  SGD<LogisticRegressionFunction> sgdOpt(lrf);
  sgdOpt.StepSize() = 0.15;
  sgdOpt.Tolerance() = 1e-75;
  LogisticRegression<SGD> lr2(sgdOpt);

  // Test sigmoid function.
  sigmoids = 1 / (1 + arma::exp(-lr2.Parameters()[0]
      - data.t() * lr2.Parameters().subvec(1, lr2.Parameters().n_elem - 1)));

  // Error tolerance is small because we tightened the optimizer tolerance.
  BOOST_REQUIRE_CLOSE(sigmoids[0], 1.0, 0.1);
  BOOST_REQUIRE_CLOSE(sigmoids[1], 1.0, 0.6);
  BOOST_REQUIRE_SMALL(sigmoids[2], 0.1);
}

BOOST_AUTO_TEST_SUITE_END();
