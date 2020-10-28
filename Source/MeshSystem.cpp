#include "MeshSystem.h"
#include "CollisionObject.h"
#include "Mesh.h"

using namespace MeshSystem;


TemplateNode::TemplateNode(std::string name, const Transform& transform) :
	Name(name),
	TemplateTransform(transform),
	CollisionObjTemplate(nullptr)
{
}

TemplateNode::TemplateNode(const TemplateNode& node, bool copyChildren):
	Name(node.Name),
	TemplateTransform(node.TemplateTransform)
{
	CollisionObjTemplate = (node.CollisionObjTemplate) ? (std::make_unique<CollisionObject>(*node.CollisionObjTemplate)) : (nullptr);

	if (!copyChildren)
		return;

	for (int i = 0; i < node.Children.size(); i++)
	{
		if (MeshNode* meshNodeCast = dynamic_cast<MeshNode*>(node.Children[i].get()))
			Children.push_back(std::make_unique<MeshNode>(MeshNode(*meshNodeCast, true)));
		else
			Children.push_back(std::make_unique<TemplateNode>(TemplateNode(*node.Children[i])));
	}
}

TemplateNode::TemplateNode(TemplateNode&& node):
	Name(node.Name),
	TemplateTransform(node.TemplateTransform),
	Children(std::move(node.Children))
{
	CollisionObjTemplate = std::unique_ptr<CollisionObject>((node.CollisionObjTemplate) ? (node.CollisionObjTemplate.release()) : (nullptr));
}

std::string TemplateNode::GetName() const
{
	return Name;
}

const Transform& TemplateNode::GetTemplateTransform() const
{
	return TemplateTransform;
}

const TemplateNode* TemplateNode::GetChild(int index) const
{
	if (index >= Children.size())
	{
		std::cout << "ERROR! Tried to get child nr " << index << ", but " + Name + " has only " << Children.size() << " children.\n";
		return nullptr;
	}

	return Children[index].get();
}

int TemplateNode::GetChildCount() const
{
	return Children.size();
}

std::unique_ptr<CollisionObject> TemplateNode::InstantiateCollisionObj() const
{
	if (!CollisionObjTemplate)
		return nullptr;

	return std::make_unique<CollisionObject>(CollisionObject(*CollisionObjTemplate));
}

void TemplateNode::SetCollisionObject(std::unique_ptr<CollisionObject>& obj)
{
	obj.swap(CollisionObjTemplate);
}

void TemplateNode::SetTemplateTransform(Transform& t)
{
	TemplateTransform = t;
}

template <typename T> T* TemplateNode::AddChild(std::string name)
{
	Children.push_back(std::make_unique<T>(T(name)));
	return dynamic_cast<T*>(Children.back().get());
}

template<typename T> T* MeshSystem::TemplateNode::AddChild(const T& node)
{
	Children.push_back(std::make_unique<T>(T(node)));
	return dynamic_cast<T*>(Children.back().get());
}

template<typename T> T* MeshSystem::TemplateNode::AddChild(T&& node)
{
	Children.push_back(std::make_unique<T>(T(node)));
	return dynamic_cast<T*>(Children.back().get());
}

TemplateNode* TemplateNode::FindNode(std::string name)
{
	if (Name == name)
		return this;

	for (int i = 0; i < static_cast<int>(Children.size()); i++)
		if (TemplateNode* node = Children[i].get()->FindNode(name))
			return node;

	return nullptr;
}

TemplateNode* TemplateNode::FindNode(int number, int&& currentNumber)
{
	if (number == currentNumber)
		return this;

	for (int i = 0; i < static_cast<int>(Children.size()); i++)
	{
		currentNumber++;
		if (Children[i].get()->FindNode(number, (int&&)currentNumber))
			return Children[i].get();
	}

	return nullptr;
}

Mesh* TemplateNode::FindMesh(std::string name)
{
	for (int i = 0; i < static_cast<int>(Children.size()); i++)
		if (Mesh* found = Children[i].get()->FindMesh(name))
			return found;

	return nullptr;
}

