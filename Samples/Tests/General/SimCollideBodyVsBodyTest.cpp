// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2025 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <TestFramework.h>

#include <Tests/General/SimCollideBodyVsBodyTest.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/CollisionDispatch.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Core/STLLocalAllocator.h>
#include <Layers.h>
#include <Renderer/DebugRendererImp.h>

JPH_IMPLEMENT_RTTI_VIRTUAL(SimCollideBodyVsBodyTest)
{
	JPH_ADD_BASE_CLASS(SimCollideBodyVsBodyTest, Test)
}

template <class LeafCollector>
static void sCollideBodyVsBodyPerBody(const Body &inBody1, const Body &inBody2, Mat44Arg inCenterOfMassTransform1, Mat44Arg inCenterOfMassTransform2, CollideShapeSettings &ioCollideShapeSettings, CollideShapeCollector &ioCollector, const ShapeFilter &inShapeFilter)
{
	if (inBody1.IsSensor() || inBody2.IsSensor())
	{
		LeafCollector collector;
		SubShapeIDCreator part1, part2;
		CollisionDispatch::sCollideShapeVsShape(inBody1.GetShape(), inBody2.GetShape(), Vec3::sOne(), Vec3::sOne(), inCenterOfMassTransform1, inCenterOfMassTransform2, part1, part2, ioCollideShapeSettings, collector);
		if (collector.HadHit())
			ioCollector.AddHit(collector.mHit);
	}
	else
	{
		// If not a sensor: fall back to the default
		PhysicsSystem::sDefaultSimCollideBodyVsBody(inBody1, inBody2, inCenterOfMassTransform1, inCenterOfMassTransform2, ioCollideShapeSettings, ioCollector, inShapeFilter);
	}
}

template <class LeafCollector>
static void sCollideBodyVsBodyPerLeaf(const Body &inBody1, const Body &inBody2, Mat44Arg inCenterOfMassTransform1, Mat44Arg inCenterOfMassTransform2, CollideShapeSettings &ioCollideShapeSettings, CollideShapeCollector &ioCollector, const ShapeFilter &inShapeFilter)
{
	if (inBody1.IsSensor() || inBody2.IsSensor())
	{
		// Tracks information we need about a leaf shape
		struct LeafShape
		{
								LeafShape() = default;

								LeafShape(const AABox &inBounds, Mat44Arg inCenterOfMassTransform, Vec3Arg inScale, const Shape *inShape, const SubShapeIDCreator &inSubShapeIDCreator) :
				mBounds(inBounds),
				mCenterOfMassTransform(inCenterOfMassTransform),
				mScale(inScale),
				mShape(inShape),
				mSubShapeIDCreator(inSubShapeIDCreator)
			{
			}

			AABox				mBounds;
			Mat44				mCenterOfMassTransform;
			Vec3				mScale;
			const Shape *		mShape;
			SubShapeIDCreator	mSubShapeIDCreator;
		};

		// A collector that stores the information we need from a leaf shape in an array that is usually on the stack but can fall back to the heap if needed
		class MyCollector : public TransformedShapeCollector
		{
		public:
			void				AddHit(const TransformedShape &inShape) override
			{
				mHits.emplace_back(inShape.GetWorldSpaceBounds(), inShape.GetCenterOfMassTransform().ToMat44(), inShape.GetShapeScale(), inShape.mShape, inShape.mSubShapeIDCreator);
			}

			Array<LeafShape, STLLocalAllocator<LeafShape, 32>> mHits;
		};

		// Get bounds of both shapes
		AABox bounds1 = inBody1.GetShape()->GetWorldSpaceBounds(inCenterOfMassTransform1, Vec3::sOne());
		AABox bounds2 = inBody2.GetShape()->GetWorldSpaceBounds(inCenterOfMassTransform2, Vec3::sOne());

		// Get leaf shapes that overlap with the bounds of the other shape
		SubShapeIDCreator part1, part2;
		MyCollector leaf_shapes1, leaf_shapes2;
		inBody1.GetShape()->CollectTransformedShapes(bounds2, inCenterOfMassTransform1.GetTranslation(), inCenterOfMassTransform1.GetQuaternion(), Vec3::sOne(), part1, leaf_shapes1, inShapeFilter);
		inBody2.GetShape()->CollectTransformedShapes(bounds1, inCenterOfMassTransform2.GetTranslation(), inCenterOfMassTransform2.GetQuaternion(), Vec3::sOne(), part2, leaf_shapes2, inShapeFilter);

		// Now test each leaf shape against each other leaf
		for (const LeafShape &leaf1 : leaf_shapes1.mHits)
			for (const LeafShape &leaf2 : leaf_shapes2.mHits)
				if (leaf1.mBounds.Overlaps(leaf2.mBounds))
				{
					LeafCollector collector;
					CollisionDispatch::sCollideShapeVsShape(leaf1.mShape, leaf2.mShape, leaf1.mScale, leaf2.mScale, leaf1.mCenterOfMassTransform, leaf2.mCenterOfMassTransform, leaf1.mSubShapeIDCreator, leaf2.mSubShapeIDCreator, ioCollideShapeSettings, collector, inShapeFilter);
					if (collector.HadHit())
						ioCollector.AddHit(collector.mHit);
				}
	}
	else
	{
		// If not a sensor: fall back to the default
		PhysicsSystem::sDefaultSimCollideBodyVsBody(inBody1, inBody2, inCenterOfMassTransform1, inCenterOfMassTransform2, ioCollideShapeSettings, ioCollector, inShapeFilter);
	}
}

