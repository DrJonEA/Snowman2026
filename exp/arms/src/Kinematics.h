/*
 * Kinematics.h
 *
 * Small serial-chain framework (fixed offsets, revolute joint, bent rod)
 * using Eigen. Units are metres and radians.
 */

#ifndef EXP_ARMS_SRC_KINEMATICS_H_
#define EXP_ARMS_SRC_KINEMATICS_H_

#include <Eigen/Geometry>
#include <memory>
#include <vector>

namespace Kin {

using Vec3 = Eigen::Vector3f;
using Tf = Eigen::Isometry3f;

class Link {
public:
	virtual ~Link() {}
	// Transform from the parent frame to this link's end frame at joint value q.
	virtual Tf transform(float q = 0.0f) const = 0;
};

// Rigid offset: translation then optional rotation.
class FixedJoint : public Link {
public:
	FixedJoint(const Vec3 &translation,
			const Vec3 &axis = Vec3::UnitZ(), float angle = 0.0f);
	Tf transform(float q = 0.0f) const override;

private:
	Vec3 xTranslation;
	Vec3 xAxis;
	float xAngle;
};

class RevoluteJoint : public Link {
public:
	RevoluteJoint(const Vec3 &axis);
	Tf transform(float q) const override;
	const Vec3 &axis() const { return xAxis; }

private:
	Vec3 xAxis;
};

// Straight rod of length1 along direction, a bend of bendAngle about bendAxis, then a
// straight rod of length2 along the same (now rotated) direction. The end frame origin is the tip.
class Rod : public Link {
public:
	Rod(float length1, float length2, const Vec3 &direction,
			const Vec3 &bendAxis, float bendAngle);
	Tf transform(float q = 0.0f) const override;

private:
	float xLen1, xLen2;
	Vec3 xDirection;
	Vec3 xBendAxis;
	float xBendAngle;
};

// Serial chain of links with any number of revolute joints (up to MAX_JOINTS).
// Joint values are taken in the order the joints were added.
class Chain {
public:
	static const size_t MAX_JOINTS = 6;

	Chain();

	void add(std::unique_ptr<Link> link);
	void add(std::unique_ptr<RevoluteJoint> joint);

	size_t jointCount() const;

	// Tip frame; q holds one value per joint.
	Tf fk(const float *q) const;
	Vec3 position(const float *q) const;
	Tf fk(float q) const;
	Vec3 position(float q) const;

	// Single-joint chains only: closed-form joint angle bringing the tip closest to target.
	// Returns true if the tip lands within tol metres of target.
	// Returns false without touching q if the chain has not exactly one joint or the target is on the joint axis.
	bool ik(const Vec3 &target, float &q, float tol = 0.005f) const;

	// Any chain: damped least squares position IK, starting from the values in q.
	// Updates q with the closest solution found and returns true if within tol metres of target.
	bool ik(const Vec3 &target, float *q, float tol = 0.005f, int maxIter = 50) const;

private:
	std::vector<std::unique_ptr<Link>> xLinks;
	std::vector<int> xJointOfLink;      // joint number per link, or -1
	std::vector<const RevoluteJoint *> xJoints;
	std::vector<size_t> xJointLink;     // link index per joint
};

} // namespace Kin

#endif /* EXP_ARMS_SRC_KINEMATICS_H_ */
