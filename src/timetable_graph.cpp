/*
 * timetable_graph.cpp
 *
 *  Created on: 30 août 2018
 *      Author: theo
 */

#include "stdafx.h"
#include "timetable_graph.h"
#include <set>

/**
 *
 */
TimetableGraphBuilder::TimetableGraphBuilder(const OrderList* baseOrders)
	: baseOrders(baseOrders)
{
}

void TimetableGraphBuilder::SetBaseOrderList(const OrderList* baseOrders) {
	this->baseOrders = baseOrders;
	destinations.clear();
	destinationsIndex.clear();
}

GraphLine TimetableGraphBuilder::BuildGraph()
{
	this->BuildDestinationsIndex();
	return destinations;
}

GraphLine TimetableGraphBuilder::GetGraphForOrderList(const OrderList* orders)
{
	std::vector<GraphSegment> graph;

	for (GotoOrderListIterator compIt(orders); !compIt.IsRepeating(); ++compIt) {

		std::pair<DestIndexIterator, DestIndexIterator> pair = destinationsIndex.equal_range(compIt->GetDestination());
		for (DestIndexIterator baseIt = pair.first; baseIt != pair.second; ++baseIt) {

			GraphSegment segment = BuildGraphLine(orders, compIt, GotoOrderListIterator(baseOrders, baseIt->second.first), baseIt->second.second);
			if (segment.order1 != NULL) {
				graph.push_back(segment);
			}
		}

	}

	return graph;
}

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
			baseEnded = true;
		}
		if (!compEnded && visitedComp.count(compIt->GetDestination()) != 0) {
			compEnded = true;
		}
		if (compEnded && baseEnded) return GraphSegment();

		if (!baseEnded && !compEnded) {
			if (baseIt->GetDestination() == compIt->GetDestination()) {
				GraphSegment segment(*compItStart, *compIt, baseStartIndex, currentBaseIndex);
				if (compIt.HasPassedEnd()) {
					segment.offset2 = orderList->GetTimetableDuration().GetLengthAsDate();
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
						segment.offset2 = orderList->GetTimetableDuration().GetLengthAsDate();
					}
					return segment;
				}
			}

			++compIt;
			visitedComp.insert(compIt->GetDestination());
		}

		if (!baseEnded) {
			for (GotoOrderListIterator it = compItStart; it != compIt; ++it) {

				if (it->GetDestination() == baseIt->GetDestination()) {
					GraphSegment segment(*compItStart, *it, baseStartIndex, currentBaseIndex);
					if (it.HasPassedEnd()) {
						segment.offset2 = orderList->GetTimetableDuration().GetLengthAsDate();
					}
					return segment;
				}
			}
			++baseIt;
			++currentBaseIndex;
			visitedBase.insert(baseIt->GetDestination());
		}


	}

}


void TimetableGraphBuilder::BuildDestinationsIndex()
{
	int i = 0;
	GotoOrderListIterator orderIt1(baseOrders), orderIt2(baseOrders);
	++orderIt2;
	while (!orderIt1.IsRepeating()) {
		//If we are adding the last segment (from the last order to the first, we add an offset
		destinations.push_back(GraphSegment(*orderIt1, *orderIt2, i, i+1, 0,
				orderIt2.IsRepeating() ? baseOrders->GetTimetableDuration().GetLengthAsDate() : 0));

		destinationsIndex.insert(std::pair<Destination, BasePair>(orderIt1->GetDestination(), BasePair(*orderIt1, i)));
		++i;
		++orderIt1;
		++orderIt2;
	}//FIXME What happens with an orderlist with 1 order ?
	/*
	if (i>= 2) {
		destinationsIndex.insert(std::pair<Destination, BasePair>(baseOrders->GetFirstOrder()->GetDestination(), BasePair(baseOrders->GetFirstOrder(), i)));
	}*/
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
