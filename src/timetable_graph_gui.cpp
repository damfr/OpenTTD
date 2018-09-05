/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file timetable_graph_gui.cpp GUI for timetable graph */

#include "stdafx.h"
#include "strings_func.h"
#include "gfx_func.h"
#include "timetable_graph_gui.h"
#include "window_type.h"
#include "window_gui.h"
#include "vehiclelist.h"
#include "vehicle_base.h"
#include "widgets/timetable_widget.h"
#include "core/geometry_func.hpp"
#include "timetable_graph.h"


static const NWidgetPart _nested_timetable_graph[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_TGW_CAPTION), SetDataTip(STR_TIMETABLE_GRAPH_CAPTION, STR_NULL),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_DEFSIZEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),

	NWidget(WWT_PANEL, COLOUR_GREY/*, WID_TGW_BACKGROUND*/),
		NWidget(NWID_HORIZONTAL), SetPadding(10, 0, 0, 10),//FIXME
			NWidget(WWT_EMPTY, COLOUR_GREY, WID_TGW_GRAPH), SetMinimalSize(576, 160), SetFill(0, 1), SetResize(1, 1),
			NWidget(NWID_VERTICAL),
				NWidget(NWID_SPACER), SetFill(0, 1), SetResize(0, 1),
				NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, WID_TGW_ENABLE_ALL), SetDataTip(STR_GRAPH_CARGO_ENABLE_ALL, STR_GRAPH_CARGO_TOOLTIP_ENABLE_ALL), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, WID_TGW_DISABLE_ALL), SetDataTip(STR_GRAPH_CARGO_DISABLE_ALL, STR_GRAPH_CARGO_TOOLTIP_DISABLE_ALL), SetFill(1, 0),
				NWidget(NWID_SPACER), SetFill(0, 1), SetResize(0, 1),
				NWidget(NWID_VERTICAL, COLOUR_GREY, WID_TGW_ORDERS_SELECTION), SetFill(1,0), SetResize(1,1),
					//Added on construction of the window
				EndContainer(),
				NWidget(NWID_SPACER), SetFill(0, 1), SetResize(0, 1),
			EndContainer(),
		EndContainer(),
		NWidget(NWID_HORIZONTAL),
			NWidget(NWID_SPACER), SetFill(1, 0), SetResize(1, 0),
			NWidget(WWT_RESIZEBOX, COLOUR_GREY, WID_TGW_RESIZE),
		EndContainer(),
	EndContainer()
};


/**
 * Window for the (old) vehicle listing.
 *
 * bitmask for w->window_number
 * 0-7 CompanyID (owner)
 * 8-10 window type (use flags in vehicle_gui.h)
 * 11-15 vehicle type (using VEH_, but can be compressed to fewer bytes if needed)
 * 16-31 StationID or OrderID depending on window type (bit 8-10)
 */
struct TimetableGraphWindow : public Window {
protected:
	OrderList* baseOrderList; //Order list shown
	VehicleListIdentifier vli; //Identifier of the shared order list shown

	std::vector<int> yindexPositions; //Positions of each row
	uint rowCount;

	typedef DestinationID Destination;

	/*
	struct Row {
		Destination dest; //The destination
		uint index;		//Its index on the list of destinations shown
		Row(Destination dest, uint index) : dest(dest), index(index) {}
	};

	*/

	uint YlabelWidth;

	struct OrderListGraph {
		GraphLine line;
		const OrderList* orderList;
		bool enabled;

		OrderListGraph(const GraphLine& line, const OrderList* orderList, bool enabled = true)
			: line(line), orderList(orderList), enabled(enabled) {}
	};

	GraphLine baseGraphLine;
	std::vector<OrderListGraph> orderListGraphs;

	TimetableGraphBuilder builder;

public:

	TimetableGraphWindow(WindowDesc *desc, WindowNumber window_number)
		: Window(desc), baseOrderList(NULL), vli(VehicleListIdentifier::UnPack(window_number)),
			YlabelWidth(0), baseGraphLine(), orderListGraphs()
	{
		this->CreateNestedTree();

		this->baseOrderList = Vehicle::GetIfValid(this->vli.index)->orders.list;

		InitGraphData();

		// Building the buttons to enable/disable showing the order lists
		NWidgetContainer* buttonsContainer = this->GetWidget<NWidgetContainer>(WID_TGW_ORDERS_SELECTION);
		for (int i = 0; i < orderListGraphs.size(); ++i) {
			NWidgetBackground* button = new NWidgetBackground(WWT_PANEL, COLOUR_YELLOW, WID_TGW_ORDERS_SELECTION_BEGIN + i);
			//button->SetToolTip(STR_TIMETABLE_GRAPH_TIMETABLE_TOOLTIP); TODO
			button->SetFill(1, 0);
			button->SetLowered(true);
			buttonsContainer->Add(button);
		}


		CalculateYLabelWidth();

		/* Set up the window widgets */
		//this->GetWidget<NWidgetCore>(WID_VL_LIST)->tool_tip = STR_VEHICLE_LIST_TRAIN_LIST_TOOLTIP + this->vli.vtype;

		//TODO put timetable name in caption
		//this->GetWidget<NWidgetCore>(WID_TGW_CAPTION)->widget_data = STR_TIMETABLE_GRAPH_CAPTION;


		this->FinishInitNested(window_number);


		InitYAxisPositions();

		if (this->vli.company != OWNER_NONE) this->owner = this->vli.company;
	}


protected:
	bool IsOrderListTimetabled(const OrderList* orders) const {
		return orders->GetNumVehicles() > 0 && orders->GetFirstSharedVehicle()->type == vli.type &&
				orders->HasStartTime() && !orders->GetTimetableDuration().IsInvalid();
	}

	/**
	 * Initializes the destinations vector and the mainGraphLine linked list from the orderList
	 * Only keeps goto orders (not conditionnal or implicit)
	 * Also initializes reverseIndex
	 */
	void InitGraphData() {
		builder.SetBaseOrderList(this->baseOrderList);
		this->baseGraphLine = builder.BuildGraph();
		rowCount = baseGraphLine.size();

		orderListGraphs.clear();
		const OrderList* orderList;

		FOR_ALL_ORDER_LISTS(orderList) {
			if (IsOrderListTimetabled(orderList)) {
				GraphLine graphLine = builder.GetGraphForOrderList(orderList);
				if (!graphLine.empty()) {
					orderListGraphs.push_back(OrderListGraph(graphLine, orderList, true));
				}
			}
		}
	}





	void InitYAxisPositions() {
		uint graphHeight = GetWidget<NWidgetBase>(WID_TGW_GRAPH)->current_y;
		yindexPositions.clear();
		yindexPositions.reserve(rowCount);

		for (uint i = 0; i < rowCount; i++) {
			yindexPositions.push_back(i * graphHeight / rowCount);
		}
	}

