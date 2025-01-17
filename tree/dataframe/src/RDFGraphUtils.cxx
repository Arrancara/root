// Author: Enrico Guiraud, Danilo Piparo, CERN, Massimo Tumolo Politecnico di Torino 08/2018

/*************************************************************************
 * Copyright (C) 1995-2020, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#include "ROOT/RDF/RColumnRegister.hxx"
#include "ROOT/RDF/GraphUtils.hxx"

#include <algorithm> // std::find

namespace ROOT {
namespace Internal {
namespace RDF {
namespace GraphDrawing {

std::string GraphCreatorHelper::FromGraphLeafToDot(std::shared_ptr<GraphNode> leaf)
{
   // Only the mapping between node id and node label (i.e. name)
   std::stringstream dotStringLabels;
   // Representation of the relationships between nodes
   std::stringstream dotStringGraph;

   // Explore the graph bottom-up and store its dot representation.
   while (leaf) {
      dotStringLabels << "\t" << leaf->fCounter << " [label=\"" << leaf->fName << "\", style=\"filled\", fillcolor=\""
                      << leaf->fColor << "\", shape=\"" << leaf->fShape << "\"];\n";
      if (leaf->fPrevNode) {
         dotStringGraph << "\t" << leaf->fPrevNode->fCounter << " -> " << leaf->fCounter << ";\n";
      }
      leaf = leaf->fPrevNode;
   }

   return "digraph {\n" + dotStringLabels.str() + dotStringGraph.str() + "}";
}

std::string GraphCreatorHelper::FromGraphActionsToDot(std::vector<std::shared_ptr<GraphNode>> leaves)
{
   // Only the mapping between node id and node label (i.e. name)
   std::stringstream dotStringLabels;
   // Representation of the relationships between nodes
   std::stringstream dotStringGraph;

   for (auto leaf : leaves) {
      while (leaf && !leaf->fIsExplored) {
         dotStringLabels << "\t" << leaf->fCounter << " [label=\"" << leaf->fName
                         << "\", style=\"filled\", fillcolor=\"" << leaf->fColor << "\", shape=\"" << leaf->fShape
                         << "\"];\n";
         if (leaf->fPrevNode) {
            dotStringGraph << "\t" << leaf->fPrevNode->fCounter << " -> " << leaf->fCounter << ";\n";
         }
         // Multiple branches may share the same nodes. It is wrong to explore them more than once.
         leaf->fIsExplored = true;
         leaf = leaf->fPrevNode;
      }
   }
   return "digraph {\n" + dotStringLabels.str() + dotStringGraph.str() + "}";
}

std::string GraphCreatorHelper::RepresentGraph(ROOT::RDataFrame &rDataFrame)
{
   auto loopManager = rDataFrame.GetLoopManager();
   // Jitting is triggered because nodes must not be empty at the time of the calling in order to draw the graph.
   loopManager->Jit();

   return RepresentGraph(loopManager);
}

std::string GraphCreatorHelper::RepresentGraph(RLoopManager *loopManager)
{
   const auto actions = loopManager->GetAllActions();
   const auto edges = loopManager->GetGraphEdges();

   std::vector<std::shared_ptr<GraphNode>> nodes;
   nodes.reserve(actions.size() + edges.size());

   for (auto *action : actions)
      nodes.emplace_back(action->GetGraph(fVisitedMap));
   for (auto *edge : edges)
      nodes.emplace_back(edge->GetGraph(fVisitedMap));

   return FromGraphActionsToDot(nodes);
}

std::shared_ptr<GraphNode> CreateDefineNode(const std::string &columnName,
                                            const ROOT::Detail::RDF::RDefineBase *columnPtr,
                                            std::unordered_map<void *, std::shared_ptr<GraphNode>> &visitedMap)
{
   // If there is already a node for this define (recognized by the custom column it is defining) return it. If there is
   // not, return a new one.
   auto duplicateDefineIt = visitedMap.find((void *)columnPtr);
   if (duplicateDefineIt != visitedMap.end())
      return duplicateDefineIt->second;

   auto node = std::make_shared<GraphNode>("Define\n" + columnName);
   node->SetDefine();
   node->SetCounter(visitedMap.size());
   visitedMap[(void *)columnPtr] = node;
   return node;
}

std::shared_ptr<GraphNode> CreateFilterNode(const ROOT::Detail::RDF::RFilterBase *filterPtr,
                                            std::unordered_map<void *, std::shared_ptr<GraphNode>> &visitedMap)
{
   // If there is already a node for this filter return it. If there is not, return a new one.
   auto duplicateFilterIt = visitedMap.find((void *)filterPtr);
   if (duplicateFilterIt != visitedMap.end()) {
      duplicateFilterIt->second->SetIsNew(false);
      return duplicateFilterIt->second;
   }

   auto node = std::make_shared<GraphNode>((filterPtr->HasName() ? filterPtr->GetName() : "Filter"));
   node->SetFilter();
   node->SetCounter(visitedMap.size());
   visitedMap[(void *)filterPtr] = node;
   return node;
}

std::shared_ptr<GraphNode> CreateRangeNode(const ROOT::Detail::RDF::RRangeBase *rangePtr,
                                           std::unordered_map<void *, std::shared_ptr<GraphNode>> &visitedMap)
{
   // If there is already a node for this range return it. If there is not, return a new one.
   auto duplicateRangeIt = visitedMap.find((void *)rangePtr);
   if (duplicateRangeIt != visitedMap.end()) {
      duplicateRangeIt->second->SetIsNew(false);
      return duplicateRangeIt->second;
   }

   auto node = std::make_shared<GraphNode>("Range");
   node->SetRange();
   node->SetCounter(visitedMap.size());
   visitedMap[(void *)rangePtr] = node;
   return node;
}

std::shared_ptr<GraphNode> AddDefinesToGraph(std::shared_ptr<GraphNode> node, const RColumnRegister &colRegister,
                                             const std::vector<std::string> &prevNodeDefines,
                                             std::unordered_map<void *, std::shared_ptr<GraphNode>> &visitedMap)
{
   auto upmostNode = node;
   const auto &defineNames = colRegister.GetNames();
   const auto &defineMap = colRegister.GetColumns();
   for (auto i = int(defineNames.size()) - 1; i >= 0; --i) { // walk backwards through the names of defined columns
      const auto colName = defineNames[i];
      const bool isAlias = defineMap.find(colName) == defineMap.end();
      if (isAlias || IsInternalColumn(colName))
         continue; // aliases appear in the list of defineNames but we don't support them yet
      const bool isANewDefine =
         std::find(prevNodeDefines.begin(), prevNodeDefines.end(), colName) == prevNodeDefines.end();
      if (!isANewDefine)
         break; // we walked back through all new defines, the rest is stuff that was already in the graph

      // create a node for this new Define
      auto defineNode = RDFGraphDrawing::CreateDefineNode(colName, defineMap.at(colName).get(), visitedMap);
      upmostNode->SetPrevNode(defineNode);
      upmostNode = defineNode;
   }

   return upmostNode;
}

} // namespace GraphDrawing
} // namespace RDF
} // namespace Internal
} // namespace ROOT
