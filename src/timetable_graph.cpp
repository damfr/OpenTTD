/*
 * timetable_graph.cpp
 *
 *  Created on: 30 août 2018
 *      Author: theo
 */

#include "stdafx.h"
#include "timetable_graph.h"
#include "vehicle_base.h"

/**
 *
 */
TimetableGraphBuilder::TimetableGraphBuilder(const OrderList* baseOrders)
	: baseOrders(baseOrders)
{
}

void TimetableGraphBuilder::SetBaseOrderList(const OrderList* baseOrders) {
	this->baseOrders = baseOrders;
	mainGraphLine.orderList = baseOrders;
	mainGraphLine.segments.clear();
	mainGraphLine.offsets.clear();
	destinationsIndex.clear();
}

/**
 * Build a GraphLine from the current OrderList.
 * Includes all offsets from vehicles using this OrderList
 * @return a GraphLine
 */
GraphLine TimetableGraphBuilder::BuildGraph()
{
	this->BuildDestinationsIndex();
	if (!mainGraphLine.segments.empty()) {	//No need to continue if we aren't going to draw anything
		//Building the offsets vector
		this->mainGraphLine.offsets = this->GetOrderListOffsets(this->baseOrders);
	}
	return mainGraphLine;
}

/**
 * Builds a GraphLine for another OrderList, keeping only relevant times between stations present in the base OrderList
 * @param orders An OrderList to build from
 * @return A GraphLine containing segments from the given OrderList, to draw in addition to the default GraphLine (@see TimetableGraphBuilder::BuildGraph())
 */
GraphLine TimetableGraphBuilder::GetGraphForOrderList(const OrderList* orders)
{
	GraphLine line;
	
	for (GotoOrderListIterator compIt(orders); !compIt.IsRepeating(); ++compIt) {
		/* First we iterate over all ordres of the new OrderList to check for stations already present in baseOrderList */
		std::pair<DestIndexIterator, DestIndexIterator> pair = destinationsIndex.equal_range(compIt->GetDestination());
		for (DestIndexIterator baseIt = pair.first; baseIt != pair.second; ++baseIt) {
			/* Then for all stations in the baseOrderList mathcing the current station, we try to build a segment*/
			GraphSegment segment = BuildGraphLine(orders, compIt, GotoOrderListIterator(baseOrders, baseIt->second.first), baseIt->second.second);
			if (segment.order1 != NULL) {
				line.segments.push_back(segment);
			}
		}

	}

	if (!line.segments.empty()) {	//No need to continue if we aren't going to draw anything
		//Building the offsets vector
		line.offsets = this->GetOrderListOffsets(orders);
	}

	line.orderList = orders;

	return line;
}

/**
 * Try to build a graph segment
 * @param orderList The OrderList to build the graph segment from
 * @param compItStart the iterator in orderList that points to the first point of the segment we search
 * @param baseItStart the iterator in baseOrderList that points to the first point of the segment we search (same destination ad compItStart)
 * @param baseStartIndex the index in the baseOrderList of baseItStart
 * @return If found, a GrpahSegment from orderList to draw. If not found, returns GraphSegment()
 */