const std::shared_ptr<Mesh> TemplateNode::FindMeshPtr(std::string name)
{
	for (int i = 0; i < static_cast<int>(Children.size()); i++)
		if (std::shared_ptr<Mesh> found = Children[i].get()->FindMeshPtr(name))
			return found;

	return nullptr;
}

Material* TemplateNode::FindMaterial(std::string name)
{
	for (int i = 0; i < static_cast<int>(Children.size()); i++)
		if (Material* found = Children[i].get()->FindMaterial(name))
			return found;

	return nullptr;
}

void TemplateNode::DebugPrint(int nrTabs)
{
	std::string output;
	for (int i = 0; i < nrTabs; i++)
		output += "	";

	std::cout << output + Name + "\n";

	for (int i = 0; i < static_cast<int>(Children.size()); i++)
		Children[i]->DebugPrint(nrTabs + 1);
}

template TemplateNode* TemplateNode::AddChild<TemplateNode>(std::string name);
template MeshNode* TemplateNode::AddChild<MeshNode>(std::string name);
template BoneNode* TemplateNode::AddChild<BoneNode>(std::string name);
template TemplateNode* TemplateNode::AddChild<TemplateNode>(const TemplateNode&);
template MeshNode* TemplateNode::AddChild<MeshNode>(const MeshNode&);
template BoneNode* TemplateNode::AddChild<BoneNode>(const BoneNode&);
template TemplateNode* TemplateNode::AddChild<TemplateNode>(TemplateNode&&);
template MeshNode* TemplateNode::AddChild<MeshNode>(MeshNode&&);
template BoneNode* TemplateNode::AddChild<BoneNode>(BoneNode&&);

///////////////////////////////////

MeshNode::MeshNode(std::string name, std::shared_ptr<Mesh> optionalMesh) :
	TemplateNode(name),
	OverrideMaterial(nullptr)
{
	if (optionalMesh)
		Meshes.push_back(optionalMesh);
}

MeshNode::MeshNode(std::shared_ptr<Mesh> optionalMesh) :
	MeshNode("undefined", optionalMesh)
{
}

MeshNode::MeshNode(const MeshNode& node, bool copyChildren) :
	TemplateNode(node, copyChildren),
	Meshes(node.Meshes),
	OverrideMaterial(node.OverrideMaterial)
{
}

MeshNode::MeshNode(MeshNode&& node) :
	TemplateNode(node),
	Meshes(node.Meshes),
	OverrideMaterial(node.OverrideMaterial)
{
}

int MeshNode::GetMeshCount() const
{
	return Meshes.size();
}

Mesh* MeshNode::GetMesh(int index) const
{
	if (index >= Meshes.size())
		return nullptr;

	return Meshes[index].get();
}

Material* MeshNode::GetOverrideMaterial() const
{
	return OverrideMaterial;
}

Mesh& MeshNode::AddMesh(std::string name)
{
	Meshes.push_back(std::make_shared<Mesh>(Mesh(name)));
	return *Meshes.back();
}

void MeshNode::AddMesh(std::shared_ptr<Mesh> mesh)
{
	Meshes.push_back(mesh);
}

Mesh* MeshNode::FindMesh(std::string name)
{
	auto found = std::find_if(Meshes.begin(), Meshes.end(), [name](const std::shared_ptr<Mesh>& mesh) { return mesh->GetName() == name; });
	if (found != Meshes.end())
		return found->get();

	return TemplateNode::FindMesh(name);
}

const std::shared_ptr<Mesh> MeshNode::FindMeshPtr(std::string name)
{
	auto found = std::find_if(Meshes.begin(), Meshes.end(), [name](const std::shared_ptr<Mesh>& mesh) { return mesh->GetName() == name; });
	if (found != Meshes.end())
		return *found;

	return TemplateNode::FindMeshPtr(name);
}

Material* MeshSystem::MeshNode::FindMaterial(std::string name)
{
	auto found = std::find_if(Meshes.begin(), Meshes.end(), [name](const std::shared_ptr<Mesh>& mesh) { return mesh->GetMaterial()->GetName() == name; });
	if (found != Meshes.end())
		return found->get()->GetMaterial();

	return TemplateNode::FindMaterial(name);
}

void MeshNode::SetOverrideMaterial(Material* material)
{
	OverrideMaterial = material;
}

