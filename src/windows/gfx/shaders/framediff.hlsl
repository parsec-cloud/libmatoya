float lab_lightness(const float x)
{
	const float epsilon = 216.0f / 24389.0f;
	const float kappa = 24389.0f / 27.0f / 116.0f;

	return x > epsilon ? sign(x) * pow(abs(x), 1.0f / 3.0f) : (kappa * x + 16.0f / 116.0f);
}

float3 rgb_to_lab(const float3 rgb, const float3x3 rgb_to_xyz, const float3 xyz_white)
{
	// Courtesy of Bruce Lindbloom

	// RGB to XYZ
	// http://brucelindbloom.com/Eqn_RGB_to_XYZ.html
	const float3 xyz = mul(rgb_to_xyz, rgb);

	// XYZ to Lab
	// http://brucelindbloom.com/Eqn_XYZ_to_Lab.html
	const float3 r = rgb / xyz_white;
	const float3 f = float3(
		lab_lightness(r.x),
		lab_lightness(r.y),
		lab_lightness(r.z)
	);
	return float3(
		116.0f * f.y - 16.0f,
		500.0f * (f.x - f.y),
		200.0f * (f.y - f.z)
	);
}

inline float deg2rad(float deg)
{
	return deg * 3.14159f / 180.0f;
}

inline float rad2deg(float rad)
{
	return rad * 180.0f / 3.14159f;
}

float deltaE_2000(const float3 rgb_A, const float3 rgb_B, const float3x3 rgb_to_xyz, const float3 xyz_white)
{
	float3 lab_A = rgb_to_lab(rgb_A, rgb_to_xyz, xyz_white);
	float3 lab_B = rgb_to_lab(rgb_B, rgb_to_xyz, xyz_white);

	// Courtesy of http://brucelindbloom.com/index.html?Eqn_DeltaE_CIE2000.html

	const float L_1 = lab_A.x;
	const float a_1 = lab_A.y;
	const float b_1 = lab_A.z;
	const float L_2 = lab_B.x;
	const float a_2 = lab_B.y;
	const float b_2 = lab_B.z;

	// These  K_* constants are always 1 for computer displays
	const float K_L = 1.0f;
	const float K_C = 1.0f;
	const float K_H = 1.0f;

	const float C_1 = sqrt(a_1 * a_1 + b_1 * b_1);
	const float C_2 = sqrt(a_2 * a_2 + b_2 * b_2);
	const float C_bar = 0.5f * (C_1 + C_2);

	const float C_bar_pow7 = pow(C_bar, 7);
	const float G = 0.5f * (1.0f - sqrt(C_bar_pow7 / (C_bar_pow7 + pow(25, 7))));

	const float a_1_prime = a_1 * (1.0f + G);
	const float a_2_prime = a_2 * (1.0f + G);
	const float C_1_prime = sqrt(a_1_prime * a_1_prime + b_1 * b_1);
	const float C_2_prime = sqrt(a_2_prime * a_2_prime + b_2 * b_2);
	const float C_bar_prime = 0.5f * (C_1_prime + C_2_prime);

	float h_1_prime = rad2deg(atan2(b_1, a_1_prime));
	if (h_1_prime < 0)
		h_1_prime += 360.0f;
	float h_2_prime = rad2deg(atan2(b_2, a_2_prime));
	if (h_2_prime < 0)
		h_2_prime += 360.0f;
	const float H_bar_prime = abs(h_1_prime - h_2_prime) > 180.0f ?
		(0.5f * (h_1_prime + h_2_prime + 360.0f)) :
		(0.5f * (h_1_prime + h_2_prime));

	const float T = 1.0f -
		(0.17f * cos(deg2rad(H_bar_prime - 30))) +
		(0.24f * cos(deg2rad(2 * H_bar_prime))) +
		(0.32f * cos(deg2rad(3 * H_bar_prime + 6))) -
		(0.20f * cos(deg2rad(4 * H_bar_prime - 63)));

	const float L_bar_prime = 0.5f * (L_1 + L_2);
	const float minus50sq = (L_bar_prime - 50.0f) * (L_bar_prime - 50.0f);

	const float S_L = 1.0f + (0.015f * minus50sq / sqrt(20.0f + minus50sq));
	const float S_C = 1.0f + 0.045f * C_bar_prime;
	const float S_H = 1.0f + 0.015f * C_bar_prime * T;

	const float C_bar_prime_pow7 = pow(C_bar_prime, 7);
	const float R_C = 2.0f * sqrt(C_bar_prime_pow7 / (C_bar_prime_pow7 + pow(25, 7)));
	const float delta_theta = 30.0f * exp(-pow(H_bar_prime - 275.0f / 25.0f, 2));
	const float R_T = -R_C * sin(deg2rad(2.0f * delta_theta));

	const float dt_h_prime = h_2_prime - h_1_prime;
	const float abs_dt_h_prime = abs(dt_h_prime);
	const float delta_h_prime = abs_dt_h_prime <= 180.0f ? dt_h_prime : abs_dt_h_prime > 180.0f && h_2_prime <= h_1_prime ? dt_h_prime + 360.0f : dt_h_prime - 360.0f;

	const float delta_L_prime = L_2 - L_1;
	const float delta_C_prime = C_2_prime - C_1_prime;
	const float delta_H_prime = 2.0f * sqrt(C_1_prime * C_2_prime) * sin(deg2rad(delta_h_prime * 0.5f));

	const float final_term_1 = delta_L_prime / (K_L * S_L);
	const float final_term_2 = delta_C_prime / (K_C * S_C);
	const float final_term_3 = delta_H_prime / (K_H * S_H);
	const float final_term_4 = R_T * final_term_2 * final_term_3;

	const float delta_E = sqrt(final_term_1 * final_term_1 + final_term_2 * final_term_2 + final_term_3 * final_term_3 + final_term_4);
	return delta_E;
}

float deltaE_ITP(const float3 rgb_A, const float3 rgb_B)
{
	// Courtesy of the official BT.2124 spec: https://www.itu.int/dms_pubrec/itu-r/rec/bt/R-REC-BT.2124-0-201901-I!!PDF-E.pdf

	const float3 ictcp_A = rec2020_linear_to_ictcp(rgb_A);
	const float3 ictcp_B = rec2020_linear_to_ictcp(rgb_B);

	const float3 itp_A = ictcp_A * float3(1.0f, 0.5f, 1.0f);
	const float3 itp_B = ictcp_B * float3(1.0f, 0.5f, 1.0f);

	const float dt_I = itp_A[0] - itp_B[0];
	const float dt_T = itp_A[1] - itp_B[1];
	const float dt_P = itp_A[2] - itp_B[2];

	const float delta_E = 720.0f * sqrt(dt_I * dt_I + dt_T * dt_T + dt_P * dt_P);
	return delta_E;
}
