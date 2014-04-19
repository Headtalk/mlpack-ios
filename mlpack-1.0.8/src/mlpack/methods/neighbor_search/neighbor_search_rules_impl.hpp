/**
 * @file nearest_neighbor_rules_impl.hpp
 * @author Ryan Curtin
 *
 * Implementation of NearestNeighborRules.
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
#ifndef __MLPACK_METHODS_NEIGHBOR_SEARCH_NEAREST_NEIGHBOR_RULES_IMPL_HPP
#define __MLPACK_METHODS_NEIGHBOR_SEARCH_NEAREST_NEIGHBOR_RULES_IMPL_HPP

// In case it hasn't been included yet.
#include "neighbor_search_rules.hpp"

namespace mlpack {
namespace neighbor {

template<typename SortPolicy, typename MetricType, typename TreeType>
NeighborSearchRules<SortPolicy, MetricType, TreeType>::NeighborSearchRules(
    const arma::mat& referenceSet,
    const arma::mat& querySet,
    arma::Mat<size_t>& neighbors,
    arma::mat& distances,
    MetricType& metric) :
    referenceSet(referenceSet),
    querySet(querySet),
    neighbors(neighbors),
    distances(distances),
    metric(metric),
    lastQueryIndex(querySet.n_cols),
    lastReferenceIndex(referenceSet.n_cols)
{ /* Nothing left to do. */ }

template<typename SortPolicy, typename MetricType, typename TreeType>
inline force_inline // Absolutely MUST be inline so optimizations can happen.
double NeighborSearchRules<SortPolicy, MetricType, TreeType>::
BaseCase(const size_t queryIndex, const size_t referenceIndex)
{
  // If the datasets are the same, then this search is only using one dataset
  // and we should not return identical points.
  if ((&querySet == &referenceSet) && (queryIndex == referenceIndex))
    return 0.0;

  // If we have already performed this base case, then do not perform it again.
  if ((lastQueryIndex == queryIndex) && (lastReferenceIndex == referenceIndex))
    return lastBaseCase;

  double distance = metric.Evaluate(querySet.unsafe_col(queryIndex),
                                    referenceSet.unsafe_col(referenceIndex));

  // If this distance is better than any of the current candidates, the
  // SortDistance() function will give us the position to insert it into.
  arma::vec queryDist = distances.unsafe_col(queryIndex);
  const size_t insertPosition = SortPolicy::SortDistance(queryDist, distance);

  // SortDistance() returns (size_t() - 1) if we shouldn't add it.
  if (insertPosition != (size_t() - 1))
    InsertNeighbor(queryIndex, insertPosition, referenceIndex, distance);

  // Cache this information for the next time BaseCase() is called.
  lastQueryIndex = queryIndex;
  lastReferenceIndex = referenceIndex;
  lastBaseCase = distance;

  return distance;
}

template<typename SortPolicy, typename MetricType, typename TreeType>
inline double NeighborSearchRules<SortPolicy, MetricType, TreeType>::Score(
    const size_t queryIndex,
    TreeType& referenceNode)
{
  double distance;
  if (tree::TreeTraits<TreeType>::FirstPointIsCentroid)
  {
    // The first point in the tree is the centroid.  So we can then calculate
    // the base case between that and the query point.
    double baseCase;
    if (tree::TreeTraits<TreeType>::HasSelfChildren)
    {
      // If the parent node is the same, then we have already calculated the
      // base case.
      if ((referenceNode.Parent() != NULL) &&
          (referenceNode.Point(0) == referenceNode.Parent()->Point(0)))
        baseCase = referenceNode.Parent()->Stat().LastDistance();
      else
        baseCase = BaseCase(queryIndex, referenceNode.Point(0));

      // Save this evaluation.
      referenceNode.Stat().LastDistance() = baseCase;
    }

    distance = SortPolicy::CombineBest(baseCase,
        referenceNode.FurthestDescendantDistance());
  }
  else
  {
    const arma::vec queryPoint = querySet.unsafe_col(queryIndex);
    distance = SortPolicy::BestPointToNodeDistance(queryPoint, &referenceNode);
  }

  // Compare against the best k'th distance for this query point so far.
  const double bestDistance = distances(distances.n_rows - 1, queryIndex);

  return (SortPolicy::IsBetter(distance, bestDistance)) ? distance : DBL_MAX;
}

template<typename SortPolicy, typename MetricType, typename TreeType>
inline double NeighborSearchRules<SortPolicy, MetricType, TreeType>::Rescore(
    const size_t queryIndex,
    TreeType& /* referenceNode */,
    const double oldScore) const
{
  // If we are already pruning, still prune.
  if (oldScore == DBL_MAX)
    return oldScore;

  // Just check the score again against the distances.
  const double bestDistance = distances(distances.n_rows - 1, queryIndex);

  return (SortPolicy::IsBetter(oldScore, bestDistance)) ? oldScore : DBL_MAX;
}

