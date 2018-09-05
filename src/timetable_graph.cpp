/*
 * timetable_graph.cpp
 *
 *  Created on: 30 août 2018
 *      Author: theo
 */

#include "stdafx.h"
#include "timetable_graph.h"

TimetableGraphBuilder::TimetableGraphBuilder(const OrderList* baseOrders)
	: baseOrders(baseOrders)
{
}

void TimetableGraphBuilder::SetBaseOrderList(const OrderList* baseOrders) {
	this->baseOrders = baseOrders;
}

GraphLine TimetableGraphBuilder::BuildGraph()
{
	this->BuildDestinationsIndex();
	return destinations;
}

GraphLine TimetableGraphBuilder::GetGraphForOrderList(const OrderList* orders)
{
	orderIt = GotoOrderListIterator(orders);
	baseOrderIt = GotoOrderListIterator(baseOrders);
	return std::vector<GraphSegment>();//TODO

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

		destinationsIndex.insert(std::pair<Destination, int>(orderIt1->GetDestination(), i));
		++i;
		++orderIt1;
		++orderIt2;
	}//FIXME What happens with an orderlist with 1 order ?

	if (i>= 2) { //
		destinationsIndex.insert(std::pair<Destination, int>(baseOrders->GetFirstOrder()->GetDestination(), i));
	}
}





GotoOrderListIterator::GotoOrderListIterator(const OrderList* orderList, const Order* order)
	: orderList(orderList), current(orderList != NULL ? orderList->GetFirstOrder() : NULL), counter(0)
{
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
		current = orderList->GetNext(current);
		++internalCounter;
		if (incrCounter) ++counter;
	} while (internalCounter < orderList->GetNumOrders() && !current->IsGotoOrder());
}