	/**
	 * Build a linked list of GraphPoint from a first match
	 * @pre (*ordIterator)->GetDestinationId() == destinations[destIndex]
	 * @param ordIterator a pointer to the *Order which is moved along the orderList's linked list as the algorith progresses
	 * @param destIt an iterator from the mainGraphLine of the first match
	 * @return the first element of the built linkedList
	 */
	/*
	GraphPoint* BuildGraphLine(const Order** ordIterator, std::vector<DestinationID>::const_iterator destIt) const {
		bool beforeReversal = destIt < reverseIt;

		GraphPoint* first = new GraphPoint(*ordIterator, destIndex, NULL);
		GraphPoint* current = first;


		while ((*ordIterator = (*ordIterator)->next) != NULL) {

			destIndex = current->yindex +1; //We start looking for common destinations after the previous match

			while (destIndex < rowCount && this->destinations[destIndex] != (*ordIterator)->GetDestination()) {
				destIndex++;
			}

			if ((destIndex < reverseIndex) != beforeReversal || destIndex >= rowCount) {
				//We have passed the reverse point or we haven't found a match with this->destinations
				break;
			}

			//We have found a new matching destination : adding it to the graph line
			GraphPoint* newPoint = new GraphPoint(*ordIterator, destIndex, NULL);
			current->next = newPoint;
			current = newPoint;
		}

		if (first->next != NULL) { //The line has at least two points
			return first;
		}
		else return NULL;
	}

	void InitOtherOrderLists() {
		this->otherGraphLines.clear();

		const OrderList* otherOrderList;

		FOR_ALL_ORDER_LISTS(otherOrderList) {
			if (otherOrderList->index == baseOrderList->index) continue;

			GraphPoint* down = NULL;
			GraphPoint* up = NULL;

			for (const Order* order = otherOrderList->GetFirstOrder(); order != NULL; order = order->next) {
				int destinationIndex;
				if (order->IsGotoOrder() &&
						(destinationIndex = destinations.FindIndex(order->GetDestination())) != -1) {
					bool beforeReversal = destinationIndex < reverseIndex;

					//We have found the first match in this portion of the orderList
					//Now we look for other matches which are on the same side of the reversal point as this one
					//Note: BuildGraphLine changes order (moves it forward in the linked list)
					GraphPoint* point = BuildGraphLine(&order, destinationIndex);

					if (beforeReversal) {
						down = point; //point can be NULL, but there is no need to check
					}
					else {
						up = point; //Same as above
					}

				}
			}

			if (down != NULL || up != NULL) { //There is at least one non-empty GraphLine
				otherGraphLines.insert(
					std::pair<OrderListID, GraphLine>(otherOrderList->index,
													GraphLine(down, up, false)));
			}

		}
	}


	void InitOtherOrderLists2() {
		class CompRows {
			bool operator() (const Row& a, const Row& b) {
				return (a.dest < b.dest) || (a.dest == b.dest && a.index < b.index);
				//Lexicographical order on Row
			}
		};
		std::set<Row, CompRows> rowMap;

		//Building the set
		for (int i = 0; i < destinations.size(); i++) {
			rowMap.insert(Row(destinations[i], i));
		}


		const OrderList* otherOrderList;

		FOR_ALL_ORDER_LISTS(otherOrderList) {
			if (otherOrderList->index == baseOrderList->index) continue;

			int prevMatchIndex = -1;
			Destination prevMatchDest = INVALID_STATION;

			for (const Order* order = otherOrderList->GetFirstOrder(); order != NULL; order = order->next) {
				Destination currDest = order->GetDestination();

				if (!order->IsGotoOrder() || currDest == prevMatchDest) continue;

				std::set<Row, CompRows>::const_iterator potentialNextMatch =
						rowMap.lower_bound(Row(order->GetDestination(), prevMatchIndex+1));

				if (potentialNextMatch->dest != currDest) {
					//We didn't find a match that was in the correct order from the previous one
					//(we must have prevMatchIndex < currMatchIndex

					//Now we search whether there is
				}

			}
		}

	}
	*/

	/**
	 * Set the DParam for the label string
	 * @param order The order for which we must prepare the label
	 * @return the string ID to show as label, STR_NULL if this order must not be shown (not a goto order)
	 */
	StringID PrepareDestinationLabel(const Order* order) const
	{
		if (!order->IsGotoOrder()) return STR_NULL;

		SetDParam(0, order->GetDestination());
		switch (order->GetType()) {
			case OT_GOTO_STATION:
				return STR_TIMETABLE_GRAPH_STATION_LABEL;
			case OT_GOTO_WAYPOINT:
				return STR_TIMETABLE_GRAPH_WAYPOINT_LABEL;
			case OT_GOTO_DEPOT:
				return STR_TIMETABLE_GRAPH_DEPOT_LABEL;
			default:
				NOT_REACHED();
		}
	}
	/**
	 * Calculate width for Y labels.
	 */
	void CalculateYLabelWidth()
	{
		uint max_width = 0;

		for (GraphLine::const_iterator it = baseGraphLine.begin(); it != baseGraphLine.end(); ++it) {
			StringID id = PrepareDestinationLabel(it->order1);
			if (id != STR_NULL) {
				Dimension d = GetStringBoundingBox(id);
				if (d.width > max_width) max_width = d.width;
			}
		}

		this->YlabelWidth = max_width;
	}

	/**
	 * Map a Date to a X position on the graph
	 * @param date the date to map
	 * @return x position
	 */
	int MapDateToXPosition(Date date, uint width) const {
		return (date - baseOrderList->GetStartTime()) * width / (this->baseOrderList->GetTimetableDuration().GetLengthAsDate());
	}