GraphSegment TimetableGraphBuilder::BuildGraphLine(const OrderList* orderList, GotoOrderListIterator compItStart, GotoOrderListIterator baseItStart, int baseStartIndex)
{
	std::set<Destination> visitedBase, visitedComp;
	visitedBase.insert(baseItStart->GetDestination());
	visitedComp.insert(baseItStart->GetDestination());
	int currentBaseIndex = baseStartIndex +1;
	bool baseEnded = false, compEnded = false;

	GotoOrderListIterator compIt = compItStart;
	++compIt;
	GotoOrderListIterator baseIt = baseItStart;
	++baseIt;

	for (;;) {
		if (!baseEnded && visitedBase.count(baseIt->GetDestination()) != 0) {
			/* baseIt reached a destination already encountered : don't advance baseIt anymore */
			baseEnded = true;
		}
		if (!compEnded && visitedComp.count(compIt->GetDestination()) != 0) {
			/* compIt reached a destination already encountered : don't advance compIt anymore */
			compEnded = true;
		}
		if (compEnded && baseEnded) return GraphSegment();

		if (!baseEnded && !compEnded) {
			if (baseIt->GetDestination() == compIt->GetDestination()) {
				GraphSegment segment(*compItStart, *compIt, baseStartIndex, currentBaseIndex);
				if (compIt.HasPassedEnd()) {
					segment.offset2 = orderList->GetTimetableDuration();
				}
				return segment;
			}
		}

		if (!compEnded) {
			int index = baseStartIndex;
			for (GotoOrderListIterator it = baseItStart; it != baseIt; ++it, ++index) {

				if (it->GetDestination() == compIt->GetDestination()) {
					GraphSegment segment(*compItStart, *compIt, baseStartIndex, index);
					if (compIt.HasPassedEnd()) {
						segment.offset2 = orderList->GetTimetableDuration();
					}
					return segment;
				}
			}
			visitedComp.insert(compIt->GetDestination());
			++compIt;

		}

		if (!baseEnded) {
			for (GotoOrderListIterator it = compItStart; it != compIt; ++it) {

				if (it->GetDestination() == baseIt->GetDestination()) {
					GraphSegment segment(*compItStart, *it, baseStartIndex, currentBaseIndex);
					if (it.HasPassedEnd()) {
						segment.offset2 = orderList->GetTimetableDuration();
					}
					return segment;
				}
			}
			visitedBase.insert(baseIt->GetDestination());
			++baseIt;
			++currentBaseIndex;

		}


	}

}

/**
 * Build the mainGraphLine and the destinationIndex
 */
void TimetableGraphBuilder::BuildDestinationsIndex()
{
	int i = 0;
	GotoOrderListIterator orderIt1(baseOrders), orderIt2(baseOrders);
	++orderIt2;
	while (!orderIt1.IsRepeating()) {
		//If we are adding the last segment (from the last order to the first, we add an offset
		mainGraphLine.segments.push_back(GraphSegment(*orderIt1, *orderIt2, i, i+1, Duration(0, DU_DAYS),
				orderIt2.IsRepeating() ? baseOrders->GetTimetableDuration() : Duration(0, DU_DAYS)));

		destinationsIndex.insert(std::pair<Destination, BasePair>(orderIt1->GetDestination(), BasePair(*orderIt1, i)));
		++i;
		++orderIt1;
		++orderIt2;
	}
	
	//FIXME What happens with an orderlist with 1 order ?
}

/**
 * Builds a set of Duration representing all the offsets of the given OrderList
 * @param orderList the order list to get the offsets from
 * @return A set containing all the durations of the offsets
 */
std::set<Duration> TimetableGraphBuilder::GetOrderListOffsets(const OrderList* orderList) const
{
	std::set<Duration> offsets;
	for (const Vehicle* v = orderList->GetFirstSharedVehicle(); v != nullptr; v = v->NextShared()) {
		if (!v->GetTimetableOffset().IsInvalid()) {
			offsets.insert(v->GetTimetableOffset());
		}
	}
	return offsets;
}



GotoOrderListIterator::GotoOrderListIterator(const OrderList* orderList, const Order* order)
	: orderList(orderList), current(order), counter(0), passedEnd(false)
{
	if (orderList != NULL && current == NULL) current = orderList->GetFirstOrder();
	if (orderList != NULL && !current->IsGotoOrder()) this->AdvanceToNextGoto(false);
}



GotoOrderListIterator& GotoOrderListIterator::operator++()
{
	this->AdvanceToNextGoto(true);
	return *this;
}

void GotoOrderListIterator::AdvanceToNextGoto(bool incrCounter) {
	int internalCounter = 0; //prevent infinite loops when there is no goto order
	do {
		if (current->next == NULL) {
			current = orderList->GetFirstOrder();
			passedEnd = true;
		} else {
			current = current->next;
		}
		++internalCounter;
		if (incrCounter) ++counter;
	} while (internalCounter < orderList->GetNumOrders() && !current->IsGotoOrder());
}

