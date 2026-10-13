#include "BulletWorld.h"
#include <btBulletDynamicsCommon.h>
#include <cmath>
#include <vector>

namespace Gamebox {
namespace Alley {
namespace {
constexpr float kPi = 3.14159265358979f;
constexpr float kPitDepth = 0.8f;
constexpr float kPitLength = 2.4f;
constexpr float kBallMass = 7.26f;
constexpr float kPinMass = 1.5f;
constexpr float kSlowLinear = 0.05f;   // m/s
constexpr float kSlowAngular = 0.2f;   // rad/s
constexpr float kStandingCos = 0.7071f;  // cos 45°

Pose toPose(const btTransform& t) {
    Pose p;
    const btVector3& o = t.getOrigin();
    p.position = {o.x(), o.y(), o.z()};
    const btQuaternion q = t.getRotation();
    p.rotation = {q.x(), q.y(), q.z(), q.w()};
    return p;
}

btTransform at(float x, float y, float z) {
    btTransform t;
    t.setIdentity();
    t.setOrigin(btVector3(x, y, z));
    return t;
}
}  // namespace

struct BulletWorld::Impl {
    btDefaultCollisionConfiguration config;
    btCollisionDispatcher dispatcher {&config};
    btDbvtBroadphase broadphase;
    btSequentialImpulseConstraintSolver solver;
    btDiscreteDynamicsWorld world {&dispatcher, &broadphase, &solver, &config};

    std::vector<std::unique_ptr<btCollisionShape>> shapes;
    std::vector<std::unique_ptr<btDefaultMotionState>> motions;
    std::vector<std::unique_ptr<btRigidBody>> statics;
    std::unique_ptr<btRigidBody> ball;
    std::array<std::unique_ptr<btRigidBody>, 10> pins;
    std::array<bool, 10> inPlay {};

    Impl() { build(); }
    ~Impl() {
        // Bodies must leave the world before they are destroyed.
        for (int i = 0; i < 10; ++i)
            if (inPlay[i])
                world.removeRigidBody(pins[i].get());
        world.removeRigidBody(ball.get());
        for (auto& b : statics)
            world.removeRigidBody(b.get());
    }

    btCollisionShape* keep(std::unique_ptr<btCollisionShape> s) {
        shapes.push_back(std::move(s));
        return shapes.back().get();
    }

    void addStatic(btCollisionShape* shape, const btTransform& t, float friction, float rolling,
                   float restitution) {
        motions.push_back(std::make_unique<btDefaultMotionState>(t));
        btRigidBody::btRigidBodyConstructionInfo info(0.f, motions.back().get(), shape);
        info.m_friction = friction;
        info.m_rollingFriction = rolling;
        info.m_restitution = restitution;
        statics.push_back(std::make_unique<btRigidBody>(info));
        world.addRigidBody(statics.back().get());
    }

    std::unique_ptr<btRigidBody> makeDynamic(btCollisionShape* shape, float mass,
                                             const btTransform& t, float friction,
                                             float rolling, float restitution) {
        btVector3 inertia(0, 0, 0);
        shape->calculateLocalInertia(mass, inertia);
        motions.push_back(std::make_unique<btDefaultMotionState>(t));
        btRigidBody::btRigidBodyConstructionInfo info(mass, motions.back().get(), shape, inertia);
        info.m_friction = friction;
        info.m_rollingFriction = rolling;
        info.m_restitution = restitution;
        auto body = std::make_unique<btRigidBody>(info);
        body->setSleepingThresholds(0.05f, 0.1f);
        return body;
    }

