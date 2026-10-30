// sawheels_math.cpp - checks the wheel/suspension arithmetic reVC uses for
// vehicles: a VC model (separate unit wheel mesh scaled by m_wheelScale) versus
// an SA model (its own wheel mesh of radius r), before and after the SA fix.
//
// Standalone replica of the formulas in
//   CAutomobile::SetupSuspensionLines / CAutomobile::PreRender   (reVC)
//   CVehicleModelInfo::AddGenericWheelModel                      (VC wheels)
//   CVehicleModelInfo::CloneSAWheelMeshes                        (SA wheels)
//
// Build:  g++ -std=c++11 sawheels_math.cpp -o sawheels_math
// Run:    ./sawheels_math
//
// admiral.dff measurements (see out_geodump.txt):
//   wheel dummy position z = 0.1770    (frame wheel_rf_dummy, all four equal)
//   wheel mesh radius      r = 0.3901  (vertex bbox of frame "wheel")
//   model bbox min z     = -0.2132     (= dummy z - r = the true tyre bottom)
//
// Physics recap - the tyre radius enters every formula through m_wheelScale:
//   suspension line  p0.z = dummyZ + upperLimit
//                    p1.z = dummyZ + lowerLimit - 0.5*m_wheelScale
//   ride height      m_fHeightAboveRoad = springLen*(1-1/(4F)) - p0.z + 0.5*m_wheelScale
//   wheel position   m_aWheelPosition   = 0.5*m_wheelScale - m_fHeightAboveRoad
// The body rests where the suspension line meets the ground, so the tyres of the
// model stand exactly on the road when m_wheelScale is the true tyre diameter
// (2r) and the wheel mesh is rendered unscaled.
#include <stdio.h>

struct Params {
	float upper;	// fSuspensionUpperLimit
	float lower;	// fSuspensionLowerLimit
	float force;	// fSuspensionForceLevel
};

struct Pose {
	float springLen, lineLen;
	float p0z, p1z;
	float heightAboveRoad;	// m_fHeightAboveRoad
	float wheelPosZ;		// m_aWheelPosition
	float physicsRadius;	// 0.5*m_wheelScale
	float renderRadius;	// radius the wheel mesh is drawn with
};

// sa: use the static spring compression as the top of the wheel travel, i.e. let
// the model (not the handling data) define where the tyre touches the ground
// (this is what the fix does, see CAutomobile::SetupSuspensionLines).
static Pose
Setup(float wheelScale, float dummyZ, float unitMeshRadius, Params h, bool sa)
{
	Pose p;
	float suspUpper = h.upper;
	float sag = (h.upper - h.lower)/(4.0f*h.force);

	if(sa){
		// the model defines where the tyre is: the line ends at the tyre bottom of
		// the model (dummy z - radius) minus the static sag, and starts one spring
		// length plus the sag above that
		suspUpper = (h.upper - h.lower) - sag;
		p.springLen = h.upper - h.lower;
		p.p0z = dummyZ + suspUpper;
		p.p1z = dummyZ - 0.5f*wheelScale - sag;
		p.lineLen = p.p0z - p.p1z;
		p.heightAboveRoad = p.springLen*(1.0f - 1.0f/(4.0f*h.force)) - p.p0z + 0.5f*wheelScale;
		p.wheelPosZ = 0.5f*wheelScale - p.heightAboveRoad;
		p.physicsRadius = 0.5f*wheelScale;
		p.renderRadius = unitMeshRadius;
		return p;
	}

	p.springLen = h.upper - h.lower;
	p.p0z = dummyZ + suspUpper;
	p.p1z = p.p0z + (h.lower - suspUpper) - 0.5f*wheelScale;
	p.lineLen = p.p0z - p.p1z;
	p.heightAboveRoad = p.springLen*(1.0f - 1.0f/(4.0f*h.force)) - p.p0z + 0.5f*wheelScale;
	p.wheelPosZ = 0.5f*wheelScale - p.heightAboveRoad;
	p.physicsRadius = 0.5f*wheelScale;
	// VC: the separate unit wheel mesh (radius 0.5) is scaled by m_wheelScale.
	// SA: the mesh that came with the model already has its final size.
	p.renderRadius = unitMeshRadius*wheelScale;
	return p;
}