template<typename SortPolicy, typename MetricType, typename TreeType>
inline double NeighborSearchRules<SortPolicy, MetricType, TreeType>::Score(
    TreeType& queryNode,
    TreeType& referenceNode)
{
  double distance;
  if (tree::TreeTraits<TreeType>::FirstPointIsCentroid)
  {
    // The first point in the node is the centroid, so we can calculate the
    // distance between the two points using BaseCase() and then find the
    // bounds.  This is potentially loose for non-ball bounds.
    bool alreadyDone = false;
    double baseCase;
    if (tree::TreeTraits<TreeType>::HasSelfChildren)
    {
      // In this case, we may have already calculated the base case.
      TreeType* lastRef = (TreeType*) queryNode.Stat().LastDistanceNode();
      TreeType* lastQuery = (TreeType*) referenceNode.Stat().LastDistanceNode();

      // Does the query node have the base case cached?
      if ((lastRef != NULL) && (referenceNode.Point(0) == lastRef->Point(0)))
      {
        baseCase = queryNode.Stat().LastDistance();
        alreadyDone = true;
      }

      // Does the reference node have the base case cached?
      if ((lastQuery != NULL) &&
          (queryNode.Point(0) == lastQuery->Point(0)))
      {
        baseCase = referenceNode.Stat().LastDistance();
        alreadyDone = true;
      }

      // Is the query node a self-child, and if so, does the query node's parent
      // have the base case cached?
      if ((queryNode.Parent() != NULL) &&
          (queryNode.Parent()->Point(0) == queryNode.Point(0)))
      {
        TreeType* lastParentRef = (TreeType*)
            queryNode.Parent()->Stat().LastDistanceNode();
        if (lastParentRef->Point(0) == referenceNode.Point(0))
        {
          baseCase = queryNode.Parent()->Stat().LastDistance();
          alreadyDone = true;
        }
      }

      // Is the reference node a self-child, and if so, does the reference
      // node's parent have the base case cached?
      if ((referenceNode.Parent() != NULL) &&
          (referenceNode.Parent()->Point(0) == referenceNode.Point(0)))
      {
        TreeType* lastParentRef = (TreeType*)
            referenceNode.Parent()->Stat().LastDistanceNode();
        if (lastParentRef->Point(0) == queryNode.Point(0))
        {
          baseCase = referenceNode.Parent()->Stat().LastDistance();
          alreadyDone = true;
        }
      }
    }

    // If we did not find a cached base case, then recalculate it.
    if (!alreadyDone)
    {
      baseCase = BaseCase(queryNode.Point(0), referenceNode.Point(0));
    }
    else
    {
      // Set lastQueryIndex and lastReferenceIndex, so that BaseCase() does not
      // duplicate work.
      lastQueryIndex = queryNode.Point(0);
      lastReferenceIndex = referenceNode.Point(0);
      lastBaseCase = baseCase;
    }

    distance = SortPolicy::CombineBest(baseCase,
        queryNode.FurthestDescendantDistance() +
        referenceNode.FurthestDescendantDistance());

    // Update the last distance calculation for the query and reference nodes.
    queryNode.Stat().LastDistanceNode() = (void*) &referenceNode;
    queryNode.Stat().LastDistance() = baseCase;
    referenceNode.Stat().LastDistanceNode() = (void*) &queryNode;
    referenceNode.Stat().LastDistance() = baseCase;
  }
  else
  {
    distance = SortPolicy::BestNodeToNodeDistance(&queryNode, &referenceNode);
  }

  // Update our bound.
  const double bestDistance = CalculateBound(queryNode);

  return (SortPolicy::IsBetter(distance, bestDistance)) ? distance : DBL_MAX;
}

template<typename SortPolicy, typename MetricType, typename TreeType>
inline double NeighborSearchRules<SortPolicy, MetricType, TreeType>::Rescore(
    TreeType& queryNode,
    TreeType& /* referenceNode */,
    const double oldScore) const
{
  if (oldScore == DBL_MAX)
    return oldScore;

  // Update our bound.
  const double bestDistance = CalculateBound(queryNode);

  return (SortPolicy::IsBetter(oldScore, bestDistance)) ? oldScore : DBL_MAX;
}

