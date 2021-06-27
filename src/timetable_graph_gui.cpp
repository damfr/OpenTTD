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
#include "vehicle_gui.h"
#include "widgets/timetable_widget.h"
#include "core/geometry_func.hpp"
#include "timetable_graph.h"
#include "date_func.h"


static const int MAX_ORDER_LISTS_SHOWN = 20;

static NWidgetBase* MakeOrderListButtons(int *biggestIndex) {
	NWidgetVertical* ver = new NWidgetVertical;

	for (int i = 0; i < MAX_ORDER_LISTS_SHOWN; ++i) {
		NWidgetBackground* button = new NWidgetBackground(WWT_PANEL, COLOUR_YELLOW, WID_TGW_ORDERS_SELECTION_BEGIN + i);
		//button->tool_tip =
		button->SetFill(1, 0);
		button->SetLowered(true);
		button->SetDisabled(true);
		ver->Add(button);
	}
	*biggestIndex = WID_TGW_ORDERS_SELECTION_BEGIN + MAX_ORDER_LISTS_SHOWN;
	return ver;
}

static const NWidgetPart _nested_timetable_graph[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_TGW_CAPTION), SetDataTip(STR_TIMETABLE_GRAPH_CAPTION, STR_NULL),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_DEFSIZEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),

	NWidget(WWT_PANEL, COLOUR_GREY/*, WID_TGW_BACKGROUND*/),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_EMPTY, COLOUR_GREY, WID_TGW_GRAPH), SetMinimalSize(576, 160), SetFill(0, 1), SetResize(1, 1),
			NWidget(NWID_VERTICAL),
				NWidget(NWID_SPACER), SetFill(0, 1), SetResize(0, 1),
				NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, WID_TGW_ENABLE_ALL), SetDataTip(STR_GRAPH_CARGO_ENABLE_ALL, STR_GRAPH_CARGO_TOOLTIP_ENABLE_ALL), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, WID_TGW_DISABLE_ALL), SetDataTip(STR_GRAPH_CARGO_DISABLE_ALL, STR_GRAPH_CARGO_TOOLTIP_DISABLE_ALL), SetFill(1, 0),
				NWidget(NWID_SPACER), SetMinimalSize(0, 4),
				NWidgetFunction(MakeOrderListButtons),
				NWidget(NWID_SPACER), SetFill(0, 1), SetResize(0, 1),
			EndContainer(),
		EndContainer(),
		NWidget(NWID_HORIZONTAL),
			NWidget(NWID_SPACER), SetFill(1, 0), SetResize(1, 0),
			NWidget(WWT_RESIZEBOX, COLOUR_GREY, WID_TGW_RESIZE),
		EndContainer(),
	EndContainer()
};

static const int MIN_PXL_PER_DAY = 1;

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
	uint yindexCount; ///< The number of elements in yIndexPositions
	uint rowCount;  ///< The number of rows in the graph (may be less than yindexCount when reversing mode is enabled)

	/**
	 * The index at which we start drawing upwards (for single line graphs)
	 * (INT_MAX-1 if we never reverse)
	 */
	uint reverseIndex;

	typedef DestinationID Destination;

	uint YlabelWidth;
	Date startDate;
	Date endDate;
	int XLegendHeight;

	struct OrderListGraph {
		GraphLine line;
		byte colour;
		bool enabled;

		OrderListGraph(const GraphLine& line, byte colour, bool enabled = true)
			: line(line), colour(colour), enabled(enabled) {}
	};

	GraphLine baseGraphLine;
	std::vector<OrderListGraph> orderListGraphs;

	TimetableGraphBuilder builder;

	int graphPaddingTop;
	int graphPaddingBottom;
	int graphPaddingLeft;
	int graphPaddingRight;

public:

	TimetableGraphWindow(WindowDesc *desc, WindowNumber window_number)
		: Window(desc), baseOrderList(nullptr), vli(VehicleListIdentifier::UnPack(window_number)),
			yindexCount(0), rowCount(0), reverseIndex((uint) INT_MAX-1), YlabelWidth(0), baseGraphLine(), orderListGraphs(),
			graphPaddingTop(GetCharacterHeight(FS_SMALL) / 2 + 5), graphPaddingBottom(GetCharacterHeight(FS_SMALL) / 2), graphPaddingLeft(5),
			graphPaddingRight(10)
	{
		this->CreateNestedTree();

		this->baseOrderList = OrderList::GetIfValid(this->vli.index);

		InitGraphData();

		InitOrderListButtons();

		CalculateYLabelWidth();
		InitXAxis();

		//TODO put timetable name in caption
		//this->GetWidget<NWidgetCore>(WID_TGW_CAPTION)->widget_data = STR_TIMETABLE_GRAPH_CAPTION;


		this->FinishInitNested(window_number);


		InitYAxisPositions();

		if (this->vli.company != OWNER_NONE) this->owner = this->vli.company;
	}