// static sag of the spring = how deep the physics tyre is pressed into the ground
// at rest. VC handling data is authored so that this equals springLen/(4F) - that
// is exactly what the m_fHeightAboveRoad formula above encodes.
static float
DesignSag(Params h)
{
	return (h.upper - h.lower)/(4.0f*h.force);
}

// rest pose of the body and where the rendered tyres end up relative to the road
static void
Row(const char *model, float wheelScale, float dummyZ, float unitMeshRadius, Params h,
	bool sa, float bodyLowestZ)
{
	Pose p = Setup(wheelScale, dummyZ, unitMeshRadius, h, sa);
	float z0 = -p.p1z - DesignSag(h);
	float tyre = z0 + p.wheelPosZ - p.renderRadius;
	float body = z0 + bodyLowestZ;

	printf("  %-28s %8.4f %10.4f %10.4f %10.4f %+10.4f\n",
		model, wheelScale, p.renderRadius, p.physicsRadius, z0, tyre);
	(void)body;
}

#define HDR()	printf("  %-28s %8s %10s %10s %10s %10s\n", \
		"model / wheels", "scale", "rendered_r", "phys_r", "rest_z0", "tyre_gap")

int
main(void)
{
	const float dummyZ = 0.1770f;		// admiral wheel dummies
	const float radius = 0.3901f;		// true wheel radius inside the DFF
	const float bodyLowest = -0.2132f;	// admiral model bbox min z

	// VC reference model: unit wheel mesh (radius 0.5), dummy z = 0.5*wheelScale
	const float vcScale = 0.70f;
	const float vcDummyZ = 0.5f*vcScale;

	Params c[3];
	c[0].upper = 0.15f; c[0].lower = -0.15f; c[0].force = 0.5f;
	c[1].upper = 0.10f; c[1].lower = -0.20f; c[1].force = 0.5f;
	c[2].upper = 0.30f; c[2].lower = -0.10f; c[2].force = 0.5f;

	printf("\nBEFORE the fix - SA model with the wheelScale of the replaced VC car\n");
	printf("(wheels scaled and the physics using 0.5*wheelScale instead of the real radius)\n\n");
	HDR();
	for(float s = 0.50f; s < 1.01f; s += 0.10f)
		Row("admiral.dff (SA, pre-fix)", s, dummyZ, radius, c[0], false, bodyLowest);
	printf("\n  %-28s %8.4f %10.4f %10.4f %10.4f %+10.4f   <-- correct reference\n",
		"VC model + VC handling", vcScale, 0.5f*vcScale, 0.5f*vcScale,
		0.0f, 0.0f);

	printf("\nAFTER the fix - m_wheelScale = 2*r = %.4f, wheel meshes rendered unscaled\n\n",
		2.0f*radius);
	printf("  %-28s %8s %10s %10s %10s %10s\n", "handling data (upper/lower/F)",
		"scale", "rendered_r", "phys_r", "rest_z0", "tyre_gap");
	for(int i = 0; i < 3; i++){
		char buf[64];
		printf("  upper %+.2f lower %+.2f force %.2f:\n", c[i].upper, c[i].lower, c[i].force);
		snprintf(buf, sizeof(buf), "  VC model (same data)");
		Row(buf, vcScale, vcDummyZ, 0.50f, c[i], false, 0.0f);
		snprintf(buf, sizeof(buf), "  admiral.dff (SA, fixed)");
		Row(buf, 2.0f*radius, dummyZ, radius, c[i], true, bodyLowest);
	}

	printf("\n  tyre_gap = rendered tyre bottom above the road at rest\n"
	       "             (0.0000 = tyres stand on the road)\n"
	       "  rest_z0  = height of the model origin above the road at rest\n\n"
	       "  VC rows: the wheels are always glued to the road (the render position is\n"
	       "           derived from m_fHeightAboveRoad), the *body* moves with the data.\n"
	       "  SA rows: rest height is tyreRadius - dummyZ = %.4f, i.e. the model stands\n"
	       "           on its own wheels, and the rendered radius equals the physics\n"
	       "           radius for any handling data.\n\n", 0.3901f - 0.1770f);
	return 0;
}