MeshNode& MeshNode::operator=(const MeshNode& node)
{
	Name = node.Name;
	Meshes.insert(Meshes.end(), node.Meshes.begin(), node.Meshes.end());
	CollisionObjTemplate = (node.CollisionObjTemplate) ? (std::make_unique<CollisionObject>(*node.CollisionObjTemplate)) : (nullptr);
	TemplateTransform = node.TemplateTransform;
	//Children = std::move(node.Children);

	return *this;
}

MeshNode& MeshNode::operator=(MeshNode&& node)
{
	Name = node.Name;
	Meshes.insert(Meshes.end(), node.Meshes.begin(), node.Meshes.end());
	CollisionObjTemplate = (node.CollisionObjTemplate) ? (std::make_unique<CollisionObject>(*node.CollisionObjTemplate)) : (nullptr);
	Children = std::move(node.Children);
	TemplateTransform = node.TemplateTransform;

	return *this;
}

/*
==================================================================================================================================================================
==================================================================================================================================================================
==================================================================================================================================================================
*/

MeshTree::MeshTree(std::string path):
	Root(path),
	Path(path),
	TreeBoneMapping(std::make_unique<BoneMapping>()),
	animation(nullptr)
{
}

MeshTree::MeshTree(const MeshTree& tree, std::string path) :
	Path((path.empty()) ? (tree.Path) : (path)),
	Root(tree.Root, true),
	TreeBoneMapping(std::make_unique<BoneMapping>()),
	animation(nullptr)
{

}

MeshTree::MeshTree(MeshTree&& tree, std::string path) :
	Path((path.empty()) ? (tree.Path) : (path)),
	Root(tree.Root, true),
	TreeBoneMapping(std::make_unique<BoneMapping>()),
	animation(nullptr)
{

}

std::string MeshTree::GetPath() const
{
	return Path;
}

MeshNode& MeshTree::GetRoot()
{
	return Root;
}

BoneMapping* MeshSystem::MeshTree::GetBoneMapping() const
{
	return TreeBoneMapping.get();
}

bool MeshTree::IsEmpty()
{
	return Root.GetChildCount() > 0;
}

void MeshTree::SetPath(std::string path)
{
	Path = path;
}

Mesh* MeshTree::FindMesh(std::string name)
{
	return Root.FindMesh(name);
}

const std::shared_ptr<Mesh> MeshSystem::MeshTree::FindMeshPtr(std::string name)
{
	return Root.FindMeshPtr(name);
}

Material* MeshSystem::MeshTree::FindMaterial(std::string name)
{
	return Root.FindMaterial(name);
}

TemplateNode* MeshTree::FindNode(std::string name)
{
	if (isInteger(name))
		FindNode(std::stoi(name));

	return Root.FindNode(name);
}

TemplateNode* MeshTree::FindNode(int number)
{
	return Root.FindNode(number);
}

bool isInteger(std::string str)
{
	return (!str.empty() && std::find_if(str.begin(), str.end(), [](unsigned char c) { return !std::isdigit(c); }) == str.end());
}

MeshSystem::BoneNode::BoneNode(std::string name, const Transform& transform):
	TemplateNode(name, transform),
	BoneOffset(glm::mat4(1.0f))
{
}

MeshSystem::BoneNode::BoneNode(const BoneNode& node, bool copyChildren):
	TemplateNode(node, copyChildren)
{
}

MeshSystem::BoneNode::BoneNode(BoneNode&& node):
	TemplateNode(node)
{
}

glm::mat4 MeshSystem::BoneNode::GetBoneOffset() const
{
	return BoneOffset;
}

void MeshSystem::BoneNode::SetBoneOffset(const glm::mat4& boneOffset)
{
	BoneOffset = boneOffset;
}

void BoneNode::SetBoneID(unsigned int id)
{
	BoneID = id;
}

void BoneNode::DebugPrint(int nrTabs)
{
	std::string output;
	for (int i = 0; i < nrTabs; i++)
		output += "	";

	std::cout << output + "*" + Name + "\n";

	for (int i = 0; i < static_cast<int>(Children.size()); i++)
		Children[i]->DebugPrint(nrTabs + 1);
}