// Calculate the bound for a given query node in its current state and update
// it.
template<typename SortPolicy, typename MetricType, typename TreeType>
inline double NeighborSearchRules<SortPolicy, MetricType, TreeType>::
    CalculateBound(TreeType& queryNode) const
{
  // We have five possible bounds, and we must take the best of them all.  We
  // don't use min/max here, but instead "best/worst", because this is general
  // to the nearest-neighbors/furthest-neighbors cases.  For nearest neighbors,
  // min = best, max = worst.
  //
  // (1) worst ( worst_{all points p in queryNode} D_p[k],
  //             worst_{all children c in queryNode} B(c) );
  // (2) best_{all points p in queryNode} D_p[k] + worst child distance +
  //        worst descendant distance;
  // (3) best_{all children c in queryNode} B(c) +
  //      2 ( worst descendant distance of queryNode -
  //          worst descendant distance of c );
  // (4) B_1(parent of queryNode)
  // (5) B_2(parent of queryNode);
  //
  // D_p[k] is the current k'th candidate distance for point p.
  // So we will loop over the points in queryNode and the children in queryNode
  // to calculate all five of these quantities.

  double worstPointDistance = SortPolicy::BestDistance();
  double bestPointDistance = SortPolicy::WorstDistance();

  // Loop over all points in this node to find the best and worst distance
  // candidates (for (1) and (2)).
  for (size_t i = 0; i < queryNode.NumPoints(); ++i)
  {
    const double distance = distances(distances.n_rows - 1, queryNode.Point(i));
    if (SortPolicy::IsBetter(distance, bestPointDistance))
      bestPointDistance = distance;
    if (SortPolicy::IsBetter(worstPointDistance, distance))
      worstPointDistance = distance;
  }

  // Loop over all the children in this node to find the worst bound (for (1))
  // and the best bound with the correcting factor for descendant distances (for
  // (3)).
  double worstChildBound = SortPolicy::BestDistance();
  double bestAdjustedChildBound = SortPolicy::WorstDistance();
  const double queryMaxDescendantDistance =
      queryNode.FurthestDescendantDistance();

  for (size_t i = 0; i < queryNode.NumChildren(); ++i)
  {
    const double firstBound = queryNode.Child(i).Stat().FirstBound();
    const double secondBound = queryNode.Child(i).Stat().SecondBound();
    const double childMaxDescendantDistance =
        queryNode.Child(i).FurthestDescendantDistance();

    if (SortPolicy::IsBetter(worstChildBound, firstBound))
      worstChildBound = firstBound;

    // Now calculate adjustment for maximum descendant distances.
    const double adjustedBound = SortPolicy::CombineWorst(secondBound,
        2 * (queryMaxDescendantDistance - childMaxDescendantDistance));
    if (SortPolicy::IsBetter(adjustedBound, bestAdjustedChildBound))
      bestAdjustedChildBound = adjustedBound;
  }

  // This is bound (1).
  const double firstBound =
      (SortPolicy::IsBetter(worstPointDistance, worstChildBound)) ?
      worstChildBound : worstPointDistance;

  // This is bound (2).
  const double secondBound = SortPolicy::CombineWorst(bestPointDistance,
      2 * queryMaxDescendantDistance);

  // Bound (3) is bestAdjustedChildBound.

  // Bounds (4) and (5) are the parent bounds.
  const double fourthBound = (queryNode.Parent() != NULL) ?
      queryNode.Parent()->Stat().FirstBound() : SortPolicy::WorstDistance();
  const double fifthBound = (queryNode.Parent() != NULL) ?
      queryNode.Parent()->Stat().SecondBound() : SortPolicy::WorstDistance();

  // Now, we will take the best of these.  Unfortunately due to the way
  // IsBetter() is defined, this sort of has to be a little ugly.
  // The variable interA represents the first bound (B_1), which is the worst
  // candidate distance of any descendants of this node.
  // The variable interC represents the second bound (B_2), which is a bound on
  // the worst distance of any descendants of this node assembled using the best
  // descendant candidate distance modified using the furthest descendant
  // distance.
  const double interA = (SortPolicy::IsBetter(firstBound, fourthBound)) ?
      firstBound : fourthBound;
  const double interB =
      (SortPolicy::IsBetter(bestAdjustedChildBound, secondBound)) ?
      bestAdjustedChildBound : secondBound;
  const double interC = (SortPolicy::IsBetter(interB, fifthBound)) ? interB :
      fifthBound;

  // Update the first and second bounds of the node.
  queryNode.Stat().FirstBound() = interA;
  queryNode.Stat().SecondBound() = interC;

  // Update the actual bound of the node.
  queryNode.Stat().Bound() = (SortPolicy::IsBetter(interA, interC)) ? interA :
      interC;

  return queryNode.Stat().Bound();
}

/**
 * Helper function to insert a point into the neighbors and distances matrices.
 *
 * @param queryIndex Index of point whose neighbors we are inserting into.
 * @param pos Position in list to insert into.
 * @param neighbor Index of reference point which is being inserted.
 * @param distance Distance from query point to reference point.
 */
template<typename SortPolicy, typename MetricType, typename TreeType>
void NeighborSearchRules<SortPolicy, MetricType, TreeType>::InsertNeighbor(
    const size_t queryIndex,
    const size_t pos,
    const size_t neighbor,
    const double distance)
{
  // We only memmove() if there is actually a need to shift something.
  if (pos < (distances.n_rows - 1))
  {
    int len = (distances.n_rows - 1) - pos;
    memmove(distances.colptr(queryIndex) + (pos + 1),
        distances.colptr(queryIndex) + pos,
        sizeof(double) * len);
    memmove(neighbors.colptr(queryIndex) + (pos + 1),
        neighbors.colptr(queryIndex) + pos,
        sizeof(size_t) * len);
  }

  // Now put the new information in the right index.
  distances(pos, queryIndex) = distance;
  neighbors(pos, queryIndex) = neighbor;
}

}; // namespace neighbor
}; // namespace mlpack

#endif // __MLPACK_METHODS_NEIGHBOR_SEARCH_NEAREST_NEIGHBOR_RULES_IMPL_HPP
