#include <stdio.h>
#include <math.h>
#include <string.h>

int eraEpb2jd(double epb, double *djm0, double *djm);
int eraEpj2jd(double epj, double *djm0, double *djm);
void eraPnm06a(double date1, double date2, double rnpb[3][3]);

static void vec3_from_sphe_local(double ra, double de, double v[3]);

// Vondrak coefficient tables from desktop Stellarium precession.c
static const double precVals[18][7]=
{
  { 1.0/402.90,  -22206.325946,  1267.727824, -13765.924050,    -3243.236469, -8571.476251,  -2206.967126 },
  { 1.0/256.75,   12236.649447,  1702.324248,  13511.858383,    -3969.723769,  5309.796459,  -4186.752711 },
  { 1.0/292.00,   -1589.008343, -2970.553839,  -1455.229106,     7099.207893,  -610.393953,   6737.949677 },
  { 1.0/537.22,    2482.103195,   693.790312,   1054.394467,    -1903.696711,   923.201931,   -856.922846 },
  { 1.0/241.45,     150.322920,   -14.724451,      0.0     ,      146.435014,     3.759055,      0.0      },
  { 1.0/375.22,     -13.632066,  -516.649401,   -112.300144,     1300.630106,   -40.691114,    957.149088 },
  { 1.0/157.87,     389.437420,  -356.794454,    202.769908,     1727.498039,    80.437484,   1709.440735 },
  { 1.0/274.20,    2031.433792,  -129.552058,   1936.050095,      299.854055,   807.300668,    154.425505 },
  { 1.0/203.00,     363.748303,   256.129314,      0.0     ,    -1217.125982,    83.712326,      0.0      },
  { 1.0/440.00,    -896.747562,   190.266114,   -655.484214,     -471.367487,  -368.654854,   -243.520976 },
  { 1.0/170.72,    -926.995700,    95.103991,   -891.898637,     -441.682145,  -191.881064,   -406.539008 },
  { 1.0/713.37,      37.070667,  -332.907067,      0.0     ,      -86.169171,    -4.263770,      0.0      },
  { 1.0/313.00,    -597.682468,   131.337633,      0.0     ,     -308.320429,  -270.353691,      0.0      },
  { 1.0/128.38,      66.282812,    82.731919,   -333.322021,     -422.815629,    11.602861,   -446.656435 },
  { 1.0/202.00,       0.0     ,     0.0     ,    327.517465,        0.0     ,     0.0     ,  -1049.071786 },
  { 1.0/315.00,       0.0     ,     0.0     ,   -494.780332,        0.0     ,     0.0     ,   -301.504189 },
  { 1.0/136.32,       0.0     ,     0.0     ,    585.492621,        0.0     ,     0.0     ,     41.348740 },
  { 1.0/490.00,       0.0     ,     0.0     ,    110.512834,        0.0     ,     0.0     ,    142.525186 }};

static double arcsec2rad(void) { return M_PI * 2.0 / (360.0 * 3600.0); }

static void getPrecessionAnglesVondrak(double jde, double *eps, double *chi,
                                       double *omega, double *psi)
{
    double T = (jde - 2451545.0) * (1.0 / 36525.0);
    double T2pi = T * (2.0 * M_PI);
    int i;
    *psi = 0; *omega = 0; *chi = 0; *eps = 0;
    for (i = 0; i < 18; i++) {
        double invP = precVals[i][0];
        double phase = T2pi * invP;
        double s = sin(phase), c = cos(phase);
        *psi   += precVals[i][1]*c + precVals[i][4]*s;
        *omega += precVals[i][2]*c + precVals[i][5]*s;
        *chi   += precVals[i][3]*c + precVals[i][6]*s;
    }
    *psi   += (( 289.e-9*T - 0.00740913)*T + 5042.7980307)*T +  8473.343527;
    *omega += (( 151.e-9*T + 0.00000146)*T -    0.4436568)*T + 84283.175915;
    *chi   += (( -61.e-9*T + 0.00001472)*T +    0.0790159)*T -    19.657270;
    *psi *= arcsec2rad();
    *omega *= arcsec2rad();
    *chi *= arcsec2rad();
    // Desktop uses the fixed J2000 obliquity in the matrix, not epsilon_A.
    *eps = 84381.406 * arcsec2rad();
}

