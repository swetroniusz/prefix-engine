#include "Shader.h"

unsigned int Shader::LoadShader(GLenum type, std::string shaderPath, std::string additionalData)
{
	additionalData = "#version 400 core\n" + additionalData;
	std::fstream shaderFile(shaderPath);

	if (!shaderFile.good())
	{
		std::cout << "ERROR! Cannot open: " << shaderPath << " :(\n";
		return -1;
	}

	std::stringstream shaderStream;
	shaderStream << shaderFile.rdbuf();

	std::string shaderSourceStr = additionalData + shaderStream.str();

	const char* shaderSource = shaderSourceStr.c_str();

	unsigned int shader = glCreateShader(type);
	glShaderSource(shader, 1, &shaderSource, 0);
	glCompileShader(shader);

	DebugShader(shader);

	return shader;
}

void Shader::DebugShader(unsigned int shader)
{
	int result;
	char data[512];
	glGetShaderiv(shader, GL_COMPILE_STATUS, &result);
	if (!result)
	{
		glGetShaderInfoLog(shader, 512, NULL, data);
		std::cout << "Shader error: " << data;
	}
}

GLint Shader::FindLocation(std::string name) const
{
	auto loc = std::find_if(Locations.begin(), Locations.end(), [name](const UniformLocation& location) { return location.Name == name; });

	if (loc != Locations.end())
		return loc->Location;

	Locations.push_back(UniformLocation(name, glGetUniformLocation(Program, name.c_str())));
	return Locations.back().Location;
}

/*
==================================================================
==================================================================
==================================================================
*/

Shader::Shader():
	Program(0),
	Name("Internal"),
	ExpectedMatrices{ 0 }
{
}

Shader::Shader( std::string vShaderPath, std::string fShaderPath, std::string gShaderPath):
	Name("Internal")
{
	LoadShaders(vShaderPath, fShaderPath, gShaderPath);
}

std::string Shader::GetName()
{
	return Name;
}

std::vector<std::pair<unsigned int, std::string>>* Shader::GetMaterialTextureUnits()
{
	return &MaterialTextureUnits;
}

bool Shader::ExpectsMatrix(unsigned int index)
{
	return ExpectedMatrices[index];
}

void Shader::SetName(std::string name)
{
	Name = name;
}

void Shader::SetTextureUnitNames(std::vector <std::pair<unsigned int, std::string>> materialTexUnits)
{
	MaterialTextureUnits = materialTexUnits;
	for (unsigned int i = 0; i < MaterialTextureUnits.size(); i++)
		Uniform1i("material." + MaterialTextureUnits[i].second, MaterialTextureUnits[i].first);
}

void Shader::AddTextureUnit(unsigned int unitIndex, std::string nameInShader)
{
	MaterialTextureUnits.push_back(std::pair<unsigned int, std::string>(unitIndex, nameInShader));
	Uniform1i("material." + nameInShader, unitIndex);
}

void Shader::SetExpectedMatrices(std::vector <MatrixType> matrices)
{
	for (unsigned int i = 0; i < matrices.size(); i++)
		ExpectedMatrices[(unsigned int)matrices[i]] = true;
}

void Shader::AddExpectedMatrix(std::string matType)
{
	if (matType == "M")
		ExpectedMatrices[MatrixType::MODEL] = true;
	else if (matType == "V")
		ExpectedMatrices[MatrixType::VIEW] = true;
	else if (matType == "P")
		ExpectedMatrices[MatrixType::PROJECTION] = true;
	else if (matType == "MV")
		ExpectedMatrices[MatrixType::MV] = true;
	else if (matType == "VP")
		ExpectedMatrices[MatrixType::VP] = true;
	else if (matType == "MVP")
		ExpectedMatrices[MatrixType::MVP] = true;
	else if (matType == "NORMAL")
		ExpectedMatrices[MatrixType::NORMAL] = true;
	else
		std::cerr << "ERROR! Can't find matrix type " << matType << '\n';
}

void Shader::LoadShaders(std::string vShaderPath, std::string fShaderPath, std::string gShaderPath)
{
	LoadShadersWithInclData("", vShaderPath, fShaderPath, gShaderPath);
}

void Shader::LoadShadersWithInclData(std::string data, std::string vShaderPath, std::string fShaderPath, std::string gShaderPath)
{
	LoadShadersWithExclData(data, vShaderPath, data, fShaderPath, (gShaderPath.empty()) ? (std::string()) : (data), gShaderPath);
}

void Shader::LoadShadersWithExclData(std::string vShaderData, std::string vShaderPath, std::string fShaderData, std::string fShaderPath, std::string gShaderData, std::string gShaderPath)
{
	unsigned int nrShaders = (gShaderPath.empty()) ? (2) : (3);
	std::vector<unsigned int> shaders(nrShaders);
	shaders[0] = LoadShader(GL_VERTEX_SHADER, vShaderPath, vShaderData);
	shaders[1] = LoadShader(GL_FRAGMENT_SHADER, fShaderPath, fShaderData);
	if (nrShaders == 3) shaders[2] = LoadShader(GL_GEOMETRY_SHADER, gShaderPath, gShaderData);

	Program = glCreateProgram();
	for (unsigned int i = 0; i < nrShaders; i++)
		glAttachShader(Program, shaders[i]);
	glLinkProgram(Program);
	for (unsigned int i = 0; i < nrShaders; i++)
	{
		glDetachShader(Program, shaders[i]);
		glDeleteShader(shaders[i]);
	}
}

void Shader::Uniform1i(std::string name, int val) const
{
	glUniform1i(FindLocation(name), val);
}

void Shader::Uniform1f(std::string name, float val) const
{
	glUniform1f(FindLocation(name), val);
}

void Shader::Uniform2fv(std::string name, glm::vec2 val) const
{
	glUniform2fv(FindLocation(name), 1, glm::value_ptr(val));
}

void Shader::Uniform3fv(std::string name, glm::vec3 val) const
{
	glUniform3fv(FindLocation(name), 1, glm::value_ptr(val));
}

void Shader::Uniform4fv(std::string name, glm::vec4 val) const
{
	glUniform4fv(FindLocation(name), 1, glm::value_ptr(val));
}

void Shader::UniformMatrix3fv(std::string name, glm::mat3 val) const
{
	glUniformMatrix3fv(FindLocation(name), 1, GL_FALSE, glm::value_ptr(val));
}

void Shader::UniformMatrix4fv(std::string name, const glm::mat4& val) const
{
	glUniformMatrix4fv(FindLocation(name), 1, GL_FALSE, glm::value_ptr(val));
}

void Shader::UniformBlockBinding(std::string name, unsigned int binding) const
{
	glUniformBlockBinding(Program, GetUniformBlockIndex(name), binding);
}

void Shader::UniformBlockBinding(unsigned int blockID, unsigned int binding) const
{
	glUniformBlockBinding(Program, blockID, binding);
}

unsigned int Shader::GetUniformBlockIndex(std::string name) const
{
	return glGetUniformBlockIndex(Program, name.c_str());
}

void Shader::Use() const
{
	glUseProgram(Program);
}