struct VSOutput
{
  float4 Position : SV_POSITION;
  float4 Normal : NORMAL;
};

Texture2D tex : register(t0);
SamplerState samp : register(s0);

float4 main(VSOutput In) : SV_TARGET
{
	float3 light = normalize(float3(1, -1, 1));
	float4 color = (dot(-light, In.Normal), 1.0f);

  return color;
}