static void mat_xrot(double m[3][3], double a)
{
    m[0][0]=1; m[0][1]=0; m[0][2]=0;
    m[1][0]=0; m[1][1]=cos(a); m[1][2]=-sin(a);
    m[2][0]=0; m[2][1]=sin(a); m[2][2]=cos(a);
}
static void mat_zrot(double m[3][3], double a)
{
    m[0][0]=cos(a); m[0][1]=-sin(a); m[0][2]=0;
    m[1][0]=sin(a); m[1][1]=cos(a); m[1][2]=0;
    m[2][0]=0; m[2][1]=0; m[2][2]=1;
}
static void mat_mul(double a[3][3], double b[3][3], double out[3][3])
{
    int i, j, k;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++) {
            out[i][j] = 0;
            for (k = 0; k < 3; k++) out[i][j] += a[i][k] * b[k][j];
        }
}
static void mat_trans(double m[3][3], double out[3][3])
{
    int i, j;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++) out[i][j] = m[j][i];
}
static double mat_diff(double a[3][3], double b[3][3])
{
    int i, j; double s = 0;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++) s += fabs(a[i][j] - b[i][j]);
    return s;
}

// Reproduce the web engine skyculture_compute_edges_rot() logic.
static void edges_rot(const char *epoch, double out[3][3])
{
    double djm0 = 0.0, djm;
    double rnpb[3][3];
    if (!epoch || !epoch[0] || strcasecmp(epoch, "J2000") == 0) {
        int i, j;
        for (i = 0; i < 3; i++)
            for (j = 0; j < 3; j++) out[i][j] = (i == j) ? 1.0 : 0.0;
        return;
    }
    if (strcasecmp(epoch, "B1875") == 0) {
        eraEpb2jd(1875.0, &djm0, &djm);
    } else if (epoch[0] == 'B' || epoch[0] == 'b') {
        eraEpb2jd(atof(epoch + 1), &djm0, &djm);
    } else if (epoch[0] == 'J' || epoch[0] == 'j') {
        eraEpj2jd(atof(epoch + 1), &djm0, &djm);
    } else {
        const char *jd = (epoch[0] == 'D' || epoch[0] == 'd') ? epoch + 2
                                                              : epoch;
        djm = atof(jd);
    }
    eraPnm06a(djm0, djm, rnpb);
    mat_trans(rnpb, out);
}

// Desktop: matCustomEdgeEpochToJ2000 = xrot(eps0)*zrot(-psi)*xrot(-omega)*zrot(chi)
static void desktop_matrix(double jd, double out[3][3])
{
    double eps, chi, omega, psi;
    double R1[3][3], R2[3][3], R3[3][3], R4[3][3];
    double tmp[3][3], R12[3][3];
    getPrecessionAnglesVondrak(jd, &eps, &chi, &omega, &psi);
    mat_zrot(R4, chi);
    mat_xrot(R3, -omega);
    mat_zrot(R2, -psi);
    mat_xrot(R1, eps);
    mat_mul(R3, R4, tmp);
    mat_mul(R2, tmp, R12);
    mat_mul(R1, R12, out);
}

static void print_point(const char *name, const double m[3][3],
                        double ra_deg, double de_deg)
{
    double v[3], r[3];
    double ra, de;
    int i, j;
    vec3_from_sphe_local(ra_deg * M_PI / 180, de_deg * M_PI / 180, v);
    for (i = 0; i < 3; i++) { r[i] = 0; for (j = 0; j < 3; j++) r[i] += m[i][j] * v[j]; }
    ra = atan2(r[1], r[0]) * 180 / M_PI;
    de = asin(r[2]) * 180 / M_PI;
    if (ra < 0) ra += 360;
    printf("%-28s RA=%8.3f deg (%02dh%02dm)  Dec=%+7.3f deg\n",
           name, ra, (int)(ra/15), (int)((ra/15 - (int)(ra/15))*60), de);
}

// Original web engine hardcoded B1875 matrix (before edges_epoch support).
static const double orig_1875[3][3] = {
    {0.999535020565168, 0.027962538774844, 0.012158909862936},
    {-0.027962067406873, 0.999608963139696, -0.000208799220464},
    {-0.012159993837296, -0.000131286124061, 0.999926055923052},
};

// Web engine applies matrices via mat3_mul_vec3: out = M^T . v.
static void apply_as_web(const double m[3][3], const double v[3], double out[3])
{
    out[0] = v[0]*m[0][0] + v[1]*m[1][0] + v[2]*m[2][0];
    out[1] = v[0]*m[0][1] + v[1]*m[1][1] + v[2]*m[2][1];
    out[2] = v[0]*m[0][2] + v[1]*m[1][2] + v[2]*m[2][2];
}

static double angular_diff(const double a[3], const double b[3])
{
    double c = a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
    if (c > 1) c = 1;
    if (c < -1) c = -1;
    return acos(c) * 180.0 / M_PI * 3600.0; // arcsec
}

