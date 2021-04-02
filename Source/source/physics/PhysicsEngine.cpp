#include <physics/PhysicsEngine.h>
#include <array>
#include <rendering/RenderEngine.h>
#include <physics/CollisionObject.h>
#include <rendering/Mesh.h>
#include <math/Transform.h>

physx::PxFoundation* PhysicsEngine::Foundation = nullptr;


using namespace physx;

PhysicsEngine::PhysicsEngine(bool* debugmode):
	Physics(nullptr),
	Dispatcher(nullptr),
	DefaultMaterial(nullptr),
	Pvd(nullptr),
	WasSetup(false)
{
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);

	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)(nullptr));
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)(sizeof(float) * 3));
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);

	DebugModePtr = debugmode;
}

void PhysicsEngine::Init()
{
	if (!Foundation)
		Foundation = PxCreateFoundation(PX_PHYSICS_VERSION, Allocator, ErrorCallback);

	Pvd = PxCreatePvd(*Foundation);

	PxPvdTransport* transport = PxDefaultPvdSocketTransportCreate("localhost", 5425, 1000);
	//PxPvdTransport* transport = PxDefaultPvdFileTransportCreate("plik.pvd");
	Pvd = PxCreatePvd(*Foundation);
	Pvd->connect(*transport, PxPvdInstrumentationFlag::eDEBUG);

	Physics = PxCreatePhysics(PX_PHYSICS_VERSION, *Foundation, PxTolerancesScale(), true, Pvd);

	Cooking = PxCreateCooking(PX_PHYSICS_VERSION, *Foundation, PxCookingParams(PxTolerancesScale()));
	if (!Cooking)
		std::cerr << "ERROR! Can't initialize cooking.\n";

	DefaultMaterial = Physics->createMaterial(0.5f, 1.0f, 0.6f);
}

PxShape* PhysicsEngine::CreateTriangleMeshShape(CollisionShape* colShape, glm::vec3 scale)
{
	if (colShape->VertData.empty() || colShape->IndicesData.empty())
		return nullptr;

	PxTriangleMeshDesc desc;
	desc.points.count = colShape->VertData.size();
	desc.points.stride = sizeof(glm::vec3);
	desc.points.data = &colShape->VertData[0];

	desc.triangles.count = colShape->IndicesData.size() / 3;
	desc.triangles.stride = sizeof(unsigned int) * 3;
	desc.triangles.data = &colShape->IndicesData[0];

	PxDefaultMemoryOutputStream writeBuffer;
	PxTriangleMeshCookingResult::Enum result;
	bool status = Cooking->cookTriangleMesh(desc, writeBuffer, &result);
	if (!status)
	{
		std::cerr << "ERROR! Can't cook mesh with " << desc.points.count << " vertices.\n";
		return nullptr;
	}

	switch (result)
	{
		case PxTriangleMeshCookingResult::Enum::eSUCCESS: break;
		case PxTriangleMeshCookingResult::Enum::eLARGE_TRIANGLE: std::cout << "INFO: Triangles are too large in a cooked mesh!\n"; break;
		case PxTriangleMeshCookingResult::Enum::eFAILURE: std::cout << "ERROR! Can't cook a mesh\n"; return nullptr;
	}

	PxDefaultMemoryInputData readBuffer(writeBuffer.getData(), writeBuffer.getSize());
	PxTriangleMesh* mesh = Physics->createTriangleMesh(readBuffer);
	PxMeshScale meshScale(toPx(scale));

	return Physics->createShape(PxTriangleMeshGeometry(mesh, meshScale, PxMeshGeometryFlag::eDOUBLE_SIDED), *DefaultMaterial);
}