protected:
	bool IsOrderListTimetabled(const OrderList* orders) const {
		return orders->GetNumVehicles() > 0  &&
				orders->HasStartTime() && !orders->GetTimetableDuration().IsInvalid();
	}

	/**
	 * Initializes the mainGraphLine vector and the mainGraphLine linked list from the orderList
	 * Only keeps goto orders (not conditionnal or implicit)
	 * Also initializes reverseIndex
	 */
	void InitGraphData() {
		builder.SetBaseOrderList(this->baseOrderList);
		this->baseGraphLine = builder.BuildGraph();
		yindexCount = baseGraphLine.segments.size() +1;
		rowCount = reverseIndex >= yindexCount ? yindexCount : reverseIndex+1;

		orderListGraphs.clear();
		
		int colour = COLOUR_BEGIN;
		int shade = 4;
		
		for (const OrderList* orderList : OrderList::Iterate()) {
			if (orderList != this->baseOrderList && IsOrderListTimetabled(orderList)) {
				GraphLine graphLine = builder.GetGraphForOrderList(orderList);
				if (!graphLine.segments.empty()) {
					orderListGraphs.push_back(OrderListGraph(graphLine, _colour_gradient[colour][shade], true));
					++colour;
					if (colour >= COLOUR_WHITE) {
						colour = COLOUR_BEGIN;
						if (shade == 4) shade = 7;
						else break; //We have exhausted all colours
					}
				}
			}
		}
	}

	/**
	 * Update the state of the buttons to enable/disable showing the order lists
	 */
	void InitOrderListButtons() {
		for (uint i = 0; i < MAX_ORDER_LISTS_SHOWN; ++i) {
			NWidgetBackground* button = this->GetWidget<NWidgetBackground>(WID_TGW_ORDERS_SELECTION_BEGIN + i);

			if (i < orderListGraphs.size()) {
				button->SetDisabled(false);
				button->SetLowered(orderListGraphs[i].enabled);
			} else {
				button->SetLowered(false);
				button->SetDisabled(true);
			}

		}
	}



	void InitYAxisPositions() {
		uint graphHeight = GetWidget<NWidgetBase>(WID_TGW_GRAPH)->current_y - graphPaddingTop - graphPaddingBottom - XLegendHeight;
		uint minRowHeight = FONT_HEIGHT_SMALL;
		yindexPositions.resize(yindexCount);
		
		Date ttLength = this->baseOrderList->GetTimetableDuration().GetLengthAsDate();
		uint freeSpace = graphHeight - (rowCount-1) * minRowHeight;	//The vertical space (in pixels) that we can allocate freely (after the minimum heights have been deduced)
		Date freeSpaceDuration = 0;	//Same, but in Date instead of pixels
		Date minDuration = ttLength * minRowHeight/graphHeight;	//The equivalent of minRowHeight in Date

		/* First pass :  we calculate freeSpaceDuration. We only consider non-reverse segments */
		for (uint i = 1; i < rowCount; ++i) {
			/* Before reversing */
			const GraphSegment& segment = this->baseGraphLine.segments[i-1];

			/* If segment is not fully timetabled we consider its duration to be less than minDuration */
			if (!segment.HasDuration()) continue;

			Date segmentDuration = segment.GetDuration();

			if (segmentDuration > minDuration) {
				//This segment exceeds the minimum height : we manage its height via freeSpaceDuration

				freeSpaceDuration += segmentDuration - minDuration;
			}
		}

		//Second pass : we calculate the heights
		uint currY = 0;
		yindexPositions[0] = 0;
		for (uint i = 1; i < rowCount; i++) {
			const GraphSegment& segment = this->baseGraphLine.segments[i-1];
			if (segment.HasDuration() && segment.GetDuration() > minDuration && freeSpaceDuration > 0) {
				//We use the freespace in proportion with the duration
				currY += minRowHeight + (segment.GetDuration() - minDuration) * freeSpace / freeSpaceDuration;
			} else {
				//The height of this segment will be minRowHeight, as it would otherwise be too small
				currY += minRowHeight;
			}
			yindexPositions[i] = currY;
		}

		/* Compute the heights for reverse mode 
			Beware of overflowing j */
		// rowCount = reverseIndex +1
		for (int i = reverseIndex, j = reverseIndex +1; j < (int) yindexCount && i > 0; i--, j++) {
			const GraphSegment& segment_outbound = this->baseGraphLine.segments[i-1];
			const GraphSegment& segment_return = this->baseGraphLine.segments[j-1];
			/* We only draw segments that match exactly on the inbound and outbound
			   For now ignore the rest */

			if (segment_outbound.order1->GetDestination() == segment_return.order2->GetDestination()
				&& segment_outbound.order2->GetDestination() == segment_return.order1->GetDestination()) {
				yindexPositions[j] = yindexPositions[i-1];
			} else {
				yindexPositions[j] = -100; 
				/* We put a magic constant to draw the line outside the screen. 
				TODO : improve this */
			}
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
		uint max_width = 0;

		for (std::vector<GraphSegment>::const_iterator it = baseGraphLine.segments.begin(); it != baseGraphLine.segments.end(); ++it) {
			StringID id = PrepareDestinationLabel(it->order1);
			if (id != STR_NULL) {
				Dimension d = GetStringBoundingBox(id);
				if (d.width > max_width) max_width = d.width;
			}
		}

		this->YlabelWidth = max_width;
		this->graphPaddingLeft = YlabelWidth + 5;
	}

	void InitXAxis() {
		YearMonthDay ymd;
		ConvertDateToYMD(baseOrderList->GetStartTime(), &ymd);
		this->startDate = ConvertYMDToDate(ymd.year, ymd.month, 1);

		Duration dur = baseOrderList->GetTimetableDuration();
		dur.AddLength(1);
		this->endDate = AddToDate(baseOrderList->GetStartTime(), dur);

		this->XLegendHeight = FONT_HEIGHT_SMALL * 2;
	}

	/**
	 * Map a Date to a X position on the graph
	 * @param date the date to map
	 * @return x position
	 */
	int MapDateToXPosition(Date date, int graphWidth) const {
		if (endDate - startDate > 0) {
			return (date - startDate) * graphWidth / (endDate - startDate);
		} else return 0;
	}

	/**
	 * Draw a graph line (linked list of GraphPoint)
	 * @param r the rectangle of the graph (without the labels)
	 * @param line the line to draw
	 * @param colour colour of the line
	 * @param globalOffset an offset in days to apply to all points of the line
	 */
	void DrawGraphLine(const Rect &r, const std::vector<GraphSegment>& segments, byte colour, Duration globalOffset = Duration(0, DU_DAYS)) const {
		int graphWidth = r.right - r.left;
		for (std::vector<GraphSegment>::const_iterator it = segments.begin(); it != segments.end(); ++it) {
			if (it->order1->HasDeparture() && it->order2->HasArrival()
				&& yindexPositions[it->index1] >= 0 && yindexPositions[it->index2] >= 0) {

				int x = r.left + MapDateToXPosition(AddToDate(AddToDate(it->order1->GetDeparture(), it->offset1), globalOffset), graphWidth);
				int y = r.top + yindexPositions[it->index1];

				int x2 = r.left + MapDateToXPosition(AddToDate(AddToDate(it->order2->GetArrival(), it->offset2), globalOffset), graphWidth);
				int y2 = r.top + yindexPositions[it->index2];

				GfxDrawLine(x, y, x2, y2, colour, 1, 0);
			}
		}
	}

	/**
	 * Draw the legend for the Y axis and the horizontal grid lines
	 * @param r the rectangle of the graph widget
	 */
	void DrawYLegendAndGrid(const Rect &r) const {
		for (std::vector<GraphSegment>::const_iterator it = baseGraphLine.segments.begin(); it != baseGraphLine.segments.end(); ++it) {
			//Draw the label
			DrawString(r.left,												//left
					r.left + YlabelWidth,									//right
					r.top + yindexPositions[std::distance(baseGraphLine.segments.begin(), it)] - GetCharacterHeight(FS_SMALL) / 2,	//top
					PrepareDestinationLabel(it->order1),								//StringID
					TC_BLACK,												//colour
					SA_RIGHT);												//alignment

			//Draw the horizontal grid line
			GfxFillRect(r.left + YlabelWidth, //FIXME
						r.top + yindexPositions[std::distance(baseGraphLine.segments.begin(), it)],
						r.right,
						r.top + yindexPositions[std::distance(baseGraphLine.segments.begin(), it)],
						PC_BLACK);
		}
		if (rowCount >= 2) {
			//Last destination
			//Draw the label
			DrawString(r.left,												//left
					r.left + YlabelWidth,									//right
					r.top + yindexPositions[rowCount-1] - GetCharacterHeight(FS_SMALL) / 2,	//top
					PrepareDestinationLabel(baseGraphLine.segments[0].order1),				//StringID
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


	/**
	 * @param r The rect of the graph itself (not the widget)
	 */
	void DrawXLegendAndGrid(const Rect& r) const {
		for (Date date = startDate; date <= endDate; date = AddToDate(date, Duration(1, DU_MONTHS))) {
			int x = MapDateToXPosition(date, r.right - r.left);
			//Draw the vertical line
			GfxFillRect(r.left + x,
						r.top,
						r.left + x,
						r.bottom + this->XLegendHeight,
						PC_BLACK,
						FILLRECT_CHECKER);

			YearMonthDay ymd;
			ConvertDateToYMD(date, &ymd);
			SetDParam(0, ymd.month + STR_MONTH_ABBREV_JAN);
			SetDParam(1, ymd.year);
			DrawStringMultiLine(r.left + x - 15, r.left + x + 15, r.bottom, r.bottom + XLegendHeight,
					STR_TIMETABLE_GRAPH_X_LEGEND, TC_BLACK, SA_CENTER);
		}
	}

	StringID PrepareTimetableNameString(int orderListIndex) const {
		if (orderListGraphs[orderListIndex].line.orderList->GetName() == nullptr) {
			return STR_TIMETABLE_GRAPH_UNNAMED_TIMETABLE;
		} else {
			SetDParamStr(0, orderListGraphs[orderListIndex].line.orderList->GetName());
			return STR_TIMETABLE_GRAPH_TIMETABLE_NAME;
		}
	}


	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		if (widget == WID_TGW_GRAPH) {
			Dimension dmin;
			dmin.height = rowCount * FONT_HEIGHT_SMALL + graphPaddingTop + XLegendHeight;
			dmin.width = graphPaddingLeft + (endDate - startDate) * MIN_PXL_PER_DAY;

			*size = maxdim(*size, dmin);
		} else if (widget >= WID_TGW_ORDERS_SELECTION_BEGIN && widget < WID_TGW_ORDERS_SELECTION_BEGIN + (int) orderListGraphs.size()) {
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

	/**
	 * Draws the graph line, possibly several times (with an offset), on the graph represented by r
	 * This function attempts to determine the time offset between two vehicles sharing the order list which graphLine represents
	 * @param r the rectangle of the graph
	 * @param graphLine the graph line to draw
	 */
	void DrawMultipleGraphLine(const Rect& r, const GraphLine& graphLine, byte colour) const {
		if (graphLine.segments.size() == 0) return;

		Duration length = graphLine.orderList->GetTimetableDuration();

		Date startDate = AddToDate(graphLine.orderList->GetStartTime(), *(graphLine.offsets.begin())); //The date at which this order lists starts in reality
		Duration offsetFront = *(graphLine.offsets.begin()); //We take the unit for the  offset from the smallest of the offsets (arbitrary)
		offsetFront.SetLength(0);


		//First we go forwards to find the left-most line that is shown in the graph and calculate its offset

		while (AddToDate(startDate, offsetFront) < this->endDate) {

			for (std::set<Duration>::const_iterator itOffset = graphLine.offsets.begin(); itOffset != graphLine.offsets.end(); ++itOffset) {
				Duration currOffset = offsetFront;
				currOffset.Add(*itOffset);
				this->DrawGraphLine(r, graphLine.segments, colour, currOffset);
			}


			offsetFront.Add(length);
		}

		//Then backwards
		Duration biggestOffset = *(--graphLine.offsets.end());
		Date endDate = AddToDate(graphLine.orderList->GetStartTime(), biggestOffset);
		Duration offsetBack = -length;

		while (AddToDate(AddToDate(endDate, offsetBack), biggestOffset) > this->startDate) {
			for (std::set<Duration>::const_iterator itOffset = graphLine.offsets.begin(); itOffset != graphLine.offsets.end(); ++itOffset) {
				Duration currOffset = offsetBack;
				currOffset.Add(*itOffset);
				this->DrawGraphLine(r, graphLine.segments, colour, currOffset);
			}

			offsetBack.Subtract(length);
		}


	}


	virtual void DrawWidget(const Rect &r, int widget) const
	{
		if (widget == WID_TGW_GRAPH) {
			Rect graphRect(r);

			graphRect.top += graphPaddingTop;
			graphRect.bottom -= graphPaddingBottom + XLegendHeight;

			DrawYLegendAndGrid(graphRect);

			graphRect.left += graphPaddingLeft;
			graphRect.right -= graphPaddingRight;

			DrawXLegendAndGrid(graphRect);

			//DrawGraphLine(graphRect, baseGraphLine.segments, _colour_gradient[COLOUR_WHITE][7]);
			DrawMultipleGraphLine(graphRect, baseGraphLine, _colour_gradient[COLOUR_WHITE][7]);
			for (std::vector<OrderListGraph>::const_iterator graphLine = orderListGraphs.begin();
					graphLine != orderListGraphs.end(); ++graphLine) {
				if (graphLine->enabled) {
					DrawMultipleGraphLine(graphRect, graphLine->line, graphLine->colour);
				}
			}

		} else if (widget >= WID_TGW_ORDERS_SELECTION_BEGIN) {
			bool rtl = _current_text_dir == TD_RTL; //Language is RTL ?
			byte clk_dif = this->IsWidgetLowered(widget) ? 1 : 0; //Is the button depressed ?
			int x = r.left + WD_FRAMERECT_LEFT;
			int y = r.top;

			int rect_x = clk_dif + (rtl ? r.right - 12 : r.left + WD_FRAMERECT_LEFT);

			GfxFillRect(rect_x, y + clk_dif, rect_x + 8, y + 5 + clk_dif, PC_BLACK);
			GfxFillRect(rect_x + 1, y + 1 + clk_dif, rect_x + 7, y + 4 + clk_dif, orderListGraphs[widget-WID_TGW_ORDERS_SELECTION_BEGIN].colour);

			DrawString(rtl ? r.left : x + 14 + clk_dif, (rtl ? r.right - 14 + clk_dif : r.right), y + clk_dif, this->PrepareTimetableNameString(widget - WID_TGW_ORDERS_SELECTION_BEGIN));
		}
	}

	virtual void OnResize() {
		this->InitXAxis();
		this->InitYAxisPositions();
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			case WID_TGW_GRAPH: {
				Rect graph_rect = GetWidget<NWidgetBase>(widget)->GetCurrentRect();
				graph_rect.top += graphPaddingTop;

				if (IsInsideBS(pt.x, graph_rect.left, YlabelWidth)) {
					for (uint i = 0; i < rowCount; i++) {
						if (IsInsideBS(pt.y, graph_rect.top + yindexPositions[i] - GetCharacterHeight(FS_SMALL) / 2, GetCharacterHeight(FS_SMALL))) {
							reverseIndex = i;
							this->OnInvalidateData();
							this->SetDirty();
							return;
						}
					}
				}
				break;
			}
			case WID_TGW_DISABLE_ALL:
				for (uint i = 0; i < orderListGraphs.size(); ++i) {
					orderListGraphs[i].enabled = false;
					this->SetWidgetLoweredState(i + WID_TGW_ORDERS_SELECTION_BEGIN, false);
				}
				this->SetDirty();
				break;
			case WID_TGW_ENABLE_ALL:
				for (uint i = 0; i < orderListGraphs.size(); ++i) {
					orderListGraphs[i].enabled = true;
					this->SetWidgetLoweredState(i + WID_TGW_ORDERS_SELECTION_BEGIN, true);
				}
				this->SetDirty();
				break;
			default:
				if (widget >= WID_TGW_ORDERS_SELECTION_BEGIN) {
					int orderListIndex = widget - WID_TGW_ORDERS_SELECTION_BEGIN;
					orderListGraphs[orderListIndex].enabled = !orderListGraphs[orderListIndex].enabled;
					this->ToggleWidgetLoweredState(widget);
					this->SetDirty();
				}
		}
	}


	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	virtual void OnInvalidateData(int data = 0, bool gui_scope = true)
	{
		baseGraphLine.segments.clear();
		baseGraphLine.offsets.clear();
		orderListGraphs.clear();
		builder.SetBaseOrderList(NULL);

		if (gui_scope) {
			this->InitGraphData();
			this->InitOrderListButtons();
			this->CalculateYLabelWidth();
			this->ReInit();
			this->InitYAxisPositions();
		}
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

	WindowNumber num = VehicleListIdentifier(VL_TIMETABLE_GRAPH, orderList->GetFirstSharedVehicle()->type,
				orderList->GetFirstSharedVehicle()->owner, orderList->index).Pack();

	AllocateWindowDescFront<TimetableGraphWindow>(&_timetable_graph_desc, num);
}

