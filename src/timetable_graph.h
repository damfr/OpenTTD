/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file timetable_graph.h Back-end for the timetable graph */

#ifndef TIMETABLE_GRAPH_H
#define TIMETABLE_GRAPH_H

#include <vector>
#include <map>
#include "order_base.h"


class GotoOrderListIterator {
public:
	GotoOrderListIterator(const OrderList* = NULL, const Order* = NULL);

	inline const Order* operator -> () const { return current; };
	inline const Order* operator*() const { return current; };

	GotoOrderListIterator& operator++(); //infix

	/**
	 * Check wether the iterator is on an order which has already been reached previously (having iterated over all previous orders)
	 * @return true if the order has already been iterated over.
	 */
	bool IsRepeating() const { return counter >= orderList->GetNumOrders(); }

	bool HasPassedEnd() const { return passedEnd; }

	bool operator!=(const GotoOrderListIterator& other) { return orderList != other.orderList || current != other.current; }

private:
	const OrderList* orderList;
	const Order* current;
	int counter;
	bool passedEnd;

	void AdvanceToNextGoto(bool incrCounter);
};


struct GraphSegment {
	const Order* order1;
	int index1; //The index (y axis)
	int offset1; //Offset in days

	const Order* order2;
	int index2; //The index (y axis)
	int offset2; //Offset in days

	GraphSegment(const Order* order1 = NULL, const Order* order2 = NULL, int index1 = -1, int index2 = -1, int offset1 = 0, int offset2 = 0)
		: order1(order1), index1(index1), offset1(offset1), order2(order2), index2(index2), offset2(offset2) {}
};

typedef std::vector<GraphSegment> GraphLine;

class TimetableGraphBuilder {
public:
	TimetableGraphBuilder(const OrderList* baseOrders = NULL);

	GraphLine BuildGraph();

	typedef DestinationID Destination;

	GraphLine GetGraphForOrderList(const OrderList* orders);

	void SetBaseOrderList(const OrderList* baseOrders);

private:
	typedef std::pair<const Order*, int> BasePair;

	const OrderList* baseOrders;

	/**
	 * A map : keys = (destination, index) (index being the index on the y axis)
	 * values = wether we have already considered the associated key in the algorithm
	 */
	//std::map<BasePair, bool> destinationsIndex;
	std::multimap<Destination, BasePair> destinationsIndex;

	typedef std::multimap<Destination, BasePair>::iterator DestIndexIterator;

	GraphLine destinations;


	void BuildDestinationsIndex();

	GraphSegment BuildGraphLine(const OrderList* orderList, GotoOrderListIterator compItStart, GotoOrderListIterator baseItStart, int baseStartIndex);

};



#endif /* TIMETABLE_GRAPH_H */
