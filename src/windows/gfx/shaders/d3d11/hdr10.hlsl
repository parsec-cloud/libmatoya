float spow(float x, float p)
{
	return sign(x) * pow(abs(x), p);
}

float3 spow3(float3 v, float p)
{
	return float3(spow(v.x, p), spow(v.y, p), spow(v.z, p));
}

static const float PQ_m_1 = 2610.0f / 4096.0f / 4.0f;
static const float PQ_m_1_d = 1.0f / PQ_m_1;
static const float PQ_m_2 = 2523.0f / 4096.0f * 128.0f;
static const float PQ_m_2_d = 1.0f / PQ_m_2;
static const float PQ_c_1 = 3424.0f / 4096.0f;
static const float PQ_c_2 = 2413.0f / 4096.0f * 32.0f;
static const float PQ_c_3 = 2392.0f / 4096.0f * 32.0f;

static const float HDR10_MAX_NITS = 10000.0f;
static const float SDR_MAX_NITS = 80.0f; // the reference sRGB luminance is 80 nits (aka the brightness of paper white)

float3 rec2020_pq_to_rec2020_linear(float3 color)
{
	// Apply the PQ EOTF (SMPTE ST 2084-2014) in order to linearize it
	// Courtesy of https://github.com/colour-science/colour/blob/38782ac059e8ddd91939f3432bf06811c16667f0/colour/models/rgb/transfer_functions/st_2084.py#L126

	float3 V_p = spow3(color, PQ_m_2_d);

	float3 n = max(0, V_p - PQ_c_1);

	float3 L = spow3(n / (PQ_c_2 - PQ_c_3 * V_p), PQ_m_1_d);
	float3 C = L * HDR10_MAX_NITS / SDR_MAX_NITS;

	return C;
}

float3 rec2020_linear_to_rec2020_pq(float3 color)
{
	// Apply the inverse of the PQ EOTF (SMPTE ST 2084-2014) in order to encode the signal as PQ
	// Courtesy of https://github.com/colour-science/colour/blob/38782ac059e8ddd91939f3432bf06811c16667f0/colour/models/rgb/transfer_functions/st_2084.py#L56

	float3 Y_p = spow3(max(0.0f, (color / HDR10_MAX_NITS) * SDR_MAX_NITS), PQ_m_1);

	float3 N = spow3((PQ_c_1 + PQ_c_2 * Y_p) / (PQ_c_3 * Y_p + 1.0f), PQ_m_2);

	return N;
}

/////// RGB <-> ICtCp (courtesy of https://en.wikipedia.org/wiki/ICtCp#Derivation)

static const float3x3 REC2020_to_LMS =
{
	{1688.0f, 2146.0f,  262.0f},
	{ 683.0f, 2951.0f,  462.0f},
	{  99.0f,  309.0f, 3688.0f}
};

static const float3x3 LMS_to_REC2020 =
{
	{ 0.000839015, -0.000611927,  0.000017052},
	{-0.000193196,  0.000484277, -0.000046941},
	{-0.000006335, -0.000024149,  0.000274625}
};

static const float3x3 LMS_to_ICTCP =
{
	{ 2048.0f,   2048.0f,    0.0f},
	{ 6610.0f, -13613.0f, 7003.0f},
	{17933.0f, -17390.0f, -543.0f}
};

static const float3x3 ICTCP_to_LMS =
{
	{0.000244141f,  0.000002102f,  0.000027107f},
	{0.000244141f, -0.000002102f, -0.000027107f},
	{0.000244141f,  0.000136726f, -0.000078278f}
};

float3 rec2020_linear_to_ictcp(float3 color)
{
	float3 lms = mul(REC2020_to_LMS, color);
	lms = lms / 4096.0f;

	// Apply non-linearity
	lms = rec2020_linear_to_rec2020_pq(lms);

	float3 ictcp = mul(LMS_to_ICTCP, lms);
	ictcp = ictcp / 4096.0f;
	return ictcp;
}

float3 ictcp_to_rec2020_linear(float3 ictcp)
{
	ictcp = ictcp * 4096.0f;

	float3 lms = mul(ICTCP_to_LMS, ictcp);

	// Remove non-linearity
	lms = rec2020_pq_to_rec2020_linear(lms);

	float3 color = lms * 4096.0f;
	color = mul(LMS_to_REC2020, color);
	return color;
}