void PhysicsEngine::AddCollisionObjectToPxPipeline(GameScenePhysicsData& scenePhysicsData, CollisionObject& object)
{
	if (object.ActorPtr)
	{
		std::cout << "INFO: The given CollisionObject is already associated with a PxActor object. No PxActor will be created.\n";
		return;
	}
	if (!object.TransformPtr)
	{
		std::cerr << "ERROR! The given CollisionObject does not have a pointer to any Transform object.\n";
		return;
	}
	glm::vec3 worldObjectScale = object.TransformPtr->GetWorldTransform().ScaleRef;

	for (int i = 0; i < static_cast<int>(object.Shapes.size()); i++)
	{
		PxShape* pxShape = nullptr;
		const Transform& shapeT = object.Shapes[i]->ShapeTransform;
		glm::vec3 shapeScale = shapeT.ScaleRef * worldObjectScale;

		switch (object.Shapes[i]->Type)
		{
		case CollisionShapeType::COLLISION_TRIANGLE_MESH:
			pxShape = CreateTriangleMeshShape(object.Shapes[i].get(), shapeScale);
			break;
		case CollisionShapeType::COLLISION_BOX:
			pxShape = Physics->createShape(PxBoxGeometry(toPx(shapeScale * 0.5f)), *DefaultMaterial);
			break;
		case CollisionShapeType::COLLISION_SPHERE:
			pxShape = Physics->createShape(PxSphereGeometry(shapeScale.x), *DefaultMaterial);
			break;
		}

		if (!pxShape)
			continue;

		pxShape->setLocalPose(PxTransform(toPx(object.TransformPtr->GetWorldTransform().ScaleRef * shapeT.PositionRef), (object.IgnoreRotation) ? (physx::PxQuat()) : (toPx(shapeT.RotationRef))));
		if (i == 0)
		{
			object.ActorPtr = (object.IsStatic) ?
				(static_cast<PxRigidActor*>(PxCreateStatic(*Physics, toPx(object.TransformPtr), *pxShape))) :
				(static_cast<PxRigidActor*>(PxCreateDynamic(*Physics, toPx(object.TransformPtr), *pxShape, 10.0f)));
		}
		else
			object.ActorPtr->attachShape(*pxShape);

		pxShape->userData = new glm::vec3(object.Shapes[i]->ShapeTransform.ScaleRef);
		object.TransformDirtyFlag = object.TransformPtr->AddDirtyFlag();
	}

	if (!object.ActorPtr)
	{
		std::cerr << "ERROR! Can't create PxActor.\n";
		return;
	}

	scenePhysicsData.PhysXScene->addActor(*object.ActorPtr);
	//Scene->addActor(*object->ActorPtr);
}

void PhysicsEngine::AddScenePhysicsDataPtr(GameScenePhysicsData& scenePhysicsData)
{
	ScenesPhysicsData.push_back(&scenePhysicsData);
}

PxController* PhysicsEngine::CreateController(GameScenePhysicsData& scenePhysicsData)
{
	PxCapsuleControllerDesc desc;
	desc.radius = 0.12f;
	desc.height = 0.6f;
	desc.material = DefaultMaterial;
	desc.position = PxExtendedVec3(0.0f, 0.0f, 0.0f);
	desc.position = PxExtendedVec3(-3.0f, 1.0f, 8.0f);
	//desc.maxJumpHeight = 0.5f;
	//desc.invisibleWallHeight = 0.5f;
	desc.contactOffset = desc.radius * 0.1f;
	desc.stepOffset = 0.01f;
	//desc.slopeLimit = glm::cos(glm::radians(90.0f));

	PxController* controller = scenePhysicsData.PhysXControllerManager->createController(desc);

	PxRigidDynamic* actor = controller->getActor();
	PxShape* shapes[1]; //There is only one shape in this controller
	actor->getShapes(shapes, 1, 0); //get that shape
	PxShape* shape = shapes[0];
	//shape->setLocalPose(physx::PxTransform(physx::PxVec3(0.0f), physx::PxQuat(physx::PxHalfPi, physx::PxVec3(0.0f, 0.0f, 1.0f))));	//rotate it so we get a vertical capsule (without this line the capsule would be oriented towards X+, I guess that's how physX defaults it)
	return controller;
}

