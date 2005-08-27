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

#include "libavoid/vertices.h"
#include "libavoid/geometry.h"
#include "libavoid/graph.h"  // For alertConns
#include "libavoid/debug.h"

#include <iostream>
#include <cstdlib>
#include <cassert>


namespace Avoid {


ContainsMap contains;

    
VertID::VertID()
{
}


VertID::VertID(int s, int n)
    : shape(s)
    , vn(n)
{
}


VertID::VertID(const VertID& other)
    : shape(other.shape)
    , vn(other.vn)
{
}


VertID& VertID::operator= (const VertID& rhs)
{
    // Gracefully handle self assignment
    //if (this == &rhs) return *this;
       
    shape = rhs.shape;
    vn = rhs.vn;

    return *this;
}


bool VertID::operator==(const VertID& rhs) const
{
    if ((shape != rhs.shape) || (vn != rhs.vn))
    {
        return false;
    }
    return true;
}


bool VertID::operator!=(const VertID& rhs) const
{
    if ((shape != rhs.shape) || (vn != rhs.vn))
    {
        return true;
    }
    return false;
}


bool VertID::operator<(const VertID& rhs) const
{
    if ((shape < rhs.shape) ||
            ((shape == rhs.shape) && (vn < rhs.vn)))
    {
        return true;
    }
    return false;
}


VertID VertID::operator+(const int& rhs) const
{
    return VertID(shape, vn + rhs);
}


VertID VertID::operator-(const int& rhs) const
{
    return VertID(shape, vn - rhs);
}


VertID& VertID::operator++(int)
{
    vn += 1;
    return *this;
}


void VertID::print(FILE *file) const
{
    fprintf(file, "[%2d,%1d]", shape, vn);
}


void VertID::db_print(void) const
{
    db_printf("[%2d,%1d]", shape, vn);
}


const int VertID::src = 1;
const int VertID::tar = 2;

const VertID VertID::nullID = VertID(INT_MIN, 0);


VertInf::VertInf(const VertID& vid, const Point& vpoint)
    : id(vid)
    , point(vpoint)
    , lstPrev(NULL)
    , lstNext(NULL)
    , shPrev(NULL)
    , shNext(NULL)
    , visListSize(0)
    , invisListSize(0)
    , pathNext(NULL)
    , pathDist(0)
{
}


void VertInf::Reset(const Point& vpoint)
{
    point = vpoint;
}


void VertInf::removeFromGraph(const bool isConnVert)
{
    if (isConnVert)
    {
        assert(id.shape <= 0);
    }

    VertInf *tmp = this;
    
    // For each vertex.
    EdgeInfList& visList = tmp->visList;
    EdgeInfList::iterator finish = visList.end();
    EdgeInfList::iterator edge;
    while ((edge = visList.begin()) != finish)
    {
        // Remove each visibility edge
        (*edge)->alertConns();
        delete (*edge);
    }

    EdgeInfList& invisList = tmp->invisList;
    finish = invisList.end();
    while ((edge = invisList.begin()) != finish)
    {
        // Remove each invisibility edge
        delete (*edge);
    }
}


bool directVis(VertInf *src, VertInf *dst)
{
    ShapeSet ss = ShapeSet();
    
    Point& p = src->point;
    Point& q = dst->point;
   
    VertID& pID = src->id;
    VertID& qID = dst->id;
    
    if (pID.shape <= 0)
    {
        ss.insert(contains[pID].begin(), contains[pID].end());
    }
    if (qID.shape <= 0)
    {
        ss.insert(contains[qID].begin(), contains[qID].end());
    }

    // The "beginning" should be the first shape vertex, rather
    // than an endpoint, which are also stored in "vertices".
    VertInf *endVert = vertices.end();
    for (VertInf *k = vertices.shapesBegin(); k != endVert; k = k->lstNext)
    {
        if ((ss.find(k->id.shape) == ss.end()))
        {
            if (segmentIntersect(p, q, k->point, k->shNext->point))
            {
                return false;
            }
        }
    }
    return true;
}


VertInfList::VertInfList()
    : _firstShapeVert(NULL)
    , _firstConnVert(NULL)
    , _lastShapeVert(NULL)
    , _lastConnVert(NULL)
    , _shapeVertices(0)
    , _connVertices(0)
{
}


#define checkVertInfListConditions() \
        do { \
            assert((!_firstConnVert && (_connVertices == 0)) || \
                    ((_firstConnVert->lstPrev == NULL) && (_connVertices > 0))); \
            assert((!_firstShapeVert && (_shapeVertices == 0)) || \
                    ((_firstShapeVert->lstPrev == NULL) && (_shapeVertices > 0))); \
            assert(!_lastShapeVert || (_lastShapeVert->lstNext == NULL)); \
            assert(!_lastConnVert || (_lastConnVert->lstNext == _firstShapeVert)); \
            assert((!_firstConnVert && !_lastConnVert) || \
                    (_firstConnVert &&  _lastConnVert) ); \
            assert((!_firstShapeVert && !_lastShapeVert) || \
                    (_firstShapeVert &&  _lastShapeVert) ); \
            assert(!_firstShapeVert || (_firstShapeVert->id.shape > 0)); \
            assert(!_lastShapeVert || (_lastShapeVert->id.shape > 0)); \
            assert(!_firstConnVert || (_firstConnVert->id.shape <= 0)); \
            assert(!_lastConnVert || (_lastConnVert->id.shape <= 0)); \
        } while(0)


void VertInfList::addVertex(VertInf *vert)
{
    checkVertInfListConditions();
    assert(vert->lstPrev == NULL);
    assert(vert->lstNext == NULL);

    if (vert->id.shape <= 0)
    {
        // A Connector vertex
        if (_firstConnVert)
        {
            // Join with previous front
            vert->lstNext = _firstConnVert;
            _firstConnVert->lstPrev = vert;
            
            // Make front
            _firstConnVert = vert;
        }
        else
        {
            // Make front and back
            _firstConnVert = vert;
            _lastConnVert = vert;
            
            // Link to front of shapes list
            vert->lstNext = _firstShapeVert;
        }
        _connVertices++;
    }
    else // if (vert->id.shape > 0)
    {
        // A Shape vertex
        if (_lastShapeVert)
        {
            // Join with previous back
            vert->lstPrev = _lastShapeVert;
            _lastShapeVert->lstNext = vert;
            
            // Make back
            _lastShapeVert = vert;
        }
        else
        {
            // Make first and last
            _firstShapeVert = vert;
            _lastShapeVert = vert;
            
            // Join with conns list
            if (_lastConnVert)
            {
                assert (_lastConnVert->lstNext == NULL);

                _lastConnVert->lstNext = vert;
            }
        }
        _shapeVertices++;
    }
    checkVertInfListConditions();
}


void VertInfList::removeVertex(VertInf *vert)
{
    // Conditions for correct data structure
    checkVertInfListConditions();
    
    if (vert->id.shape <= 0)
    {
        // A Connector vertex
        if (vert == _firstConnVert)
        {

            if (vert == _lastConnVert)
            {
                _firstConnVert = NULL;
                _lastConnVert = NULL;
            }
            else
            {
                // Set new first
                _firstConnVert = _firstConnVert->lstNext;

                if (_firstConnVert)
                {
                    // Set previous
                    _firstConnVert->lstPrev = NULL;
                }
            }
        }
        else if (vert == _lastConnVert)
        {
            // Set new last
            _lastConnVert = _lastConnVert->lstPrev;

            // Make last point to shapes list
            _lastConnVert->lstNext = _firstShapeVert;
        }
        else
        {
            vert->lstNext->lstPrev = vert->lstPrev;
            vert->lstPrev->lstNext = vert->lstNext;
        }
        _connVertices--;
    }
    else // if (vert->id.shape > 0)
    {
        // A Shape vertex
        if (vert == _lastShapeVert)
        {
            // Set new last
            _lastShapeVert = _lastShapeVert->lstPrev;

            if (vert == _firstShapeVert)
            {
                _firstShapeVert = NULL;
                if (_lastConnVert)
                {
                    _lastConnVert->lstNext = NULL;
                }
            }
            
            if (_lastShapeVert)
            {
                _lastShapeVert->lstNext = NULL;
            }
        }
        else if (vert == _firstShapeVert)
        {
            // Set new first
            _firstShapeVert = _firstShapeVert->lstNext;

            // Correct the last conn vertex
            if (_lastConnVert)
            {
                _lastConnVert->lstNext = _firstShapeVert;
            }
            
            if (_firstShapeVert)
            {
                _firstShapeVert->lstPrev = NULL;
            }
        }
        else
        {
            vert->lstNext->lstPrev = vert->lstPrev;
            vert->lstPrev->lstNext = vert->lstNext;
        }
        _shapeVertices--;
    }
    vert->lstPrev = NULL;
    vert->lstNext = NULL;
    
    checkVertInfListConditions();
}


VertInf *VertInfList::shapesBegin(void)
{
    return _firstShapeVert;
}


VertInf *VertInfList::connsBegin(void)
{
    if (_firstConnVert)
    {
        return _firstConnVert;
    }
    // No connector vertices
    return _firstShapeVert;
}


VertInf *VertInfList::end(void)
{
    return NULL;
}


VertInfList vertices;


}