    void build() {
        world.setGravity(btVector3(0, -9.81f, 0));
        const LaneDimensions& d = kLane;
        const float outerX = d.halfWidth + d.gutterWidth;

        // Bullet multiplies the friction/restitution of both bodies, so the
        // static surfaces use 1.0 and the dynamic bodies carry the real value.
        // Lane: approach + lane, top surface at y = 0.
        const float laneLen = d.length + d.approach;
        addStatic(keep(std::make_unique<btBoxShape>(btVector3(d.halfWidth, 0.05f, laneLen / 2))),
                  at(0.f, -0.05f, (d.length - d.approach) / 2), 1.f, 1.f, 1.f);
        // Gutters, gutterDepth below the lane.
        auto* gutter = keep(std::make_unique<btBoxShape>(
            btVector3(d.gutterWidth / 2, 0.05f, d.length / 2)));
        for (float sx : {-1.f, 1.f})
            addStatic(gutter, at(sx * (d.halfWidth + d.gutterWidth / 2), -d.gutterDepth - 0.05f,
                                 d.length / 2), 1.f, 1.f, 1.f);
        // Gutter outer walls so the ball cannot leave the alley sideways. Inner
        // face at outerX + 0.06 = 0.825 > kMaxAimOffset + ballRadius (0.809).
        auto* wall = keep(std::make_unique<btBoxShape>(btVector3(0.02f, 0.15f, d.length / 2)));
        for (float sx : {-1.f, 1.f})
            addStatic(wall, at(sx * (outerX + 0.08f), 0.05f, d.length / 2), 1.f, 1.f, 1.f);
        // Pit behind the deck: floor, back wall, side walls.
        const float pitZ = d.length + kPitLength / 2;
        addStatic(keep(std::make_unique<btBoxShape>(btVector3(outerX + 0.12f, 0.05f, kPitLength / 2))),
                  at(0.f, -kPitDepth - 0.05f, pitZ), 1.f, 1.f, 0.2f);
        addStatic(keep(std::make_unique<btBoxShape>(btVector3(outerX + 0.12f, 0.6f, 0.05f))),
                  at(0.f, -0.3f, d.length + kPitLength + 0.05f), 1.f, 1.f, 0.2f);
        auto* pitSide = keep(std::make_unique<btBoxShape>(btVector3(0.05f, 0.6f, kPitLength / 2)));
        for (float sx : {-1.f, 1.f})
            addStatic(pitSide, at(sx * (outerX + 0.17f), -0.3f, pitZ), 1.f, 1.f, 0.2f);

        // Ball.
        auto* ballShape = keep(std::make_unique<btSphereShape>(d.ballRadius));
        ball = makeDynamic(ballShape, kBallMass, at(0.f, d.ballRadius, kBallStartZ), 0.1f, 0.005f, 0.1f);
        ball->setCcdMotionThreshold(0.05f);
        ball->setCcdSweptSphereRadius(d.ballRadius * 0.8f);
        world.addRigidBody(ball.get());

        // Pins: two stacked cylinders, origin pinComHeight above the base.
        auto pinShape = std::make_unique<btCompoundShape>();
        auto* lower = keep(std::make_unique<btCylinderShape>(btVector3(d.pinMaxRadius, 0.08f, d.pinMaxRadius)));
        auto* upper = keep(std::make_unique<btCylinderShape>(btVector3(0.035f, 0.09f, 0.035f)));
        pinShape->addChildShape(at(0.f, 0.10f - d.pinComHeight, 0.f), lower);
        pinShape->addChildShape(at(0.f, 0.29f - d.pinComHeight, 0.f), upper);
        auto* pinShapePtr = keep(std::move(pinShape));
        const auto positions = BulletWorld::pinPositions();
        for (int i = 0; i < 10; ++i) {
            pins[i] = makeDynamic(pinShapePtr, kPinMass,
                                  at(positions[i].first, d.pinComHeight, positions[i].second),
                                  0.3f, 0.05f, 0.5f);
            pins[i]->setCcdMotionThreshold(0.02f);
            pins[i]->setCcdSweptSphereRadius(0.03f);
            world.addRigidBody(pins[i].get());
            inPlay[i] = true;
        }
    }

    static void teleport(btRigidBody& body, const btTransform& t) {
        body.setWorldTransform(t);
        body.getMotionState()->setWorldTransform(t);
        body.setLinearVelocity(btVector3(0, 0, 0));
        body.setAngularVelocity(btVector3(0, 0, 0));
        body.clearForces();
        body.activate(true);
    }