void PhysicsEngine::ApplyForce(CollisionObject& obj, glm::vec3 force)
{
	PxRigidDynamic* body = obj.ActorPtr->is<PxRigidDynamic>();

	if (body)
		body->addForce(toPx(force), PxForceMode::eIMPULSE);
}


void PhysicsEngine::SetupScene(GameScenePhysicsData& scenePhysicsData)
{
	if (!Physics)
		return;
	PxSceneDesc sceneDesc(Physics->getTolerancesScale());
	Dispatcher = PxDefaultCpuDispatcherCreate(2);

	sceneDesc.gravity = PxVec3(0.0f, -9.81f, 0.0f);
	sceneDesc.cpuDispatcher = Dispatcher;
	sceneDesc.filterShader = PxDefaultSimulationFilterShader;
	scenePhysicsData.PhysXScene = Physics->createScene(sceneDesc);

	PxPvdSceneClient* pvdClient = scenePhysicsData.PhysXScene->getScenePvdClient();
	if (pvdClient)
	{
		std::cout << "Pvd successful.\n";
		pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS, true);
		pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONTACTS, true);
		pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES, true);
	}

	scenePhysicsData.PhysXControllerManager = PxCreateControllerManager(*scenePhysicsData.PhysXScene);

	PxRigidStatic* ground = PxCreatePlane(*Physics, PxPlane(0.0f, 1.0f, 0.0f, 0.5f), *DefaultMaterial);
	scenePhysicsData.PhysXScene->addActor(*ground);

	if (*DebugModePtr)
	{
		scenePhysicsData.PhysXScene->setVisualizationParameter(PxVisualizationParameter::eSCALE, 0.3f);
		scenePhysicsData.PhysXScene->setVisualizationParameter(PxVisualizationParameter::eACTOR_AXES, 1.0f);
		scenePhysicsData.PhysXScene->setVisualizationParameter(PxVisualizationParameter::eCOLLISION_SHAPES, 1.0f);
	}

	scenePhysicsData.WasSetup = true;

	for (int i = 0; i < static_cast<int>(scenePhysicsData.CollisionObjects.size()); i++)
		//if (!CollisionObjects[i]->ActorPtr)
			AddCollisionObjectToPxPipeline(scenePhysicsData, *scenePhysicsData.CollisionObjects[i]);

	AddScenePhysicsDataPtr(scenePhysicsData);
}

void PhysicsEngine::Update(float deltaTime)
{
	UpdatePxTransforms();

	for (int i = 0; i < static_cast<int>(ScenesPhysicsData.size()); i++)
	{
		ScenesPhysicsData[i]->PhysXScene->simulate(deltaTime);
		ScenesPhysicsData[i]->PhysXScene->fetchResults(true);
	}

	UpdateTransforms();
}

void PhysicsEngine::UpdateTransforms()
{
	for (int sceneIndex = 0; sceneIndex < static_cast<int>(ScenesPhysicsData.size()); sceneIndex++)
	{
		for (int i = 0; i < static_cast<int>(ScenesPhysicsData[sceneIndex]->CollisionObjects.size()); i++)
		{
			CollisionObject* obj = ScenesPhysicsData[sceneIndex]->CollisionObjects[i];
			if (!obj->ActorPtr || !obj->TransformPtr)
				continue;

			PxTransform& pxTransform = obj->ActorPtr->getGlobalPose();
			obj->TransformPtr->SetPositionWorld(toGlm(pxTransform.p));
			if (!obj->IgnoreRotation)
				obj->TransformPtr->SetRotationWorld(toGlm(pxTransform.q));

			//obj->TransformPtr->SetMatrix(t.Matrix);

		}
	}
}

