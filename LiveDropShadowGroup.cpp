//========================================================================================
//
//  $File$
//
//  $Revision$
//
//  Copyright 1987 Adobe Systems Incorporated. All rights reserved.
//
//========================================================================================

#include "IllustratorSDK.h"
#include "LiveDropShadowGroup.h"
#include "LiveDropShadowSuites.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>

// --- Suite Pointer Declarations ---
extern "C" AIArtSuite* sAIArt;
extern "C" AIPathStyleSuite* sAIPathStyle;
extern "C" AIPluginGroupSuite* sAIPluginGroup;

// Define sAIPath globally to resolve the linker error
extern "C" AIPathSuite* sAIPath = nullptr;

// --- Helper: Native C++ Bézier Oval Generator ---
static void CreateOvalPath(AIArtHandle pathArt, AIRealPoint center, float rx, float ry) {
    if (!pathArt) return;

    const float k = 0.55228475f;
    float kx = rx * k;
    float ky = ry * k;

    AIPathSegment segs[4];

    segs[0].p.h = center.h + rx;       segs[0].p.v = center.v;
    segs[0].in.h = center.h + rx;      segs[0].in.v = center.v - ky;
    segs[0].out.h = center.h + rx;     segs[0].out.v = center.v + ky;
    segs[0].corner = false;

    segs[1].p.h = center.h;            segs[1].p.v = center.v + ry;
    segs[1].in.h = center.h + kx;      segs[1].in.v = center.v + ry;
    segs[1].out.h = center.h - kx;     segs[1].out.v = center.v + ry;
    segs[1].corner = false;

    segs[2].p.h = center.h - rx;       segs[2].p.v = center.v;
    segs[2].in.h = center.h - rx;      segs[2].in.v = center.v + ky;
    segs[2].out.h = center.h - rx;     segs[2].out.v = center.v - ky;
    segs[2].corner = false;

    segs[3].p.h = center.h;            segs[3].p.v = center.v - ry;
    segs[3].in.h = center.h - kx;      segs[3].in.v = center.v - ry;
    segs[3].out.h = center.h + kx;     segs[3].out.v = center.v - ry;
    segs[3].corner = false;

    if (sAIPath) {
        sAIPath->SetPathSegments(pathArt, 0, 4, segs);
        sAIPath->SetPathClosed(pathArt, true);
    }
}

// --- Helper Functions for Curve Math ---
static AIRealPoint EvaluateBezierPoint(AIRealPoint p0, AIRealPoint c0, AIRealPoint c1, AIRealPoint p1, float t) {
    float u = 1.0f - t;
    float tt = t * t, uu = u * u;
    float uuu = uu * u, ttt = tt * t;

    AIRealPoint pt;
    pt.h = uuu * p0.h + 3.0f * uu * t * c0.h + 3.0f * u * tt * c1.h + ttt * p1.h;
    pt.v = uuu * p0.v + 3.0f * uu * t * c0.v + 3.0f * u * tt * c1.v + ttt * p1.v;
    return pt;
}

static AIRealPoint EvaluateBezierTangent(AIRealPoint p0, AIRealPoint c0, AIRealPoint c1, AIRealPoint p1, float t) {
    float u = 1.0f - t;
    AIRealPoint tan;
    tan.h = 3.0f * u * u * (c0.h - p0.h) + 6.0f * u * t * (c1.h - c0.h) + 3.0f * t * t * (p1.h - c1.h);
    tan.v = 3.0f * u * u * (c0.v - p0.v) + 6.0f * u * t * (c1.v - c0.v) + 3.0f * t * t * (p1.v - c1.v);
    return tan;
}

