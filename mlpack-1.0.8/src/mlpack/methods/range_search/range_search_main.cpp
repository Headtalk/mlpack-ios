/**
 * @file range_search_main.cpp
 * @author Ryan Curtin
 * @author Matthew Amidon
 *
 * Implementation of the RangeSearch executable.  Allows some number of standard
 * options.
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
#include <mlpack/core/metrics/lmetric.hpp>
#include <mlpack/core/tree/cover_tree.hpp>

#include "range_search.hpp"

using namespace std;
using namespace mlpack;
using namespace mlpack::range;
using namespace mlpack::tree;

// Information about the program itself.
PROGRAM_INFO("Range Search",
    "This program implements range search with a Euclidean distance metric. "
    "For a given query point, a given range, and a given set of reference "
    "points, the program will return all of the reference points with distance "
    "to the query point in the given range.  This is performed for an entire "
    "set of query points. You may specify a separate set of reference and query"
    " points, or only a reference set -- which is then used as both the "
    "reference and query set.  The given range is taken to be inclusive (that "
    "is, points with a distance exactly equal to the minimum and maximum of the"
    " range are included in the results)."
    "\n\n"
    "For example, the following will calculate the points within the range [2, "
    "5] of each point in 'input.csv' and store the distances in 'distances.csv'"
    " and the neighbors in 'neighbors.csv':"
    "\n\n"
    "$ range_search --min=2 --max=5 --reference_file=input.csv\n"
    "  --distances_file=distances.csv --neighbors_file=neighbors.csv"
    "\n\n"
    "The output files are organized such that line i corresponds to the points "
    "found for query point i.  Because sometimes 0 points may be found in the "
    "given range, lines of the output files may be empty.  The points are not "
    "ordered in any specific manner."
    "\n\n"
    "Because the number of points returned for each query point may differ, the"
    " resultant CSV-like files may not be loadable by many programs.  However, "
    "at this time a better way to store this non-square result is not known.  "
    "As a result, any output files will be written as CSVs in this manner, "
    "regardless of the given extension.");

// Define our input parameters that this program will take.
PARAM_STRING_REQ("reference_file", "File containing the reference dataset.",
    "r");
PARAM_STRING_REQ("distances_file", "File to output distances into.", "d");
PARAM_STRING_REQ("neighbors_file", "File to output neighbors into.", "n");

PARAM_DOUBLE_REQ("max", "Upper bound in range.", "M");
PARAM_DOUBLE("min", "Lower bound in range.", "m", 0.0);

PARAM_STRING("query_file", "File containing query points (optional).", "q", "");

PARAM_INT("leaf_size", "Leaf size for tree building.", "l", 20);
PARAM_FLAG("naive", "If true, O(n^2) naive mode is used for computation.", "N");
PARAM_FLAG("single_mode", "If true, single-tree search is used (as opposed to "
    "dual-tree search).", "s");
PARAM_FLAG("cover_tree", "If true, use a cover tree for range searching "
    "(instead of a kd-tree).", "c");

typedef RangeSearch<> RSType;
typedef CoverTree<metric::EuclideanDistance, tree::FirstPointIsRoot,
    RangeSearchStat> CoverTreeType;
typedef RangeSearch<metric::EuclideanDistance, CoverTreeType> RSCoverType;

int main(int argc, char *argv[])
{
  // Give CLI the command line parameters the user passed in.
  CLI::ParseCommandLine(argc, argv);

  // Get all the parameters.
  string referenceFile = CLI::GetParam<string>("reference_file");

  string distancesFile = CLI::GetParam<string>("distances_file");
  string neighborsFile = CLI::GetParam<string>("neighbors_file");

  int lsInt = CLI::GetParam<int>("leaf_size");

  double max = CLI::GetParam<double>("max");
  double min = CLI::GetParam<double>("min");

  const bool naive = CLI::HasParam("naive");
  const bool singleMode = CLI::HasParam("single_mode");
  bool coverTree = CLI::HasParam("cover_tree");

  arma::mat referenceData;
  arma::mat queryData; // So it doesn't go out of scope.
  if (!data::Load(referenceFile, referenceData))
    Log::Fatal << "Reference file " << referenceFile << "not found." << endl;

  Log::Info << "Loaded reference data from '" << referenceFile << "'." << endl;

  // Sanity check on range value: max must be greater than min.
  if (max <= min)
  {
    Log::Fatal << "Invalid range: maximum (" << max << ") must be greater than "
        << "minimum (" << min << ")." << endl;
  }

  // Sanity check on leaf size.
  if (lsInt < 0)
  {
    Log::Fatal << "Invalid leaf size: " << lsInt << ".  Must be greater "
        "than or equal to 0." << endl;
  }
  size_t leafSize = lsInt;

  // Naive mode overrides single mode.
  if (singleMode && naive)
  {
    Log::Warn << "--single_mode ignored because --naive is present." << endl;
  }

  if (naive)
    leafSize = referenceData.n_cols;

  if (coverTree && naive)
  {
    Log::Warn << "--cover_tree ignored because --naive is present." << endl;
    coverTree = false;
  }

  vector<vector<size_t> > neighbors;
  vector<vector<double> > distances;

  // The cover tree implies different types, so we must split this section.
  if (coverTree)
  {
    Log::Info << "Using cover trees." << endl;

    // This is significantly simpler than kd-tree construction because the data
    // matrix is not modified.
    RSCoverType* rangeSearch = NULL;
    CoverTreeType referenceTree(referenceData);
    CoverTreeType* queryTree = NULL;

    if (CLI::GetParam<string>("query_file") == "")
    {
      // Single dataset.
      rangeSearch = new RSCoverType(&referenceTree, referenceData, singleMode);
    }
    else
    {
      // Two datasets.
      const string queryFile = CLI::GetParam<string>("query_file");
      data::Load(queryFile, queryData, true);
      queryTree = new CoverTreeType(queryData);

      rangeSearch = new RSCoverType(&referenceTree, queryTree, referenceData,
          queryData, singleMode);
    }

    Log::Info << "Trees built." << endl;

    const math::Range r(min, max);
    rangeSearch->Search(r, neighbors, distances);

    if (queryTree)
      delete queryTree;
    delete rangeSearch;
  }
  else
  {
    // Because we may construct it differently, we need a pointer.
    RSType* rangeSearch = NULL;

    // Mappings for when we build the tree.
    vector<size_t> oldFromNewRefs;

    // Build trees by hand, so we can save memory: if we pass a tree to
    // NeighborSearch, it does not copy the matrix.
    Log::Info << "Building reference tree..." << endl;
    Timer::Start("tree_building");

    BinarySpaceTree<bound::HRectBound<2>, RangeSearchStat>
        refTree(referenceData, oldFromNewRefs, leafSize);
    BinarySpaceTree<bound::HRectBound<2>, RangeSearchStat>*
        queryTree = NULL; // Empty for now.

    Timer::Stop("tree_building");

    vector<size_t> oldFromNewQueries;

    if (CLI::GetParam<string>("query_file") != "")
    {
      const string queryFile = CLI::GetParam<string>("query_file");
      data::Load(queryFile, queryData, true);

      if (naive && leafSize < queryData.n_cols)
        leafSize = queryData.n_cols;

      Log::Info << "Loaded query data from '" << queryFile << "'." << endl;

      Log::Info << "Building query tree..." << endl;

      // Build trees by hand, so we can save memory: if we pass a tree to
      // NeighborSearch, it does not copy the matrix.
      Timer::Start("tree_building");

      queryTree = new BinarySpaceTree<bound::HRectBound<2>,
          RangeSearchStat>(queryData, oldFromNewQueries, leafSize);

      Timer::Stop("tree_building");

      rangeSearch = new RSType(&refTree, queryTree, referenceData, queryData,
          singleMode);

      Log::Info << "Tree built." << endl;
    }
    else
    {
      rangeSearch = new RSType(&refTree, referenceData, singleMode);

      Log::Info << "Trees built." << endl;
    }

    Log::Info << "Computing neighbors within range [" << min << ", " << max
        << "]." << endl;

    // Collect the results in these vectors before remapping.
    vector<vector<double> > distancesOut;
    vector<vector<size_t> > neighborsOut;

    const math::Range r(min, max);
    rangeSearch->Search(r, neighborsOut, distancesOut);

    Log::Info << "Neighbors computed." << endl;

    // We have to map back to the original indices from before the tree
    // construction.
    Log::Info << "Re-mapping indices..." << endl;

    distances.resize(distancesOut.size());
    neighbors.resize(neighborsOut.size());

    // Do the actual remapping.
    if (CLI::GetParam<string>("query_file") != "")
    {
      for (size_t i = 0; i < distances.size(); ++i)
      {
        // Map distances (copy a column).
        distances[oldFromNewQueries[i]] = distancesOut[i];

        // Map indices of neighbors.
        neighbors[oldFromNewQueries[i]].resize(neighborsOut[i].size());
        for (size_t j = 0; j < distancesOut[i].size(); ++j)
        {
          neighbors[oldFromNewQueries[i]][j] =
              oldFromNewRefs[neighborsOut[i][j]];
        }
      }
    }
    else
    {
      for (size_t i = 0; i < distances.size(); ++i)
      {
        // Map distances (copy a column).
        distances[oldFromNewRefs[i]] = distancesOut[i];

        // Map indices of neighbors.
        neighbors[oldFromNewRefs[i]].resize(neighborsOut[i].size());
        for (size_t j = 0; j < distancesOut[i].size(); ++j)
        {
          neighbors[oldFromNewRefs[i]][j] = oldFromNewRefs[neighborsOut[i][j]];
        }
      }
    }

    // Clean up.
    if (queryTree)
      delete queryTree;
    delete rangeSearch;
  }

  // Save output.  We have to do this by hand.
  fstream distancesStr(distancesFile.c_str(), fstream::out);
  if (!distancesStr.is_open())
  {
    Log::Warn << "Cannot open file '" << distancesFile << "' to save output "
        << "distances to!" << endl;
  }
  else
  {
    // Loop over each point.
    for (size_t i = 0; i < distances.size(); ++i)
    {
      // Store the distances of each point.  We may have 0 points to store, so
      // we must account for that possibility.
      for (size_t j = 0; j + 1 < distances[i].size(); ++j)
      {
        distancesStr << distances[i][j] << ", ";
      }

      if (distances[i].size() > 0)
        distancesStr << distances[i][distances[i].size() - 1];

      distancesStr << endl;
    }

    distancesStr.close();
  }

  fstream neighborsStr(neighborsFile.c_str(), fstream::out);
  if (!neighborsStr.is_open())
  {
    Log::Warn << "Cannot open file '" << neighborsFile << "' to save output "
        << "neighbor indices to!" << endl;
  }
  else
  {
    // Loop over each point.
    for (size_t i = 0; i < neighbors.size(); ++i)
    {
      // Store the neighbors of each point.  We may have 0 points to store, so
      // we must account for that possibility.
      for (size_t j = 0; j + 1 < neighbors[i].size(); ++j)
      {
        neighborsStr << neighbors[i][j] << ", ";
      }

      if (neighbors[i].size() > 0)
        neighborsStr << neighbors[i][neighbors[i].size() - 1];

      neighborsStr << endl;
    }

    neighborsStr.close();
  }
}
