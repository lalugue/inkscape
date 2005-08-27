/*
 * vim: ts=4 sw=4 et tw=0 wm=0
 *
 * libavoid - Fast, Incremental, Object-avoiding Line Router
 * Copyright (C) 2004-2005  Michael Wybrow <mjwybrow@users.sourceforge.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * 
*/

#ifndef AVOID_GRAPH_H
#define AVOID_GRAPH_H


#include <cassert>
#include <list>
#include <utility>
using std::pair;

#include "libavoid/vertices.h"


namespace Avoid {


extern bool UseAStarSearch;
extern bool IgnoreRegions;
extern bool SelectiveReroute;
extern bool IncludeEndpoints;
extern bool UseLeesAlgorithm;
extern bool InvisibilityGrph;
extern bool PartialFeedback;


typedef std::list<int> ShapeList;
typedef std::list<bool *> FlagList;


class EdgeInf
{
    public:
        EdgeInf(VertInf *v1, VertInf *v2);
        ~EdgeInf();
        double getDist(void);
        void setDist(double dist);
        void alertConns(void);
        void addConn(bool *flag);
        void addCycleBlocker(void);
        void addBlocker(int b);
        bool hasBlocker(int b);
        pair<VertID, VertID> ids(void);
        pair<Point, Point> points(void);
        void db_print(void);
        void checkVis(void);
        VertInf *otherVert(VertInf *vert);
        static EdgeInf *checkEdgeVisibility(VertInf *i, VertInf *j,
                bool knownNew = false);
        static EdgeInf *existingEdge(VertInf *i, VertInf *j);
        
        EdgeInf *lstPrev;
        EdgeInf *lstNext;
    private:
        bool _added;
        bool _visible;
        VertInf *_v1;
        VertInf *_v2;
        EdgeInfList::iterator _pos1;
        EdgeInfList::iterator _pos2;
        ShapeList _blockers;
        FlagList  _conns; 
        double  _dist;

        void makeActive(void);
        void makeInactive(void);
        int firstBlocker(void);
        bool isBetween(VertInf *i, VertInf *j);
};


class EdgeList
{
    public:
        EdgeList();
        void addEdge(EdgeInf *edge);
        void removeEdge(EdgeInf *edge);
        EdgeInf *begin(void);
        EdgeInf *end(void);
    private:
        EdgeInf *_firstEdge;
        EdgeInf *_lastEdge;
        uint _count;
};


extern EdgeList visGraph;
extern EdgeList invisGraph;

class ShapeRef;

extern void newBlockingShape(Polygn *poly, int pid);
extern void checkAllBlockedEdges(int pid);
extern void checkAllMissingEdges(void);
extern void generateContains(VertInf *pt);
extern void adjustContainsWithAdd(const Polygn& poly, const int p_shape);
extern void adjustContainsWithDel(const int p_shape);
extern void markConnectors(ShapeRef *shape);
extern void printInfo(void);


}


#endif


