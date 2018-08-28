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
#include "core/smallvec_type.hpp"
#include "timetable_graph_gui.h"
#include "window_type.h"
#include "window_gui.h"
#include "vehiclelist.h"
#include "vehicle_base.h"
#include "widgets/timetable_widget.h"
#include "core/geometry_func.hpp"


static const NWidgetPart _nested_vehicle_list[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_TGW_CAPTION),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_DEFSIZEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),

	NWidget(WWT_PANEL, COLOUR_GREY/*, WID_TGW_BACKGROUND*/),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_EMPTY, COLOUR_GREY, WID_TGW_GRAPH), SetMinimalSize(576, 160), SetFill(1, 1), SetResize(1, 1),
			NWidget(NWID_VERTICAL),
				NWidget(NWID_SPACER), SetFill(0, 1), SetResize(0, 1),
				NWidget(WWT_RESIZEBOX, COLOUR_GREY/*, WID_TGW_RESIZE*/),
			EndContainer(),
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
	OrderList* orderList; //Order list shown
	VehicleListIdentifier vli; //Identifier of the shared order list shown

	std::vector<int> yindexPositions; //Positions of each row
	uint rowCount;


	/**
	 * A point of the graph, element of a linked list of points
	 */
	struct GraphPoint {
		uint yindex; //The horizontal line onto which the point should be drawn (which has ordersShown[yaxis] DestinationID
		const Order* order; //The order this point relates to
		GraphPoint* next; //The next point in the linked list, NULL if this is the last one

		GraphPoint(const Order* order, uint yindex, GraphPoint* next = NULL) : yindex(yindex), order(order), next(next)  {}
		~GraphPoint() {
			delete next;
		}
	};

	GraphPoint* mainGraphLine; //The graph line of the orderList relating to this window
	std::vector<DestinationID> destinations; //The destinations (rows of the graph)

	uint reverseIndex; //The index for destination just before the first time the vehicle visits again the same stop


	uint YlabelWidth;

public:

	TimetableGraphWindow(WindowDesc *desc, WindowNumber window_number) : Window(desc), orderList(nullptr),
			vli(VehicleListIdentifier::UnPack(window_number)),
			 mainGraphLine(nullptr), YlabelWidth(0)
	{
		this->CreateNestedTree();

		this->orderList = Vehicle::GetIfValid(this->vli.index)->orders.list;

		InitDestinations();
		CalculateYLabelWidth();

		/* Set up the window widgets */
		//this->GetWidget<NWidgetCore>(WID_VL_LIST)->tool_tip = STR_VEHICLE_LIST_TRAIN_LIST_TOOLTIP + this->vli.vtype;

		//TODO put timetable name in caption
		this->GetWidget<NWidgetCore>(WID_TGW_CAPTION)->widget_data = STR_TIMETABLE_GRAPH_CAPTION;


		this->FinishInitNested(window_number);


		InitYAxisPositions();

		if (this->vli.company != OWNER_NONE) this->owner = this->vli.company;
	}

	~TimetableGraphWindow()
	{
		delete mainGraphLine;
	}

