/*
 * Kinematics.cpp
 */

#include "Kinematics.h"
#include <math.h>

namespace Kin {

FixedJoint::FixedJoint(const Vec3 &translation, const Vec3 &axis, float angle)
		: xTranslation(translation), xAxis(axis.normalized()), xAngle(angle) {
}

Tf FixedJoint::transform(float) const {
	Tf t = Tf::Identity();
	t.translate(xTranslation);
	t.rotate(Eigen::AngleAxisf(xAngle, xAxis));
	return t;
}

RevoluteJoint::RevoluteJoint(const Vec3 &axis) : xAxis(axis.normalized()) {
}

Tf RevoluteJoint::transform(float q) const {
	Tf t = Tf::Identity();
	t.rotate(Eigen::AngleAxisf(q, xAxis));
	return t;
}

Rod::Rod(float length1, float length2, const Vec3 &direction,
		const Vec3 &bendAxis, float bendAngle)
		: xLen1(length1), xLen2(length2), xDirection(direction.normalized()),
		  xBendAxis(bendAxis.normalized()), xBendAngle(bendAngle) {
}

Tf Rod::transform(float) const {
	Tf t = Tf::Identity();
	t.translate(xDirection * xLen1);
	t.rotate(Eigen::AngleAxisf(xBendAngle, xBendAxis));
	t.translate(xDirection * xLen2);
	return t;
}

Chain::Chain() {
}

void Chain::add(std::unique_ptr<Link> link) {
	xLinks.push_back(std::move(link));
}

void Chain::add(std::unique_ptr<RevoluteJoint> joint) {
	pJoint = joint.get();
	xJointIndex = xLinks.size();
	xLinks.push_back(std::move(joint));
}

Tf Chain::fk(float q) const {
	Tf t = Tf::Identity();
	for (const auto &link : xLinks) {
		t = t * link->transform(q);
	}
	return t;
}

Vec3 Chain::position(float q) const {
	return fk(q).translation();
}

bool Chain::ik(const Vec3 &target, float &q, float tol) const {
	if (pJoint == nullptr) {
		return false;
	}

	Tf pre = Tf::Identity();
	Tf post = Tf::Identity();
	for (size_t i = 0; i < xLinks.size(); i++) {
		if (i < xJointIndex) {
			pre = pre * xLinks[i]->transform(0.0f);
		} else if (i > xJointIndex) {
			post = post * xLinks[i]->transform(0.0f);
		}
	}

	// Work in the joint frame: rotating r about the axis must land on p.
	const Vec3 &a = pJoint->axis();
	Vec3 p = pre.inverse() * target;
	Vec3 r = post.translation();

	Vec3 pPerp = p - a * a.dot(p);
	Vec3 rPerp = r - a * a.dot(r);
	if (pPerp.norm() < 1e-6f || rPerp.norm() < 1e-6f) {
		return false;
	}

	q = atan2f(a.dot(rPerp.cross(pPerp)), rPerp.dot(pPerp));

	Vec3 reached = position(q);
	return (reached - target).norm() <= tol;
}

} // namespace Kin