void SimCollideBodyVsBodyTest::Initialize()
{
	// Create pyramid with flat top
	MeshShapeSettings pyramid;
	pyramid.mTriangleVertices = { Float3(1, 0, 1), Float3(1, 0, -1), Float3(-1, 0, -1), Float3(-1, 0, 1), Float3(0.1f, 1, 0.1f), Float3(0.1f, 1, -0.1f), Float3(-0.1f, 1, -0.1f), Float3(-0.1f, 1, 0.1f) };
	pyramid.mIndexedTriangles = { IndexedTriangle(0, 1, 4), IndexedTriangle(4, 1, 5), IndexedTriangle(1, 2, 5), IndexedTriangle(2, 6, 5), IndexedTriangle(2, 3, 6), IndexedTriangle(3, 7, 6), IndexedTriangle(3, 0, 7), IndexedTriangle(0, 4, 7), IndexedTriangle(4, 5, 6), IndexedTriangle(4, 6, 7) };
	pyramid.SetEmbedded();

	// Create floor of many pyramids
	StaticCompoundShapeSettings compound;
	for (int x = -10; x <= 10; ++x)
		for (int z = -10; z <= 10; ++z)
			compound.AddShape(Vec3(x * 2.0f, 0, z * 2.0f), Quat::sIdentity(), &pyramid);
	compound.SetEmbedded();

	mBodyInterface->CreateAndAddBody(BodyCreationSettings(&compound, RVec3::sZero(), Quat::sIdentity(), EMotionType::Static, Layers::NON_MOVING), EActivation::DontActivate);

	// A kinematic sensor that also detects static bodies
	BodyCreationSettings sensor_settings(new BoxShape(Vec3::sReplicate(10.0f)), RVec3(0, 5, 0), Quat::sIdentity(), EMotionType::Kinematic, Layers::MOVING); // Put in a layer that collides with static
	sensor_settings.mIsSensor = true;
	sensor_settings.mCollideKinematicVsNonDynamic = true;
	sensor_settings.mUseManifoldReduction = false;
	mSensorID = mBodyInterface->CreateAndAddBody(sensor_settings, EActivation::Activate);

	// Dynamic bodies
	for (int i = 0; i < 10; ++i)
		mBodyIDs.push_back(mBodyInterface->CreateAndAddBody(BodyCreationSettings(new BoxShape(Vec3(0.1f, 0.5f, 0.2f)), RVec3::sZero(), Quat::sIdentity(), EMotionType::Dynamic, Layers::MOVING), EActivation::Activate));
}