    static bool slow(const btRigidBody& b) {
        return !b.isActive() || (b.getLinearVelocity().length() < kSlowLinear &&
                                 b.getAngularVelocity().length() < kSlowAngular);
    }
};

BulletWorld::BulletWorld() : _p(std::make_unique<Impl>()) {}
BulletWorld::~BulletWorld() = default;

std::unique_ptr<IPhysicsWorld> makeBulletWorld() {
    return std::make_unique<BulletWorld>();
}

void BulletWorld::step(float dt) {
    _p->world.stepSimulation(dt, 4, 1.f / 120.f);
}

void BulletWorld::placeBall(float offset) {
    Impl::teleport(*_p->ball, at(offset, kLane.ballRadius, kBallStartZ));
}

void BulletWorld::launchBall(float offset, float angleDeg, float speed) {
    placeBall(offset);
    const float a = angleDeg * kPi / 180.f;
    const btVector3 dir(std::sin(a), 0.f, std::cos(a));
    const btVector3 up(0.f, 1.f, 0.f);
    _p->ball->setLinearVelocity(dir * speed);
    // Rolling without slipping: omega = (v / r) * (dir x up).
    _p->ball->setAngularVelocity(dir.cross(up) * (speed / kLane.ballRadius));
    _p->ball->activate(true);
}

void BulletWorld::rebuild() {
    _p = std::make_unique<Impl>();
}

void BulletWorld::resetRack() {
    const auto positions = pinPositions();
    for (int i = 0; i < 10; ++i) {
        if (!_p->inPlay[i]) {
            _p->world.addRigidBody(_p->pins[i].get());
            _p->inPlay[i] = true;
        }
        Impl::teleport(*_p->pins[i], at(positions[i].first, kLane.pinComHeight, positions[i].second));
    }
}

void BulletWorld::removePin(int i) {
    if (i < 0 || i >= 10 || !_p->inPlay[i])
        return;
    _p->world.removeRigidBody(_p->pins[i].get());
    _p->inPlay[i] = false;
}

Pose BulletWorld::ballPose() const {
    return toPose(_p->ball->getWorldTransform());
}

Pose BulletWorld::pinPose(int i) const {
    return toPose(_p->pins[i]->getWorldTransform());
}

bool BulletWorld::isPinStanding(int i) const {
    if (!_p->inPlay[i])
        return false;
    const btTransform& t = _p->pins[i]->getWorldTransform();
    const btVector3 up = t.getBasis().getColumn(1);
    const btVector3& o = t.getOrigin();
    return up.y() > kStandingCos && std::abs(o.x()) < kLane.halfWidth + 0.1f &&
           o.z() < kLane.length + 0.05f && o.y() > 0.f;
}

bool BulletWorld::isPinInPlay(int i) const {
    return _p->inPlay[i];
}

bool BulletWorld::ballInPit() const {
    return _p->ball->getWorldTransform().getOrigin().y() < -0.3f;
}

float BulletWorld::ballSpeed() const {
    return _p->ball->getLinearVelocity().length();
}

bool BulletWorld::allSlow() const {
    if (!Impl::slow(*_p->ball))
        return false;
    for (int i = 0; i < 10; ++i)
        if (_p->inPlay[i] && !Impl::slow(*_p->pins[i]))
            return false;
    return true;
}

std::array<std::pair<float, float>, 10> BulletWorld::pinPositions() {
    const float s = kLane.pinSpacing;
    const float row = s * 0.8660254f;  // s * cos 30°
    const float z0 = kLane.headPinZ;
    return {{{0.f, z0},
             {-s / 2, z0 + row}, {s / 2, z0 + row},
             {-s, z0 + 2 * row}, {0.f, z0 + 2 * row}, {s, z0 + 2 * row},
             {-1.5f * s, z0 + 3 * row}, {-s / 2, z0 + 3 * row}, {s / 2, z0 + 3 * row}, {1.5f * s, z0 + 3 * row}}};
}

}  // namespace Alley
}  // namespace Gamebox