	/**
	 * Map a yindex to the y position on the graph
	 */
	int MapYIndexToPosition(uint yindex) const {
		return yindexPositions[yindex];
	}

	/**
	 * Draw a graph line (linked list of GraphPoint)
	 * @param r the rectangle of the graph (without the labels)
	 * @param firstPoint the first point of the linked list
	 */
	void DrawGraphLine(const Rect &r, const GraphLine& line, byte colour) const {
		if (line.size() < 2 ) return;

		uint width = r.right - r.left; //Graph width

		for (GraphLine::const_iterator it = line.begin(); it + 1 != line.end(); ++it) {
			if (it->order1->HasDeparture() && it->order2->HasArrival()) {
				int x = r.left + MapDateToXPosition(it->order1->GetDeparture() + it->offset1, width);
				int y = r.top + yindexPositions[it->index1];

				int x2 = r.left + MapDateToXPosition(it->order2->GetArrival() + it->offset2, width);
				int y2 = r.top + yindexPositions[it->index2];

				GfxDrawLine(x, y, x2, y2, colour, 1, 0);
			}
		}
	}

	/**
	 * Draw the legend for the Y axis and the horizontal grid lines
	 * @param r the rectangle of the graph
	 */
	void DrawYLegendAndGrid(const Rect &r) const {
		for (GraphLine::const_iterator it = baseGraphLine.begin(); it != baseGraphLine.end(); ++it) {
			//Draw the label
			DrawString(r.left,												//left
					r.left + YlabelWidth,									//right
					r.top + yindexPositions[std::distance(baseGraphLine.begin(), it)] - GetCharacterHeight(FS_SMALL) / 2,	//top
					PrepareDestinationLabel(it->order1),								//StringID
					TC_BLACK,												//colour
					SA_RIGHT);												//alignment

			//Draw the horizontal grid line
			GfxFillRect(r.left + YlabelWidth, //FIXME
						r.top + yindexPositions[std::distance(baseGraphLine.begin(), it)],
						r.right,
						r.top + yindexPositions[std::distance(baseGraphLine.begin(), it)],
						PC_BLACK);
		}
		if (rowCount >= 2) {
			//Last destination
			//Draw the label
			DrawString(r.left,												//left
					r.left + YlabelWidth,									//right
					r.top + yindexPositions[rowCount-1] - GetCharacterHeight(FS_SMALL) / 2,	//top
					PrepareDestinationLabel(baseGraphLine[0].order1),				//StringID
					TC_BLACK,												//colour
					SA_RIGHT);												//alignment

			//Draw the horizontal grid line
			GfxFillRect(r.left + YlabelWidth, //FIXME
						r.top + yindexPositions[rowCount-1],
						r.right,
						r.top + yindexPositions[rowCount-1],
						PC_BLACK);
		}
	}

