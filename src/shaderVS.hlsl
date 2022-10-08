struct VSInput
{
  float4 Position : POSITION;
  float3 Normal : NORMAL;
};
struct VSOutput
{
  float4 Position : SV_POSITION;
  float4 Normal : NORMAL;
};

cbuffer ShaderParameter : register(b0)
{
  float4x4 world;
  float4x4 view;
  float4x4 proj;
}

VSOutput main( VSInput In )
{
  VSOutput result = (VSOutput)0;
  float4x4 mtxWVP = mul(world, mul(view, proj));
  result.Position = mul(In.Position, mtxWVP);
  //result.Position = In.Position;
  result.Normal = mul(In.Normal, world);
  return result;
}