void PhysicsEngine::UpdatePxTransforms()
{
	for (int sceneIndex = 0; sceneIndex < static_cast<int>(ScenesPhysicsData.size()); sceneIndex++)
	{
		for (int i = 0; i < static_cast<int>(ScenesPhysicsData[sceneIndex]->CollisionObjects.size()); i++)
		{
			CollisionObject* obj = ScenesPhysicsData[sceneIndex]->CollisionObjects[i];
			if (!obj->ActorPtr || !obj->TransformPtr || !obj->TransformPtr->GetDirtyFlag(obj->TransformDirtyFlag))
				continue;

			const Transform& worldTransform = obj->TransformPtr->GetWorldTransform();

			PxTransform pxTransform = obj->ActorPtr->getGlobalPose();
			pxTransform.p = toPx(worldTransform.PositionRef);
			if (!obj->IgnoreRotation)
				pxTransform.q = toPx(worldTransform.RotationRef);

			
			physx::PxShape** shapes = new physx::PxShape * [obj->ActorPtr->getNbShapes()];
			obj->ActorPtr->getShapes(shapes, obj->ActorPtr->getNbShapes());

			for (int j = 0; j < obj->ActorPtr->getNbShapes(); j++)
			{
				if (!shapes[j])
					continue;

				glm::vec3 shapeScale(1.0f);
				float* scaleData = (float*)shapes[j]->userData;
				if (scaleData)
					for (int i = 0; i < 3; i++)
						shapeScale[i] = scaleData[i];
				shapeScale = glm::max(shapeScale, glm::vec3(0.001f));

				switch (shapes[j]->getGeometryType())
				{
				case PxGeometryType::eTRIANGLEMESH:
				{
					physx::PxTriangleMeshGeometry meshGeom;
					shapes[j]->getTriangleMeshGeometry(meshGeom);
					meshGeom.scale = toPx(worldTransform.ScaleRef * shapeScale);

					obj->ActorPtr->detachShape(*shapes[j]);
					shapes[j]->setGeometry(meshGeom);
					break;
				}
				case PxGeometryType::eBOX:
				{
					physx::PxBoxGeometry boxGeom;
					shapes[j]->getBoxGeometry(boxGeom);
					boxGeom.halfExtents = toPx(worldTransform.ScaleRef * shapeScale * 0.5f);

					obj->ActorPtr->detachShape(*shapes[j]);
					shapes[j]->setGeometry(boxGeom);
					break;
				}
				case PxGeometryType::eSPHERE:
				{
					physx::PxSphereGeometry sphereGeom;
					shapes[j]->getSphereGeometry(sphereGeom);
					sphereGeom.radius = worldTransform.ScaleRef.x * shapeScale.x;

					obj->ActorPtr->detachShape(*shapes[j]);
					shapes[j]->setGeometry(sphereGeom);
					break;
				}
				case PxGeometryType::eCAPSULE:
					continue;
				default:
					std::cout << "Geometry type " << shapes[j]->getGeometryType() << " not supported.\n";
					continue; //skip iteration
					obj->ActorPtr->detachShape(*shapes[j]);
				}

				obj->ActorPtr->attachShape(*shapes[j]);
			}

			obj->ActorPtr->setGlobalPose(pxTransform);
		}
	}
}

