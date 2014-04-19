/**
 * @file pca_main.cpp
 * @author Ryan Curtin
 *
 * Main executable to run PCA.
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

#include "pca.hpp"

using namespace mlpack;
using namespace mlpack::pca;
using namespace std;

// Document program.
PROGRAM_INFO("Principal Components Analysis", "This program performs principal "
    "components analysis on the given dataset.  It will transform the data "
    "onto its principal components, optionally performing dimensionality "
    "reduction by ignoring the principal components with the smallest "
    "eigenvalues.");

// Parameters for program.
PARAM_STRING_REQ("input_file", "Input dataset to perform PCA on.", "i");
PARAM_STRING_REQ("output_file", "File to save modified dataset to.", "o");
PARAM_INT("new_dimensionality", "Desired dimensionality of output dataset.  If "
    "0, no dimensionality reduction is performed.", "d", 0);
PARAM_DOUBLE("var_to_retain", "Amount of variance to retain; should be between "
    "0 and 1.  If 1, all variance is retained.  Overrides -d.", "V", 0);

PARAM_FLAG("scale", "If set, the data will be scaled before running PCA, such "
    "that the variance of each feature is 1.", "s");

int main(int argc, char** argv)
{
  // Parse commandline.
  CLI::ParseCommandLine(argc, argv);

  // Load input dataset.
  string inputFile = CLI::GetParam<string>("input_file");
  arma::mat dataset;
  data::Load(inputFile, dataset);

  // Find out what dimension we want.
  size_t newDimension = dataset.n_rows; // No reduction, by default.
  if (CLI::GetParam<int>("new_dimensionality") != 0)
  {
    // Validate the parameter.
    newDimension = (size_t) CLI::GetParam<int>("new_dimensionality");
    if (newDimension > dataset.n_rows)
    {
      Log::Fatal << "New dimensionality (" << newDimension
          << ") cannot be greater than existing dimensionality ("
          << dataset.n_rows << ")!" << endl;
    }
  }

  // Get the options for running PCA.
  const size_t scale = CLI::HasParam("scale");

  // Perform PCA.
  PCA p(scale);
  Log::Info << "Performing PCA on dataset..." << endl;
  double varRetained;
  if (CLI::GetParam<double>("var_to_retain") != 0)
  {
    if (CLI::GetParam<int>("new_dimensionality") != 0)
      Log::Warn << "New dimensionality (-d) ignored because -V was specified."
          << endl;

    varRetained = p.Apply(dataset, CLI::GetParam<double>("var_to_retain"));
  }
  else
  {
    varRetained = p.Apply(dataset, newDimension);
  }

  Log::Info << (varRetained * 100) << "% of variance retained (" <<
      dataset.n_rows << " dimensions)." << endl;

  // Now save the results.
  string outputFile = CLI::GetParam<string>("output_file");
  data::Save(outputFile, dataset);
}