static void check_direction(const char *name, const double epoch[3][3],
                            const double desktop[3][3], double ra, double de)
{
    double v[3], w[3];
    double buggy[3], fixed[3], ref[3];
    int i, j;
    vec3_from_sphe_local(ra * M_PI / 180, de * M_PI / 180, v);
    // Desktop: out = M . v  (column convention).
    for (i = 0; i < 3; i++) { ref[i] = 0; for (j = 0; j < 3; j++) ref[i] += desktop[i][j] * v[j]; }
    // Current web code: rot = transpose(pnm06a), applied via mat3_mul_vec3 -> pnm06a . v.
    apply_as_web(epoch, v, buggy);
    // Fixed web code: store pnm06a untransposed, applied via mat3_mul_vec3 -> pnm06a^T . v.
    {
        double inv[3][3];
        for (i = 0; i < 3; i++) for (j = 0; j < 3; j++) inv[i][j] = epoch[j][i];
        apply_as_web(inv, v, fixed);
    }
    printf("%-22s web_buggy=%.1f\"  web_fixed=%.1f\"  (vs desktop)\n",
           name, angular_diff(buggy, ref), angular_diff(fixed, ref));
}

int main(void)
{
    double m[3][3], md[3][3], mp[3][3];
    double djm0, djm;
    double jdB1875, jdJ2017;
    int i, j;
    double d; // reuse for diff
    double diffs[3][3]; // declared below

    // Check that the ORIGINAL hardcoded matrix == eraPnm06a(B1875) (untransposed).
    eraEpb2jd(1875.0, &djm0, &djm);
    eraPnm06a(djm0, djm, mp);
    d = 0;
    for (i = 0; i < 3; i++) for (j = 0; j < 3; j++) d += fabs(mp[i][j] - orig_1875[i][j]);
    printf("orig_hardcoded vs eraPnm06a(1875) untransposed diff = %.3e  (0 = confirms untransposed storage)\n", d);

    eraEpb2jd(1875.0, &djm0, &djm);
    jdB1875 = djm0 + djm;
    eraEpj2jd(2017.5278633264887, &djm0, &djm);
    jdJ2017 = djm0 + djm;
    printf("JD B1875 = %.6f,  JD J2017.5 = %.6f\n", jdB1875, jdJ2017);

    // B1875: web engine edges_rot vs desktop j1875ToJ2000
    edges_rot("B1875", m);
    desktop_matrix(jdB1875, md);
    printf("B1875   diff(web edges_rot, desktop j1875ToJ2000) = %.3e  (~%.1f arcsec)\n",
           mat_diff(m, md), mat_diff(m, md) * 180 / M_PI * 3600);

    // J2017.5: web engine edges_rot vs desktop matCustomEdgeEpochToJ2000
    edges_rot("J2017.5278633264887", m);
    desktop_matrix(jdJ2017, md);
    printf("J2017.5 diff(web edges_rot, desktop custom matrix) = %.3e  (~%.1f arcsec)\n",
           mat_diff(m, md), mat_diff(m, md) * 180 / M_PI * 3600);

    // J2000: identity
    edges_rot("J2000", m);
    printf("J2000   diff(edges_rot, identity) = %.3e\n", mat_diff(m, md));

    printf("\nExample chinese_mdn boundary points (epoch -> J2000):\n");
    edges_rot("J2017.5278633264887", m);
    print_point("J2017.5 (14h, +74d)", m, 14*15, 74);
    print_point("J2017.5 (15h, +85d)", m, 15*15, 85);
    print_point("J2017.5 (00h, +88d)", m, 0, 88);

    printf("\nSirius check (desktop B1875 reference):\n");
    edges_rot("B1875", m);
    print_point("Sirius J2000 -> B1875 rot", m, 101.2871, -16.7161);

    printf("\nApplied-direction check (must be web_fixed ~ 0\"):\n");
    // B1875 data point (e.g. a western boundary vertex).
    edges_rot("B1875", m);
    desktop_matrix(jdB1875, md);
    check_direction("B1875 (6h30m, +60d)", m, md, 97.5, 60);
    check_direction("B1875 (20h, -30d)", m, md, 300, -30);
    // J2017.5 chinese_mdn data points.
    edges_rot("J2017.5278633264887", m);
    desktop_matrix(jdJ2017, md);
    check_direction("J2017.5 (14h, +74d)", m, md, 210, 74);
    check_direction("J2017.5 (15h, +85d)", m, md, 225, 85);
    check_direction("J2017.5 (00h, +88d)", m, md, 0, 88);
    check_direction("J2017.5 (20h, -30d)", m, md, 300, -30);

    return 0;
}

static void vec3_from_sphe_local(double ra, double de, double v[3])
{
    double cd = cos(de);
    v[0] = cd * cos(ra);
    v[1] = cd * sin(ra);
    v[2] = sin(de);
}
