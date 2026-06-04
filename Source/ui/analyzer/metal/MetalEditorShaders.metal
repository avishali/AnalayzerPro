#include <metal_stdlib>
using namespace metal;

struct ChromeVertexIn
{
    float2 position [[attribute(0)]];
    float2 texCoord [[attribute(1)]];
};

struct ChromeVertexOut
{
    float4 position [[position]];
    float2 texCoord;
};

vertex ChromeVertexOut analyzerproChromeVertex (ChromeVertexIn in [[stage_in]])
{
    ChromeVertexOut out;
    out.position = float4 (in.position, 0.0, 1.0);
    out.texCoord = in.texCoord;
    return out;
}

fragment float4 analyzerproChromeFragment (ChromeVertexOut in [[stage_in]],
                                           texture2d<float> chrome [[texture(0)]],
                                           sampler chromeSampler [[sampler(0)]])
{
    return chrome.sample (chromeSampler, in.texCoord);
}

struct ColourVertexIn
{
    float2 position [[attribute(0)]];
    float4 colour [[attribute(1)]];
};

struct ColourVertexOut
{
    float4 position [[position]];
    float4 colour;
};

vertex ColourVertexOut analyzerproColourVertex (ColourVertexIn in [[stage_in]])
{
    ColourVertexOut out;
    out.position = float4 (in.position, 0.0, 1.0);
    out.colour = in.colour;
    return out;
}

fragment float4 analyzerproColourFragment (ColourVertexOut in [[stage_in]])
{
    return in.colour;
}
