/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "cc/CCLayerImpl.h"

#include "GraphicsContext3D.h"
#include "LayerChromium.h"
#include "LayerRendererChromium.h"
#include "cc/CCLayerSorter.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

CCLayerImpl::CCLayerImpl(int id)
    : m_parent(0)
    , m_layerId(id)
    , m_anchorPoint(0.5, 0.5)
    , m_anchorPointZ(0)
    , m_doubleSided(true)
    , m_masksToBounds(false)
    , m_opacity(1.0)
    , m_preserves3D(false)
    , m_usesLayerScissor(false)
    , m_isNonCompositedContent(false)
    , m_drawsContent(false)
    , m_targetRenderSurface(0)
    , m_drawDepth(0)
    , m_drawOpacity(0)
    , m_debugBorderColor(0, 0, 0, 0)
    , m_debugBorderWidth(0)
{
}

CCLayerImpl::~CCLayerImpl()
{
}

void CCLayerImpl::addChild(PassRefPtr<CCLayerImpl> child)
{
    child->setParent(this);
    m_children.append(child);
}

void CCLayerImpl::removeFromParent()
{
    if (!m_parent)
        return;
    for (size_t i = 0; i < m_parent->m_children.size(); ++i) {
        if (m_parent->m_children[i].get() == this) {
            m_parent->m_children.remove(i);
            break;
        }
    }
    m_parent = 0;
}

void CCLayerImpl::removeAllChildren()
{
    while (m_children.size())
        m_children[0]->removeFromParent();
}

void CCLayerImpl::clearChildList()
{
    m_children.clear();
}

void CCLayerImpl::createRenderSurface()
{
    ASSERT(!m_renderSurface);
    m_renderSurface = adoptPtr(new CCRenderSurface(this));
}

bool CCLayerImpl::descendantDrawsContent()
{
    for (size_t i = 0; i < m_children.size(); ++i) {
        if (m_children[i]->drawsContent() || m_children[i]->descendantDrawsContent())
            return true;
    }
    return false;
}

void CCLayerImpl::draw(LayerRendererChromium*)
{
    ASSERT_NOT_REACHED();
}

void CCLayerImpl::bindContentsTexture(LayerRendererChromium*)
{
    ASSERT_NOT_REACHED();
}

void CCLayerImpl::cleanupResources()
{
    if (renderSurface())
        renderSurface()->cleanupResources();
}

const IntRect CCLayerImpl::getDrawRect() const
{
    // Form the matrix used by the shader to map the corners of the layer's
    // bounds into the view space.
    FloatRect layerRect(-0.5 * bounds().width(), -0.5 * bounds().height(), bounds().width(), bounds().height());
    IntRect mappedRect = enclosingIntRect(drawTransform().mapRect(layerRect));
    return mappedRect;
}

void CCLayerImpl::drawDebugBorder(LayerRendererChromium* layerRenderer)
{
    static float glMatrix[16];
    if (!debugBorderColor().alpha())
        return;

    GraphicsContext3D* context = layerRenderer->context();
    const LayerChromium::BorderProgram* program = layerRenderer->borderProgram();
    ASSERT(program && program->initialized());
    GLC(context, context->useProgram(program->program()));

    TransformationMatrix renderMatrix = drawTransform();
    renderMatrix.scale3d(bounds().width(), bounds().height(), 1);
    LayerRendererChromium::toGLMatrix(&glMatrix[0], layerRenderer->projectionMatrix() * renderMatrix);
    GLC(context, context->uniformMatrix4fv(program->vertexShader().matrixLocation(), false, &glMatrix[0], 1));

    GLC(context, context->uniform4f(program->fragmentShader().colorLocation(), debugBorderColor().red() / 255.0, debugBorderColor().green() / 255.0, debugBorderColor().blue() / 255.0, 1));

    GLC(context, context->lineWidth(debugBorderWidth()));

    // The indices for the line are stored in the same array as the triangle indices.
    GLC(context, context->drawElements(GraphicsContext3D::LINE_LOOP, 4, GraphicsContext3D::UNSIGNED_SHORT, 6 * sizeof(unsigned short)));
}

void CCLayerImpl::writeIndent(TextStream& ts, int indent)
{
    for (int i = 0; i != indent; ++i)
        ts << "  ";
}

void CCLayerImpl::dumpLayerProperties(TextStream& ts, int indent) const
{
    writeIndent(ts, indent);
    ts << "bounds: " << bounds().width() << ", " << bounds().height() << "\n";

    if (m_targetRenderSurface) {
        writeIndent(ts, indent);
        ts << "targetRenderSurface: " << m_targetRenderSurface->name() << "\n";
    }

    writeIndent(ts, indent);
    ts << "drawTransform: ";
    ts << m_drawTransform.m11() << ", " << m_drawTransform.m12() << ", " << m_drawTransform.m13() << ", " << m_drawTransform.m14() << ", ";
    ts << m_drawTransform.m21() << ", " << m_drawTransform.m22() << ", " << m_drawTransform.m23() << ", " << m_drawTransform.m24() << ", ";
    ts << m_drawTransform.m31() << ", " << m_drawTransform.m32() << ", " << m_drawTransform.m33() << ", " << m_drawTransform.m34() << ", ";
    ts << m_drawTransform.m41() << ", " << m_drawTransform.m42() << ", " << m_drawTransform.m43() << ", " << m_drawTransform.m44() << "\n";
}

void sortLayers(Vector<RefPtr<CCLayerImpl> >::iterator first, Vector<RefPtr<CCLayerImpl> >::iterator end, CCLayerSorter* layerSorter)
{
    TRACE_EVENT("LayerRendererChromium::sortLayers", 0, 0);
    layerSorter->sort(first, end);
}

String CCLayerImpl::layerTreeAsText() const
{
    TextStream ts;
    dumpLayer(ts, 0);
    return ts.release();
}

void CCLayerImpl::dumpLayer(TextStream& ts, int indent) const
{
    writeIndent(ts, indent);
    ts << layerTypeAsString() << "(" << m_name << ")\n";
    dumpLayerProperties(ts, indent+2);
    if (m_replicaLayer) {
        writeIndent(ts, indent+2);
        ts << "Replica:\n";
        m_replicaLayer->dumpLayer(ts, indent+3);
    }
    if (m_maskLayer) {
        writeIndent(ts, indent+2);
        ts << "Mask:\n";
        m_maskLayer->dumpLayer(ts, indent+3);
    }
    for (size_t i = 0; i < m_children.size(); ++i)
        m_children[i]->dumpLayer(ts, indent+1);
}

}


#endif // USE(ACCELERATED_COMPOSITING)