	StringID PrepareTimetableNameString(int orderListIndex) const {
		if (orderListGraphs[orderListIndex].orderList->GetName() == NULL) {
			return STR_TIMETABLE_GRAPH_UNNAMED_TIMETABLE;
		} else {
			SetDParamStr(STR_TIMETABLE_GRAPH_TIMETABLE_NAME, orderListGraphs[orderListIndex].orderList->GetName());
			return STR_TIMETABLE_GRAPH_TIMETABLE_NAME;
		}
	}


	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		if (widget == WID_TGW_GRAPH) {
			Dimension dmin;
			dmin.height = rowCount * FONT_HEIGHT_SMALL;
			dmin.width = YlabelWidth;
			if (baseOrderList && !baseOrderList->GetTimetableDuration().IsInvalid()) {
				dmin.width += 1 * baseOrderList->GetTimetableDuration().GetLengthAsDate(); // 1pixel per day
			}


			*size = maxdim(*size, dmin);
		} else if (widget >= WID_TGW_ORDERS_SELECTION_BEGIN) {
			int orderListIndex = widget - WID_TGW_ORDERS_SELECTION_BEGIN;
			Dimension dim = GetStringBoundingBox(this->PrepareTimetableNameString(orderListIndex));

			dim.width += 14; // colour field
			dim.width += WD_FRAMERECT_LEFT + WD_FRAMERECT_RIGHT;
			dim.height += WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM;

			*size = maxdim(*size, dim);
		}
	}
	/*
	virtual void SetStringParameters(int widget) const
	{
		switch (widget) {
			case WID_TGW_CAPTION:
				//TODO
				//SetDParam(0, this->vli.);
				break;

		}
	}
	*/
	virtual void DrawWidget(const Rect &r, int widget) const
	{
		if (widget == WID_TGW_GRAPH) {

			DrawYLegendAndGrid(r);

			Rect graphRect(r);
			graphRect.top += 0; //FIXME
			graphRect.left += YlabelWidth;
			graphRect.bottom -= 10; //FIXME

			DrawGraphLine(graphRect, baseGraphLine, PC_DARK_BLUE /*_colour_gradient[COLOUR_BLUE][0]*/);
			for (std::vector<OrderListGraph>::const_iterator graphLine = orderListGraphs.begin();
					graphLine != orderListGraphs.end(); ++graphLine) {
				if (graphLine->enabled) {
					DrawGraphLine(graphRect, graphLine->line, _colour_gradient[COLOUR_DARK_GREEN][0]);
				}
			}

		} else if (widget >= WID_TGW_ORDERS_SELECTION_BEGIN) {
			bool rtl = _current_text_dir == TD_RTL; //Language is RTL ?
			byte clk_dif = this->IsWidgetLowered(widget) ? 1 : 0; //Is the button depressed ?
			int x = r.left + WD_FRAMERECT_LEFT;
			int y = r.top;

			int rect_x = clk_dif + (rtl ? r.right - 12 : r.left + WD_FRAMERECT_LEFT);

			GfxFillRect(rect_x, y + clk_dif, rect_x + 8, y + 5 + clk_dif, PC_BLACK);
			GfxFillRect(rect_x + 1, y + 1 + clk_dif, rect_x + 7, y + 4 + clk_dif, _colour_gradient[COLOUR_DARK_GREEN][0]);//TODO colour

			DrawString(rtl ? r.left : x + 14 + clk_dif, (rtl ? r.right - 14 + clk_dif : r.right), y + clk_dif, this->PrepareTimetableNameString(widget - WID_TGW_ORDERS_SELECTION_BEGIN));
		}
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			case WID_TGW_GRAPH:
				//TODO
				break;

			case WID_TGW_DISABLE_ALL:
				for (int i = 0; i < orderListGraphs.size(); ++i) {
					orderListGraphs[i].enabled = false;
					this->SetWidgetLoweredState(i - WID_TGW_ORDERS_SELECTION_BEGIN, false);
				}
				this->SetDirty();
				break;
			case WID_TGW_ENABLE_ALL:
				for (int i = 0; i < orderListGraphs.size(); ++i) {
					orderListGraphs[i].enabled = true;
					this->SetWidgetLoweredState(i - WID_TGW_ORDERS_SELECTION_BEGIN, true);
				}
				this->SetDirty();
				break;
			default:
				if (widget >= WID_TGW_ORDERS_SELECTION_BEGIN) {
					int orderListIndex = widget - WID_TGW_ORDERS_SELECTION_BEGIN;
					orderListGraphs[orderListIndex].enabled = !orderListGraphs[orderListIndex].enabled;
					this->ToggleWidgetLoweredState(widget);
				}
		}
	}

	virtual void OnResize()
	{
		this->InitYAxisPositions();
	}


	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	virtual void OnInvalidateData(int data = 0, bool gui_scope = true)
	{
		this->InitGraphData();
	}
};



static WindowDesc _timetable_graph_desc(
	WDP_AUTO, "timetable_graph", 260, 246,
	WC_TIMETABLE_GRAPH, WC_NONE,
	0,
	_nested_timetable_graph, lengthof(_nested_timetable_graph)
);

void ShowTimetableGraphWindow(OrderList *orderList)
{
	if (orderList == NULL) return;

	WindowNumber num = VehicleListIdentifier(VL_SHARED_ORDERS, orderList->GetFirstSharedVehicle()->type,
			orderList->GetFirstSharedVehicle()->owner, orderList->GetFirstSharedVehicle()->index).Pack();

	AllocateWindowDescFront<TimetableGraphWindow>(&_timetable_graph_desc, num);
}

