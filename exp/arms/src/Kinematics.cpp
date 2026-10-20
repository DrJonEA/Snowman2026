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
	xJointOfLink.push_back(-1);
	xLinks.push_back(std::move(link));
}

void Chain::add(std::unique_ptr<RevoluteJoint> joint) {
	xJointOfLink.push_back((int)xJoints.size());
	xJoints.push_back(joint.get());
	xJointLink.push_back(xLinks.size());
	xLinks.push_back(std::move(joint));
}

size_t Chain::jointCount() const {
	return xJoints.size();
}

Tf Chain::fk(const float *q) const {
	Tf t = Tf::Identity();
	for (size_t i = 0; i < xLinks.size(); i++) {
		int j = xJointOfLink[i];
		t = t * xLinks[i]->transform(j >= 0 ? q[j] : 0.0f);
	}
	return t;
}

Tf Chain::fk(float q) const {
	return fk(&q);
}

Vec3 Chain::position(const float *q) const {
	return fk(q).translation();
}

Vec3 Chain::position(float q) const {
	return fk(q).translation();
}

bool Chain::ik(const Vec3 &target, float &q, float tol) const {
	if (xJoints.size() != 1) {
		return false;
	}
	const RevoluteJoint *pJoint = xJoints[0];
	size_t jointLink = xJointLink[0];

	Tf pre = Tf::Identity();
	Tf post = Tf::Identity();
	for (size_t i = 0; i < xLinks.size(); i++) {
		if (i < jointLink) {
			pre = pre * xLinks[i]->transform(0.0f);
		} else if (i > jointLink) {
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

bool Chain::ik(const Vec3 &target, float *q, float tol, int maxIter) const {
	const size_t n = xJoints.size();
	if (n == 0 || n > MAX_JOINTS) {
		return false;
	}

	const float lambda2 = 4e-6f;  // damping keeps the solve stable near singularities
	const float eps = 1e-3f;      // finite-difference step for the Jacobian
	const float maxStep = 0.3f;   // rad per iteration

	float cur[MAX_JOINTS];
	for (size_t k = 0; k < n; k++) {
		cur[k] = q[k];
	}

	for (int it = 0; it < maxIter; it++) {
		Vec3 p = position(cur);
		Vec3 e = target - p;
		if (e.norm() < 1e-5f) {
			break;
		}

		Eigen::Matrix<float, 3, Eigen::Dynamic, 0, 3, MAX_JOINTS> J(3, n);
		for (size_t k = 0; k < n; k++) {
			float saved = cur[k];
			cur[k] = saved + eps;
			J.col(k) = (position(cur) - p) / eps;
			cur[k] = saved;
		}

		// Damped least squares: dq = J^T (J J^T + lambda^2 I)^-1 e
		Eigen::Matrix3f A = J * J.transpose() + lambda2 * Eigen::Matrix3f::Identity();
		Vec3 y = A.ldlt().solve(e);
		Eigen::Matrix<float, Eigen::Dynamic, 1, 0, MAX_JOINTS, 1> dq = J.transpose() * y;

		float biggest = dq.cwiseAbs().maxCoeff();
		if (biggest < 1e-6f) {
			break;
		}
		if (biggest > maxStep) {
			dq *= maxStep / biggest;
		}
		for (size_t k = 0; k < n; k++) {
			cur[k] += dq[k];
		}
	}

	for (size_t k = 0; k < n; k++) {
		q[k] = cur[k];
	}
	return (position(cur) - target).norm() <= tol;
}

} // namespace Kin