protected:
	/**
	 * Initializes the destinations vector and the mainGraphLine linked list from the orderList
	 * Only keeps goto orders (not conditionnal or implicit)
	 * Also initializes reverseIndex
	 */
	void InitDestinations() {
		delete mainGraphLine;

		destinations.resize(this->orderList->GetNumItems());

		uint i = 0;
		GraphPoint* prevGraphPoint(NULL);

		for (Order* ord = this->orderList->GetFirstOrder(); ord != NULL; ord = ord->next) {
			if (ord->IsGotoOrder()) {
				GraphPoint* currGraphPoint = new GraphPoint(ord, i);
				if (prevGraphPoint) {
					prevGraphPoint->next = currGraphPoint;
				}
				else { // There is no previous GraphPoint : it is the first one
					this->mainGraphLine = currGraphPoint;
				}
				prevGraphPoint = currGraphPoint;

				destinations[i] = ord->GetDestination();

				if (reverseIndex == 0) { //We haven't yet reached a reversal point
					//Look if this destination was already added
					for (uint j = 0; j < i; j++) {
						if (destinations[i] == destinations[j]) {
							reverseIndex = i - 1;
							break;
						}
					}
				}

				i++;
			}
		}
		rowCount = i;
		destinations.resize(rowCount);
	}



	void InitYAxisPositions() {
		uint graphHeight = GetWidget<NWidgetBase>(WID_TGW_GRAPH)->current_y;
		yindexPositions.resize(rowCount);

		for (uint i = 0; i < rowCount; i++) {
			yindexPositions[i] = i * graphHeight / rowCount;
		}
	}





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
		GraphPoint* point = this->mainGraphLine;
		uint max_width;

		while (point->next) {
			StringID id = PrepareDestinationLabel(point->order);
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
		return (date - orderList->GetStartTime()) * width / (this->orderList->GetTimetableDuration().GetLengthAsDate());
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
	void DrawGraphLine(const Rect &r, const GraphPoint* firstPoint) const {
		uint width = r.right - r.left; //Graph width
		if (firstPoint->next) {
			if (firstPoint->order->HasDeparture() && firstPoint->next->order->HasArrival()) {
				int x = r.left + MapDateToXPosition(firstPoint->order->GetDeparture(), width);
				int y = r.top + yindexPositions[firstPoint->yindex];

				int x2 = r.left + MapDateToXPosition(firstPoint->next->order->GetArrival(), width);
				int y2 = r.top + yindexPositions[firstPoint->next->yindex];

				int colour = COLOUR_DARK_BLUE; //TODO

				GfxDrawLine(x, y, x2, y2, colour, 1, 0);
			}

			DrawGraphLine(r, firstPoint->next);
		}
	}

	/**
	 * Draw the legend for the Y axis and the horizontal grid lines
	 * @param r the rectangle of the graph
	 */
	void DrawYLegendAndGrid(const Rect &r) const {
		const GraphPoint* point = mainGraphLine;
		for (uint i = 0; i < rowCount; i++, point = point->next) {
			//Draw the label
			DrawString(r.left,												//left
					r.left + YlabelWidth,									//right
					r.top + yindexPositions[i] - GetCharacterHeight(FS_SMALL) / 2,	//top
					PrepareDestinationLabel(point->order),								//StringID
					TC_BLACK,												//colour
					SA_RIGHT);												//alignment

			//Draw the horizontal grid line
			GfxFillRect(r.left + YlabelWidth,
						r.top + yindexPositions[i],
						r.right,
						yindexPositions[i],
						PC_BLACK);
		}
	}


	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		if (widget != WID_TGW_GRAPH) return;

		Dimension dmin;
		dmin.height = yindexPositions.size() * FONT_HEIGHT_SMALL;
		dmin.width = YlabelWidth;
		if (orderList && !orderList->GetTimetableDuration().IsInvalid()) {
			dmin.width += 1 * orderList->GetTimetableDuration().GetLengthAsDate(); // 1pixel per day
		}


		*size = maxdim(*size, dmin);
	}

	virtual void SetStringParameters(int widget) const
	{
		switch (widget) {
			case WID_TGW_CAPTION:
				//TODO
				//SetDParam(0, this->vli.);
				break;

		}
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		if (widget != WID_TGW_GRAPH) return;

		if (mainGraphLine) {
			DrawYLegendAndGrid(r);

			Rect graphRect(r);
			graphRect.top += 0; //FIXME
			graphRect.left += YlabelWidth;
			graphRect.bottom -= 10; //FIXME
			DrawGraphLine(graphRect, mainGraphLine);
		}
	}

	virtual void OnPaint()
	{
		/* Hide the widgets that we will not use in this window
		 * Some windows contains actions only fit for the owner */
		/*
		int plane_to_show = (this->owner == _local_company) ? BP_SHOW_BUTTONS : BP_HIDE_BUTTONS;
		NWidgetStacked *nwi = this->GetWidget<NWidgetStacked>(WID_VL_HIDE_BUTTONS);
		if (plane_to_show != nwi->shown_plane) {
			nwi->SetDisplayedPlane(plane_to_show);
			nwi->SetDirty(this);
		}
		if (this->owner == _local_company) {
			this->SetWidgetDisabledState(WID_VL_AVAILABLE_VEHICLES, this->vli.type != VL_STANDARD);
			this->SetWidgetsDisabledState(this->vehicles.Length() == 0,
				WID_VL_MANAGE_VEHICLES_DROPDOWN,
				WID_VL_STOP_ALL,
				WID_VL_START_ALL,
				WIDGET_LIST_END);
		}
		*/
		/* Set text of sort by dropdown widget. */
		//this->GetWidget<NWidgetCore>(WID_VL_SORT_BY_PULLDOWN)->widget_data = this->vehicle_sorter_names[this->vehicles.SortType()];

		this->DrawWidgets();
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			case WID_TGW_GRAPH:
				//TODO
				break;
		}
	}
	/*
	virtual void OnDropdownSelect(int widget, int index)
	{
		switch (widget) {
			case WID_VL_SORT_BY_PULLDOWN:
				this->vehicles.SetSortType(index);
				break;
			case WID_VL_MANAGE_VEHICLES_DROPDOWN:
				assert(this->vehicles.Length() != 0);

				switch (index) {
					case ADI_REPLACE: // Replace window
						ShowReplaceGroupVehicleWindow(ALL_GROUP, this->vli.vtype);
						break;
					case ADI_SERVICE: // Send for servicing
					case ADI_DEPOT: // Send to Depots
						DoCommandP(0, DEPOT_MASS_SEND | (index == ADI_SERVICE ? DEPOT_SERVICE : (DepotCommand)0), this->window_number, GetCmdSendToDepot(this->vli.vtype));
						break;

					default: NOT_REACHED();
				}
				break;
			default: NOT_REACHED();
		}
		this->SetDirty();
	}
	*/


	virtual void OnResize()
	{
		//this->vscroll->SetCapacityFromWidget(this, WID_VL_LIST);
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	virtual void OnInvalidateData(int data = 0, bool gui_scope = true)
	{
		this->InitDestinations();
	}
};



static WindowDesc _timetable_graph_desc(
	WDP_AUTO, "timetable_graph", 260, 246,
	WC_TIMETABLE_GRAPH, WC_NONE,
	0,
	_nested_vehicle_list, lengthof(_nested_vehicle_list)
);

void ShowTimetableGraphWindow(OrderList *orderList)
{
	if (orderList == NULL) return;

	WindowNumber num = VehicleListIdentifier(VL_SHARED_ORDERS, orderList->GetFirstSharedVehicle()->type,
			orderList->GetFirstSharedVehicle()->owner, orderList->GetFirstSharedVehicle()->index).Pack();

	AllocateWindowDescFront<TimetableGraphWindow>(&_timetable_graph_desc, num);
}

