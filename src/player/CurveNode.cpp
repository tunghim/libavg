//
//  libavg - Media Playback Engine. 
//  Copyright (C) 2003-2014 Ulrich von Zadow
//
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public
//  License along with this library; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//  Current versions can be found at www.libavg.de
//

#include "CurveNode.h"

#include "TypeDefinition.h"
#include "TypeRegistry.h"

#include "../base/Exception.h"
#include "../base/MathHelper.h"

#include "../graphics/VertexData.h"

#include <math.h>
#include <float.h>
#include <iostream>
#include <sstream>

using namespace std;

namespace avg {

void CurveNode::registerType()
{
    TypeDefinition def = TypeDefinition("curve", "vectornode", 
            ExportedObject::buildObject<CurveNode>)
        .addArg(Arg<glm::vec2>("pos1", glm::vec2(0,0)))
        .addArg(Arg<glm::vec2>("pos2", glm::vec2(0,0)))
        .addArg(Arg<glm::vec2>("pos3", glm::vec2(0,0)))
        .addArg(Arg<glm::vec2>("pos4", glm::vec2(0,0)))
        .addArg(Arg<float>("texcoord1", 0, true, offsetof(CurveNode, m_TC1)))
        .addArg(Arg<float>("texcoord2", 1, true, offsetof(CurveNode, m_TC2)));
    TypeRegistry::get()->registerType(def);
}

CurveNode::CurveNode(const ArgList& args)
   : VectorNode(args)
{
    args.setMembers(this);
    glm::vec2 p0 = args.getArgVal<glm::vec2>("pos1");
    glm::vec2 p1 = args.getArgVal<glm::vec2>("pos2");
    glm::vec2 p2 = args.getArgVal<glm::vec2>("pos3");
    glm::vec2 p3 = args.getArgVal<glm::vec2>("pos4");
    m_pCurve = BezierCurvePtr(new BezierCurve(p0, p1, p2, p3));
}

CurveNode::~CurveNode()
{
}

const glm::vec2& CurveNode::getPos1() const 
{
    return m_pCurve->getPt(0);
}

void CurveNode::setPos1(const glm::vec2& pt) 
{
    m_pCurve->setPt(0, pt);
    setDrawNeeded();
}

const glm::vec2& CurveNode::getPos2() const 
{
    return m_pCurve->getPt(1);
}

void CurveNode::setPos2(const glm::vec2& pt) 
{
    m_pCurve->setPt(1, pt);
    setDrawNeeded();
}

const glm::vec2& CurveNode::getPos3() const 
{
    return m_pCurve->getPt(2);
}

void CurveNode::setPos3(const glm::vec2& pt) 
{
    m_pCurve->setPt(2, pt);
    setDrawNeeded();
}

const glm::vec2& CurveNode::getPos4() const 
{
    return m_pCurve->getPt(3);
}

void CurveNode::setPos4(const glm::vec2& pt) 
{
    m_pCurve->setPt(3, pt);
    setDrawNeeded();
}

float CurveNode::getTexCoord1() const
{
    return m_TC1;
}

void CurveNode::setTexCoord1(float tc)
{
    m_TC1 = tc;
    setDrawNeeded();
}

float CurveNode::getTexCoord2() const
{
    return m_TC2;
}

void CurveNode::setTexCoord2(float tc)
{
    m_TC2 = tc;
    setDrawNeeded();
}
 
int CurveNode::getCurveLen() const
{
    return int(m_pCurve->estimateLen());
}

glm::vec2 CurveNode::getPtOnCurve(float t) const
{
    return m_pCurve->interpolate(t);
}

void CurveNode::calcVertexes(const VertexDataPtr& pVertexData, Pixel32 color)
{
    updateLines();
    
    pVertexData->appendPos(m_LeftCurve[0], glm::vec2(m_TC1,1), color);
    pVertexData->appendPos(m_RightCurve[0], glm::vec2(m_TC2,0), color);
    for (unsigned i = 0; i < m_LeftCurve.size()-1; ++i) {
        float ratio = i/float(m_LeftCurve.size());
        float tc = (1-ratio)*m_TC1+ratio*m_TC2;
        pVertexData->appendPos(m_LeftCurve[i+1], glm::vec2(tc,1), color);
        pVertexData->appendPos(m_RightCurve[i+1], glm::vec2(tc,0), color);
        pVertexData->appendQuadIndexes((i+1)*2, i*2, (i+1)*2+1, i*2+1);
    }
}

void CurveNode::updateLines()
{
    float len = float(getCurveLen());
    m_LeftCurve.clear();
    m_RightCurve.clear();
    m_LeftCurve.reserve(int(len+1.5f));
    m_RightCurve.reserve(int(len+1.5f));

    for (unsigned i = 0; i < len; ++i) {
        float t = i/len;
        addLRCurvePoint(m_pCurve->interpolate(t), m_pCurve->getDeriv(t));
    }
    addLRCurvePoint(m_pCurve->interpolate(1), m_pCurve->getDeriv(1));
}

void CurveNode::addLRCurvePoint(const glm::vec2& pos, const glm::vec2& deriv)
{
    glm::vec2 m = glm::normalize(deriv);
    glm::vec2 w = glm::vec2(m.y, -m.x)*float(getStrokeWidth()/2);
    m_LeftCurve.push_back(pos-w);
    m_RightCurve.push_back(pos+w);
}

}