// --- Plugin Group Update Handler (The Math Engine) ---
ASErr LiveDropShadowGroup::PluginGroupUpdate(AIPluginGroupMessage* message)
{
    ASErr error = kNoErr;
    AIArtHandle pluginArt = message->art;

    // Safely acquire the Path Suite if it wasn't loaded by the sample framework
    if (!sAIPath) {
        message->d.basic->AcquireSuite(kAIPathSuite, kAIPathVersion, (const void**)&sAIPath);
    }
    if (!sAIPath) return kNoErr;

    const float stepSize = 2.0f;
    const float jitterAmount = 1.8f;
    const float dotRadius = 0.8f;

    AIArtHandle inputPath = nullptr;
    error = sAIArt->GetArtFirstChild(pluginArt, &inputPath);
    if (error || !inputPath) return error;

    short artType = kUnknownArt;
    sAIArt->GetArtType(inputPath, &artType);

    if (artType == kPathArt) {
        short segmentCount = 0;
        sAIPath->GetPathSegmentCount(inputPath, &segmentCount);

        if (segmentCount < 2) return kNoErr;

        AIPathStyle originalStyle;
        sAIPathStyle->GetPathStyle(inputPath, &originalStyle, nullptr);

        for (short i = 0; i < segmentCount - 1; ++i) {
            AIPathSegment seg0, seg1;
            sAIPath->GetPathSegments(inputPath, i, 1, &seg0);
            sAIPath->GetPathSegments(inputPath, i + 1, 1, &seg1);

            AIRealPoint p0 = seg0.p;
            AIRealPoint c0 = seg0.out;
            AIRealPoint c1 = seg1.in;
            AIRealPoint p1 = seg1.p;

            float segmentLength = 0.0f;
            AIRealPoint prevPt = p0;
            const int subSamples = 16;

            for (int s = 1; s <= subSamples; ++s) {
                float t = (float)s / subSamples;
                AIRealPoint currPt = EvaluateBezierPoint(p0, c0, c1, p1, t);
                float dx = currPt.h - prevPt.h;
                float dy = currPt.v - prevPt.v;
                segmentLength += std::sqrt(dx * dx + dy * dy);
                prevPt = currPt;
            }

            int steps = std::max(1, static_cast<int>(segmentLength / stepSize));

            for (int s = 0; s < steps; ++s) {
                float t = (float)s / steps;
                AIRealPoint pos = EvaluateBezierPoint(p0, c0, c1, p1, t);
                AIRealPoint tangent = EvaluateBezierTangent(p0, c0, c1, p1, t);

                float tanLen = std::sqrt(tangent.h * tangent.h + tangent.v * tangent.v);
                AIRealPoint normal = { 0.0f, 0.0f };
                if (tanLen > 0.0f) {
                    normal.h = -tangent.v / tanLen;
                    normal.v = tangent.h / tanLen;
                }

                float offset = ((float)rand() / RAND_MAX - 0.5f) * 2.0f * jitterAmount;
                AIRealPoint grainPos = {
                    pos.h + normal.h * offset,
                    pos.v + normal.v * offset
                };

                AIArtHandle newGrain = nullptr;
                sAIArt->NewArt(kPathArt, kPlaceInsideOnBottom, pluginArt, &newGrain);

                CreateOvalPath(newGrain, grainPos, dotRadius, dotRadius);

                AIPathStyle grainStyle;
                sAIPathStyle->GetInitialPathStyle(&grainStyle);
                grainStyle.fillPaint = true;
                if (originalStyle.strokePaint) {
                    grainStyle.fill.color = originalStyle.stroke.color;
                }
                grainStyle.strokePaint = false;
                sAIPathStyle->SetPathStyle(newGrain, &grainStyle);
            }
        }
    }
    return kNoErr;
}

// --- EXACT SIGNATURE MATCHES FOR ALL DECLARED HEADER FUNCTIONS ---

// Public Methods
ASErr LiveDropShadowGroup::StartLensObject(AIArtHandle* object, ObjectData* data) { return kNoErr; }
ASErr LiveDropShadowGroup::FinishLensObject(AIArtHandle* object, ObjectData* data) { return kNoErr; }
ASErr LiveDropShadowGroup::appendPluginGroup(AIArtHandle* object, ObjectData* data, AIArtHandle path, AIPluginGroupHandle& pluginGroupHandle) { return kNoErr; }
ASErr LiveDropShadowGroup::ReleaseLensObject(AIArtHandle plugin) { return kNoErr; }
ASErr LiveDropShadowGroup::FirstLensObject(AIArtHandle* object, ObjectData* data, AIPluginGroupHandle& pluginGroupHandle) { return kNoErr; }
ASErr LiveDropShadowGroup::LastLensObject(AIArtHandle* object, ObjectData* data) { return kNoErr; }
ASErr LiveDropShadowGroup::ModifyLensObject(AIArtHandle* object, ObjectData* data, AIArtHandle plugin) { return kNoErr; }

// Private Methods
ASErr LiveDropShadowGroup::EditLensObject(ObjectData* data) { return kNoErr; }
ASErr LiveDropShadowGroup::SetFilteredFill(AIBoolean* paint, AIFillStyle* fill) { return kNoErr; }
ASErr LiveDropShadowGroup::SetFilteredStroke(AIBoolean* paint, AIStrokeStyle* stroke) { return kNoErr; }
ASErr LiveDropShadowGroup::SetFilteredStyle(AIArtHandle object) { return kNoErr; }
ASErr LiveDropShadowGroup::SetBackingFill(AIBoolean* paint, AIFillStyle* fill) { return kNoErr; }
ASErr LiveDropShadowGroup::SetBackingStroke(AIBoolean* paint, AIStrokeStyle* stroke) { return kNoErr; }
ASErr LiveDropShadowGroup::SetBackingStyle(AIArtHandle object) { return kNoErr; }
ASErr LiveDropShadowGroup::SetBoundingFill(AIBoolean* paint, AIFillStyle* fill) { return kNoErr; }
ASErr LiveDropShadowGroup::SetBoundingStroke(AIBoolean* paint, AIStrokeStyle* stroke) { return kNoErr; }
ASErr LiveDropShadowGroup::SetBoundingStyle(AIArtHandle object) { return kNoErr; }
ASErr LiveDropShadowGroup::SetClippingStyle(AIArtHandle object) { return kNoErr; }
ASErr LiveDropShadowGroup::DuplicateCoveredArt(AIArtHandle source, AIArtHandle destination, AIRealRect* bounds, AIArtHandle start, AIBoolean* found) { return kNoErr; }
ASErr LiveDropShadowGroup::ReportError(AIErr result) { return kNoErr; }
ASErr LiveDropShadowGroup::setDropShadowStyle(AIArtHandle object) { return kNoErr; }
ASErr LiveDropShadowGroup::setDropShadowFill(AIFillStyle* fill) { return kNoErr; }
ASErr LiveDropShadowGroup::setDropShadowStroke(AIStrokeStyle* stroke) { return kNoErr; }
ASErr LiveDropShadowGroup::transformDropShadow(AIArtHandle art, AIReal tx, AIReal ty, AIReal theta, AIReal sx, AIReal sy) { return kNoErr; }