void SimCollideBodyVsBodyTest::PrePhysicsUpdate(const PreUpdateParams &inParams)
{
	// Update time
	mTime += inParams.mDeltaTime;

	const char *mode_string = nullptr;
	int mode = int(mTime / 3.0f) % 5;
	switch (mode)
	{
	default:
		mode_string = "Sensor: Collect all contact points";
		mPhysicsSystem->SetSimCollideBodyVsBody(&PhysicsSystem::sDefaultSimCollideBodyVsBody);
		break;

	case 1:
		mode_string = "Sensor: Collect any contact point per body";
		mPhysicsSystem->SetSimCollideBodyVsBody(&sCollideBodyVsBodyPerBody<AnyHitCollisionCollector<CollideShapeCollector>>);
		break;

	case 2:
		mode_string = "Sensor: Collect deepest contact point per body";
		mPhysicsSystem->SetSimCollideBodyVsBody(&sCollideBodyVsBodyPerBody<ClosestHitCollisionCollector<CollideShapeCollector>>);
		break;

	case 3:
		mode_string = "Sensor: Collect any contact point per leaf shape";
		mPhysicsSystem->SetSimCollideBodyVsBody(&sCollideBodyVsBodyPerLeaf<AnyHitCollisionCollector<CollideShapeCollector>>);
		break;

	case 4:
		mode_string = "Sensor: Collect deepest contact point per leaf shape";
		mPhysicsSystem->SetSimCollideBodyVsBody(&sCollideBodyVsBodyPerLeaf<ClosestHitCollisionCollector<CollideShapeCollector>>);
		break;
	}
	DebugRenderer::sInstance->DrawText3D(RVec3(0, 5, 0), mode_string);

	// If the mode changes
	if (mode != mPrevMode)
	{
		// Start all bodies from the top
		for (int i = 0; i < (int)mBodyIDs.size(); ++i)
		{
			BodyID id = mBodyIDs[i];
			mBodyInterface->SetPositionRotationAndVelocity(id, RVec3(-4.9_r + i * 1.0_r, 5.0_r, 0), Quat::sIdentity(), Vec3::sZero(), Vec3::sZero());
			mBodyInterface->ActivateBody(id);
		}

		// Invalidate collisions with sensor to refresh contacts
		mBodyInterface->InvalidateContactCache(mSensorID);

		mPrevMode = mode;
	}
}

void SimCollideBodyVsBodyTest::OnContactAdded(const Body &inBody1, const Body &inBody2, const ContactManifold &inManifold, ContactSettings &ioSettings)
{
	if (!inBody1.IsSensor())
	{
		mDebugRenderer->DrawWirePolygon(RMat44::sTranslation(inManifold.mBaseOffset), inManifold.mRelativeContactPointsOn1, Color::sGreen, 0.01f);
		Vec3 average = Vec3::sZero();
		for (const Vec3 &p : inManifold.mRelativeContactPointsOn1)
			average += p;
		average /= (float)inManifold.mRelativeContactPointsOn1.size();
		mDebugRenderer->DrawArrow(inManifold.mBaseOffset + average, inManifold.mBaseOffset + average - inManifold.mWorldSpaceNormal, Color::sYellow, 0.1f);
	}
	if (!inBody2.IsSensor())
	{
		mDebugRenderer->DrawWirePolygon(RMat44::sTranslation(inManifold.mBaseOffset), inManifold.mRelativeContactPointsOn2, Color::sGreen, 0.01f);
		Vec3 average = Vec3::sZero();
		for (const Vec3 &p : inManifold.mRelativeContactPointsOn2)
			average += p;
		average /= (float)inManifold.mRelativeContactPointsOn2.size();
		mDebugRenderer->DrawArrow(inManifold.mBaseOffset + average, inManifold.mBaseOffset + average + inManifold.mWorldSpaceNormal, Color::sYellow, 0.1f);
	}
}

void SimCollideBodyVsBodyTest::OnContactPersisted(const Body &inBody1, const Body &inBody2, const ContactManifold &inManifold, ContactSettings &ioSettings)
{
	OnContactAdded(inBody1, inBody2, inManifold, ioSettings);
}

void SimCollideBodyVsBodyTest::SaveState(StateRecorder &inStream) const
{
	inStream.Write(mPrevMode);
	inStream.Write(mTime);
}

void SimCollideBodyVsBodyTest::RestoreState(StateRecorder &inStream)
{
	inStream.Read(mPrevMode);
	inStream.Read(mTime);
}