void PhysicsEngine::DebugRender(GameScenePhysicsData& scenePhysicsData, RenderEngine& renderEng, RenderInfo& info)
{
	if (!(*DebugModePtr))
	{
		std::cerr << "ERROR! Physics debug render called, but DebugMode is false.\n";
		return;
	}
	
	const PxRenderBuffer& rb = scenePhysicsData.PhysXScene->getRenderBuffer();


	std::vector<std::array<glm::vec3, 2>> verts;
	int sizeSum = rb.getNbPoints() + rb.getNbLines() * 2 + rb.getNbTriangles() * 3;
	int v = 0;
	verts.resize(sizeSum);

	for (int i = 0; i < static_cast<int>(rb.getNbPoints()); i++)
	{
		const PxDebugPoint& point = rb.getPoints()[i];

		verts[v][0] = toGlm(point.pos);
		verts[v++][1] = toVecColor(static_cast<PxDebugColor::Enum>(point.color));
	}
	

	for (int i = 0; i < static_cast<int>(rb.getNbLines()); i++)
	{
		const PxDebugLine& line = rb.getLines()[i];

		verts[v][0] = toGlm(line.pos0);
		verts[v++][1] = toVecColor(static_cast<PxDebugColor::Enum>(line.color0));
		verts[v][0] = toGlm(line.pos1);
		verts[v++][1] = toVecColor(static_cast<PxDebugColor::Enum>(line.color1));
	}

	for (int i = 0; i < static_cast<int>(rb.getNbTriangles()); i++)
	{
		const PxDebugTriangle& triangle = rb.getTriangles()[i];

		verts[v][0] = toGlm(triangle.pos0);
		verts[v++][1] = toVecColor(static_cast<PxDebugColor::Enum>(triangle.color0));
		verts[v][0] = toGlm(triangle.pos1);
		verts[v++][1] = toVecColor(static_cast<PxDebugColor::Enum>(triangle.color1));
		verts[v][0] = toGlm(triangle.pos2);
		verts[v++][1] = toVecColor(static_cast<PxDebugColor::Enum>(triangle.color2));
	}


	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * 2 * verts.size(), &verts[0][0], GL_STATIC_DRAW);
	
	renderEng.RenderBoundInDebug(info, GL_POINTS, 0, rb.getNbPoints(), glm::vec3(0.0f));
	renderEng.RenderBoundInDebug(info, GL_LINES, rb.getNbPoints(), rb.getNbLines() * 2, glm::vec3(0.0f));
	renderEng.RenderBoundInDebug(info, GL_TRIANGLES, rb.getNbPoints() + rb.getNbLines() * 2, rb.getNbTriangles() * 3, glm::vec3(0.0f));
}

PhysicsEngine::~PhysicsEngine()
{
	for (int i = 0; i < static_cast<int>(ScenesPhysicsData.size()); i++)
		ScenesPhysicsData[i]->PhysXScene->release();

	Dispatcher->release();
	Physics->release();
	Cooking->release();

	if (Pvd)
	{
		PxPvdTransport* transport = Pvd->getTransport();
		Pvd->release();
		Pvd = nullptr;
		if (transport)
			transport->release();
	}

	//Foundation->release();
	std::cout << "Physics engine successfully destroyed!\n";
}


glm::vec3 toVecColor(PxDebugColor::Enum col)
{
	switch (col)
	{
	case PxDebugColor::eARGB_BLACK: return glm::vec3(0.0f);
	case PxDebugColor::eARGB_BLUE: return glm::vec3(0.0f, 0.0f, 1.0f);
	case PxDebugColor::eARGB_CYAN: return glm::vec3(0.5f, 0.5f, 1.0f);
	case PxDebugColor::eARGB_DARKBLUE: return glm::vec3(0.0f, 0.0f, 0.75f);
	case PxDebugColor::eARGB_DARKGREEN: return glm::vec3(0.0f, 0.75f, 0.0f);
	case PxDebugColor::eARGB_DARKRED: return glm::vec3(0.75f, 0.0f, 0.0f);
	case PxDebugColor::eARGB_GREEN: return glm::vec3(0.0f, 1.0f, 0.0f);
	case PxDebugColor::eARGB_GREY: return glm::vec3(0.5f, 0.5f, 0.5f);
	case PxDebugColor::eARGB_MAGENTA : return glm::vec3(1.0f, 0.0f, 1.0f);
	case PxDebugColor::eARGB_RED: return glm::vec3(1.0f, 0.0f, 0.0f);
	case PxDebugColor::eARGB_WHITE: return glm::vec3(1.0f);
	case PxDebugColor::eARGB_YELLOW: return glm::vec3(1.0f, 1.0f, 0.0f);
	}

	return glm::vec3(0.0f);
}